
#line 1 "modXML.rl"
// regenerate (the sed step sections the state tables into flash for ESP8266; the
// manifest's strings-in-flash-force-l32 recipe makes the byte reads from flash safe):
//
//	ragel -T0 modXML.rl -o modXML.c
//	sed -i "" -E "s/^(static const [a-z ]+ _[A-Za-z0-9_]+\[\]) = \{/\1 ICACHE_XS6RO_ATTR = {/" modXML.c
/*
 * Copyright (c) 2016-2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK Runtime.
 *
 *   The Moddable SDK Runtime is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   The Moddable SDK Runtime is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public License
 *   along with the Moddable SDK Runtime.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/*
	The parser runs in place over the JavaScript input string. Chunk memory moves when
	the garbage collector compacts, so every pointer into the input is rebased against
	xsArg(0) after any action that can allocate, and text runs are tracked as offsets.
	New strings are materialized by allocating first (xsStringBuffer with NULL), then
	re-fetching the source pointer to copy — never by passing a chunk pointer to an
	allocating call. XML line-ending normalization (CR LF and CR become LF) happens
	during those copies, in the parse direction only.

	ESP8266 cannot read bytes directly from flash-resident strings, and the generated
	state machine dereferences the input directly, so that platform alone parses a
	RAM copy of the input.

	Leniency: the parser accepts anything well-formed and never fails on a valid
	document. Numeric character references outside U+0001..U+10FFFF (or in the
	surrogate range) are replaced by U+FFFD rather than overflowing or throwing.
	DOCTYPE declarations are skipped. Non-ASCII bytes are accepted in names.

	Nesting: the Ragel call stack is fixed at XML_STACK_SIZE (one entry per open
	element) and guarded by the prepush action, which throws "stack overflow". 32 is
	ample for WebDAV/CalDAV (depth 5-9) and typical config documents; raise to 64
	(another 128 bytes of C stack) for deep XHTML-like content by setting
	MODDEF_XML_STACK_SIZE from the manifest. A dynamically grown stack (realloc in
	prepush, freed in xsCatch) is possible but deliberately not used, to avoid heap
	allocation inside the parse loop on constrained devices.

	Changes (2026-08-28): comment, CDATA and processing instruction bodies may contain
	the first characters of their terminator (":>>" finish-guarded concatenation; a
	lone "-" in a comment or "]" in CDATA used to fail the parse); leading whitespace
	and a UTF-8 BOM before the root are accepted; bounded numeric character
	references (U+FFFD on invalid, fixes signed overflow); prepush stack-overflow
	guard; DOCTYPE skipped; non-ASCII bytes accepted in names; XML.search (xml.js)
	matches local names without prefix.

	Contributed by Mark Wharton.
*/

#include "xsmc.h"
#include "xsHost.h"
#include "mc.xs.h" // for xsID_ values

#if defined(__ets__) && !ESP32
	#define mxXMLCopyInput 1
#else
	#define mxXMLCopyInput 0
#endif

static const char gHexa[] ICACHE_XS6RO2_ATTR = "0123456789ABCDEF";
#define escapeHexa(X) (char)c_read8(gHexa + ((X) & 15))
#define unescapeHexa(X) \
	((('0' <= (X)) && ((X) <= '9')) \
		? ((X) - '0') \
		: ((('a' <= (X)) && ((X) <= 'f')) \
			? (10 + (X) - 'a') \
			: (10 + (X) - 'A')))

// Ragel call stack: one entry per open element (see header note). Override from a
// manifest with "defines": { "xml": { "stack_size": 64 } }.
#ifdef MODDEF_XML_STACK_SIZE
	#define XML_STACK_SIZE MODDEF_XML_STACK_SIZE
#else
	#define XML_STACK_SIZE 32
#endif
#define XML_MAX_CODEPOINT 0x10FFFF
#define XML_REPLACEMENT_CHARACTER 0xFFFD

typedef struct {
	struct FSMParserStruct {
		xsStringValue p;
		xsStringValue pe;
		xsStringValue eof;
		xsIntegerValue cs;
		xsIntegerValue stack[XML_STACK_SIZE];
		xsIntegerValue top;
	} fsm;
	xsStringValue base;
	xsStringValue copy;
	xsIntegerValue compact;
	xsIntegerValue textOffset;
	xsIntegerValue textLength;
} XMLParserRecord, *XMLParser;

typedef struct {
	struct FSMTextScannerStruct {
		xsStringValue p;
		xsStringValue pe;
		xsStringValue eof;
		xsStringValue ts;
		xsStringValue te;
		xsIntegerValue act;
		xsIntegerValue cs;
	} fsm;
	xsStringValue base;
	xsStringValue copy;
	xsIntegerValue mode;
} XMLTextScannerRecord, *XMLTextScanner;

static void xml_text_scanner(xsMachine* the, xsIntegerValue offset, xsIntegerValue length, xsIntegerValue mode, xsStringValue copy);

// shared

static xsIntegerValue xml_copy(xsStringValue dst, xsStringValue src, xsIntegerValue length, xsIntegerValue normalize)
{
	xsStringValue start = dst;
	xsIntegerValue i = 0;
	while (i < length) {
		char c = c_read8(src + i);
		i++;
		if (normalize && (c == '\r')) {
			if ((i < length) && (c_read8(src + i) == '\n'))
				i++;
			c = '\n';
		}
		*dst++ = c;
	}
	*dst = 0;
	return (xsIntegerValue)(dst - start);
}

static xsIntegerValue xml_array_length(xsMachine* the, xsSlot* array)
{
	xsmcGet(xsVar(4), *array, xsID_length);
	return xsmcToInteger(xsVar(4));
}

static void xml_array_push_property(xsMachine* the, xsSlot* container, xsUnsignedValue id, xsSlot* value)
{
	xsIntegerValue length;
	if (!xsmcGet(xsVar(5), *container, id)) {
		xsmcSetNewArray(xsVar(5), 0);
		xsmcSet(*container, id, xsVar(5));
	}
	length = xml_array_length(the, &xsVar(5));
	xsmcSetInteger(xsVar(4), length);
	xsmcSetAt(xsVar(5), xsVar(4), *value);
}

// xml parser

static xsStringValue xml_parser_base(xsMachine* the, XMLParser parser)
{
	return parser->copy ? parser->copy : xsmcToString(xsArg(0));
}

static void xml_parser_rebase(xsMachine* the, XMLParser parser)
{
	xsStringValue base;
	ptrdiff_t delta;
	if (parser->copy)
		return;
	base = xsmcToString(xsArg(0));
	delta = base - parser->base;
	if (delta) {
		parser->fsm.p += delta;
		parser->fsm.pe += delta;
		parser->fsm.eof += delta;
		parser->base = base;
	}
}

static void xml_parser_start_text(xsMachine* the, XMLParser parser, xsIntegerValue offset)
{
	parser->textOffset = (xsIntegerValue)(parser->fsm.p - parser->base) + offset;
}

static void xml_parser_stop_text(xsMachine* the, XMLParser parser, xsIntegerValue offset)
{
	parser->textLength = (xsIntegerValue)(parser->fsm.p - parser->base) + offset - parser->textOffset;
}

static void xml_parser_xsVar2_text_helper(xsMachine* the, XMLParser parser, xsIntegerValue mode)
{
	if (mode) {
		xsVar(3) = xsResult;
		xml_text_scanner(the, parser->textOffset, parser->textLength, mode, parser->copy);
		xsVar(2) = xsResult;
		xsResult = xsVar(3);
	}
	else {
		xsStringValue src, dst;
		xsmcSetStringBuffer(xsVar(2), NULL, parser->textLength);
		src = xml_parser_base(the, parser) + parser->textOffset;
		dst = xsmcToString(xsVar(2));
		xml_copy(dst, src, parser->textLength, 1);
	}
	xml_parser_rebase(the, parser);
}

static void xml_parser_assign_value(xsMachine* the, XMLParser parser)
{
	xml_parser_xsVar2_text_helper(the, parser, 3);
	xsmcSet(xsVar(1), xsID_value, xsVar(2));
	xml_parser_rebase(the, parser);
}

static void xml_parser_attribute(xsMachine* the, XMLParser parser)
{
	xml_parser_xsVar2_text_helper(the, parser, 0);
	xsmcSetNewObject(xsVar(1));
	xsmcSet(xsVar(1), xsID_name, xsVar(2));
	xml_array_push_property(the, &xsResult, xsID_attributes, &xsVar(1));
	xml_parser_rebase(the, parser);
}

static void xml_parser_comment(xsMachine* the, XMLParser parser)
{
	// not processed
}

static void xml_parser_element(xsMachine* the, XMLParser parser)
{
	xsIntegerValue length = xml_array_length(the, &xsVar(0));
	xsmcSetNewObject(xsResult);
	xml_parser_xsVar2_text_helper(the, parser, 0);
	xsmcSet(xsResult, xsID_name, xsVar(2));
	if (length > 0) {
		xsmcSetInteger(xsVar(5), length - 1);
		xsmcGetAt(xsVar(1), xsVar(0), xsVar(5));
		xml_array_push_property(the, &xsVar(1), xsID_elements, &xsResult);
	}
	xml_parser_rebase(the, parser);
}

static void xml_parser_enter_element(xsMachine* the, XMLParser parser)
{
	xsIntegerValue length = xml_array_length(the, &xsVar(0));
	xsmcSetInteger(xsVar(5), length);
	xsmcSetAt(xsVar(0), xsVar(5), xsResult);
	xml_parser_rebase(the, parser);
}

static void xml_parser_exit_element(xsMachine* the, XMLParser parser)
{
	xsIntegerValue length = xml_array_length(the, &xsVar(0));
	xsmcSetInteger(xsVar(5), length - 1);
	xsmcGetAt(xsResult, xsVar(0), xsVar(5));
	xsmcSet(xsVar(0), xsID_length, xsVar(5));
	xml_parser_xsVar2_text_helper(the, parser, 0);
	xsmcGet(xsVar(1), xsResult, xsID_name);
	if (c_strcmp(xsmcToString(xsVar(1)), xsmcToString(xsVar(2))))
		xsUnknownError("xml_parser: tag mismatch");
	if (parser->compact) { // compact text trim / delete
		if (xsmcGet(xsVar(2), xsResult, xsID_text)) {
			xsmcCall(xsVar(2), xsVar(2), xsID_trim, NULL);
			if (xsmcTest(xsVar(2)))
				xsmcSet(xsResult, xsID_text, xsVar(2));
			else
				xsmcDelete(xsResult, xsID_text);
		}
	}
	xml_parser_rebase(the, parser);
}

static void xml_parser_processing_instruction(xsMachine* the, XMLParser parser)
{
	// not processed
}

static void xml_parser_text(xsMachine* the, XMLParser parser, xsIntegerValue normal)
{
	xsIntegerValue length = xml_array_length(the, &xsVar(0));
	if (0 == length)
		xsUnknownError("xml_parser: text outside element");
	xsmcSetInteger(xsVar(5), length - 1);
	xsmcGetAt(xsVar(1), xsVar(0), xsVar(5));
	xml_parser_xsVar2_text_helper(the, parser, normal ? 3 : 0);
	if (parser->compact) { // compact text concat / merge CDATA section text
		if (xsmcGet(xsVar(3), xsVar(1), xsID_text))
			xsmcCall(xsVar(2), xsVar(3), xsID_concat, &xsVar(2), NULL);
		xsmcSet(xsVar(1), xsID_text, xsVar(2));
	}
	else {
		xsmcSetNewObject(xsVar(3));
		xsmcSet(xsVar(3), xsID_text, xsVar(2));
		xml_array_push_property(the, &xsVar(1), xsID_elements, &xsVar(3));
	}
	xml_parser_rebase(the, parser);
}

static void xml_parser_cdata_section(xsMachine* the, XMLParser parser)
{
	xml_parser_text(the, parser, 0);
}

// xml parser specification


#line 409 "modXML.rl"



#line 318 "modXML.c"
static const char _parser_actions[] ICACHE_XS6RO_ATTR = {
	0, 1, 8, 1, 9, 1, 10, 2, 
	5, 14, 2, 6, 16, 2, 10, 1, 
	2, 10, 4, 2, 11, 0, 2, 11, 
	7, 2, 12, 2, 2, 12, 3, 3, 
	10, 6, 16, 3, 10, 13, 15, 4, 
	10, 1, 5, 14, 4, 10, 4, 5, 
	14, 4, 11, 0, 5, 14
};

static const unsigned char _parser_key_offsets[] ICACHE_XS6RO_ATTR = {
	0, 0, 5, 6, 7, 11, 20, 36, 
	50, 67, 68, 70, 73, 78, 79, 82, 
	83, 85, 86, 87, 88, 89, 91, 92, 
	93, 94, 95, 96, 97, 99, 100, 104, 
	105, 106, 108, 109, 110, 120, 136, 150, 
	167, 168, 170, 173, 178, 179, 182, 183, 
	185, 186, 187, 188, 189, 191, 192, 193, 
	194, 195, 196, 197, 198, 199, 200, 202, 
	209, 225, 229, 230, 231, 233, 236
};

static const char _parser_trans_keys[] ICACHE_XS6RO_ATTR = {
	-17, 32, 60, 9, 13, -69, -65, 32, 
	60, 9, 13, 33, 63, 96, 0, 64, 
	91, 94, 123, 127, 32, 47, 62, 96, 
	0, 8, 9, 13, 14, 44, 59, 64, 
	91, 94, 123, 127, 32, 47, 62, 96, 
	0, 8, 9, 13, 14, 64, 91, 94, 
	123, 127, 32, 47, 61, 62, 96, 0, 
	8, 9, 13, 14, 44, 59, 64, 91, 
	94, 123, 127, 62, 34, 39, 34, 38, 
	60, 32, 47, 62, 9, 13, 59, 38, 
	39, 60, 59, 45, 68, 45, 45, 45, 
	45, 45, 62, 79, 67, 84, 89, 80, 
	69, 62, 91, 93, 32, 62, 9, 13, 
	63, 63, 62, 63, 60, 60, 33, 47, 
	63, 96, 0, 64, 91, 94, 123, 127, 
	32, 47, 62, 96, 0, 8, 9, 13, 
	14, 44, 59, 64, 91, 94, 123, 127, 
	32, 47, 62, 96, 0, 8, 9, 13, 
	14, 64, 91, 94, 123, 127, 32, 47, 
	61, 62, 96, 0, 8, 9, 13, 14, 
	44, 59, 64, 91, 94, 123, 127, 62, 
	34, 39, 34, 38, 60, 32, 47, 62, 
	9, 13, 59, 38, 39, 60, 59, 45, 
	91, 45, 45, 45, 45, 45, 62, 67, 
	68, 65, 84, 65, 91, 93, 93, 93, 
	62, 93, 96, 0, 64, 91, 94, 123, 
	127, 32, 47, 62, 96, 0, 8, 9, 
	13, 14, 44, 59, 64, 91, 94, 123, 
	127, 32, 62, 9, 13, 63, 63, 62, 
	63, 32, 9, 13, 0
};

static const char _parser_single_lengths[] ICACHE_XS6RO_ATTR = {
	0, 3, 1, 1, 2, 3, 4, 4, 
	5, 1, 2, 3, 3, 1, 3, 1, 
	2, 1, 1, 1, 1, 2, 1, 1, 
	1, 1, 1, 1, 2, 1, 2, 1, 
	1, 2, 1, 1, 4, 4, 4, 5, 
	1, 2, 3, 3, 1, 3, 1, 2, 
	1, 1, 1, 1, 2, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 2, 1, 
	4, 2, 1, 1, 2, 1, 0
};

static const char _parser_range_lengths[] ICACHE_XS6RO_ATTR = {
	0, 1, 0, 0, 1, 3, 6, 5, 
	6, 0, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 1, 0, 
	0, 0, 0, 0, 3, 6, 5, 6, 
	0, 0, 0, 1, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 3, 
	6, 1, 0, 0, 0, 1, 0
};

static const short _parser_index_offsets[] ICACHE_XS6RO_ATTR = {
	0, 0, 5, 7, 9, 13, 20, 31, 
	41, 53, 55, 58, 62, 67, 69, 73, 
	75, 78, 80, 82, 84, 86, 89, 91, 
	93, 95, 97, 99, 101, 104, 106, 110, 
	112, 114, 117, 119, 121, 129, 140, 150, 
	162, 164, 167, 171, 176, 178, 182, 184, 
	187, 189, 191, 193, 195, 198, 200, 202, 
	204, 206, 208, 210, 212, 214, 216, 219, 
	224, 235, 239, 241, 243, 246, 249
};

static const char _parser_indicies[] ICACHE_XS6RO_ATTR = {
	0, 2, 3, 2, 1, 4, 1, 2, 
	1, 2, 3, 2, 1, 6, 7, 1, 
	1, 1, 1, 5, 9, 10, 11, 1, 
	1, 9, 1, 1, 1, 1, 8, 13, 
	14, 15, 1, 1, 13, 1, 1, 1, 
	12, 17, 18, 19, 20, 1, 1, 17, 
	1, 1, 1, 1, 16, 21, 1, 22, 
	23, 1, 25, 26, 1, 24, 27, 28, 
	29, 27, 1, 24, 26, 31, 25, 1, 
	30, 30, 31, 32, 33, 1, 34, 1, 
	36, 35, 38, 37, 39, 37, 39, 40, 
	37, 41, 1, 42, 1, 43, 1, 44, 
	1, 45, 1, 46, 1, 2, 47, 46, 
	48, 47, 48, 2, 48, 1, 50, 49, 
	52, 51, 53, 52, 51, 55, 54, 57, 
	56, 59, 60, 61, 1, 1, 1, 1, 
	58, 63, 64, 65, 1, 1, 63, 1, 
	1, 1, 1, 62, 67, 68, 69, 1, 
	1, 67, 1, 1, 1, 66, 71, 72, 
	73, 74, 1, 1, 71, 1, 1, 1, 
	1, 70, 75, 1, 76, 77, 1, 79, 
	80, 1, 78, 81, 82, 83, 81, 1, 
	78, 80, 85, 79, 1, 84, 84, 85, 
	86, 87, 1, 88, 1, 90, 89, 92, 
	91, 93, 91, 93, 94, 91, 95, 1, 
	96, 1, 97, 1, 98, 1, 99, 1, 
	100, 1, 102, 101, 104, 103, 105, 103, 
	106, 105, 103, 1, 1, 1, 1, 107, 
	109, 1, 110, 1, 1, 109, 1, 1, 
	1, 1, 108, 111, 112, 111, 1, 114, 
	113, 116, 115, 117, 116, 115, 21, 21, 
	1, 1, 0
};

static const char _parser_trans_targs[] ICACHE_XS6RO_ATTR = {
	2, 0, 4, 5, 3, 6, 16, 31, 
	6, 7, 9, 69, 8, 7, 9, 69, 
	8, 7, 9, 10, 69, 69, 11, 14, 
	11, 12, 13, 7, 9, 69, 14, 15, 
	17, 22, 18, 19, 20, 19, 20, 21, 
	4, 23, 24, 25, 26, 27, 28, 29, 
	30, 32, 33, 32, 33, 4, 35, 36, 
	35, 34, 37, 47, 63, 66, 37, 38, 
	40, 34, 39, 38, 40, 34, 39, 38, 
	40, 41, 34, 34, 42, 45, 42, 43, 
	44, 38, 40, 34, 45, 46, 48, 53, 
	49, 50, 51, 50, 51, 52, 34, 54, 
	55, 56, 57, 58, 59, 60, 61, 60, 
	61, 62, 34, 64, 64, 65, 70, 65, 
	70, 67, 68, 67, 68, 34
};

static const char _parser_trans_actions[] ICACHE_XS6RO_ATTR = {
	0, 0, 0, 0, 0, 1, 0, 0, 
	0, 16, 16, 44, 1, 0, 0, 7, 
	0, 13, 13, 13, 39, 0, 3, 3, 
	0, 0, 0, 19, 19, 49, 0, 0, 
	0, 0, 0, 1, 1, 0, 0, 0, 
	28, 0, 0, 0, 0, 0, 0, 0, 
	0, 1, 1, 0, 0, 22, 1, 0, 
	0, 35, 1, 0, 0, 0, 0, 16, 
	16, 44, 1, 0, 0, 7, 0, 13, 
	13, 13, 39, 0, 3, 3, 0, 0, 
	0, 19, 19, 49, 0, 0, 0, 0, 
	0, 1, 1, 0, 0, 0, 28, 0, 
	0, 0, 0, 0, 0, 1, 1, 0, 
	0, 0, 25, 1, 0, 5, 31, 0, 
	10, 1, 1, 0, 0, 22
};

static const int parser_start = 1;
static const int parser_first_final = 69;
static const int parser_error = 0;

static const int parser_en_inner = 34;
static const int parser_en_main = 1;


#line 412 "modXML.rl"

#pragma unused (parser_en_inner)
#pragma unused (parser_en_main)
#pragma unused (parser_error)

void xs_xml_parse(xsMachine* the)
{
	XMLParserRecord parserRecord;
	XMLParser parser = &parserRecord;
	xsIntegerValue length;

	xsmcVars(6);
	parser->compact = (xsmcArgc > 1) ? xsmcToBoolean(xsArg(1)) : 1;
	parser->textOffset = 0;
	parser->textLength = 0;
	parser->copy = NULL;
	xsmcSetNewArray(xsVar(0), 0);
	parser->base = xsmcToString(xsArg(0));
	length = (xsIntegerValue)c_strlen(parser->base);
#if mxXMLCopyInput
	parser->copy = c_malloc(length + 1);
	if (!parser->copy)
		xsUnknownError("not enough memory");
	c_memcpy(parser->copy, parser->base, length + 1);
	parser->base = parser->copy;
	xsTry {
#endif
	parser->fsm.p = parser->base;
	parser->fsm.pe = parser->fsm.p + length;
	parser->fsm.eof = parser->fsm.pe;
	
#line 521 "modXML.c"
	{
	 parser->fsm.cs = parser_start;
	 parser->fsm.top = 0;
	}

#line 443 "modXML.rl"
	
#line 529 "modXML.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( ( parser->fsm.p) == ( parser->fsm.pe) )
		goto _test_eof;
	if (  parser->fsm.cs == 0 )
		goto _out;
_resume:
	_keys = _parser_trans_keys + _parser_key_offsets[ parser->fsm.cs];
	_trans = _parser_index_offsets[ parser->fsm.cs];

	_klen = _parser_single_lengths[ parser->fsm.cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*( parser->fsm.p)) < *_mid )
				_upper = _mid - 1;
			else if ( (*( parser->fsm.p)) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _parser_range_lengths[ parser->fsm.cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*( parser->fsm.p)) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*( parser->fsm.p)) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _parser_indicies[_trans];
	 parser->fsm.cs = _parser_trans_targs[_trans];

	if ( _parser_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _parser_actions + _parser_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 322 "modXML.rl"
	{
		xml_parser_assign_value(the, parser);
	}
	break;
	case 1:
#line 326 "modXML.rl"
	{
		xml_parser_attribute(the, parser);
	}
	break;
	case 2:
#line 330 "modXML.rl"
	{
		xml_parser_cdata_section(the, parser);
	}
	break;
	case 3:
#line 334 "modXML.rl"
	{
		xml_parser_comment(the, parser);
	}
	break;
	case 4:
#line 338 "modXML.rl"
	{
		xml_parser_element(the, parser);
	}
	break;
	case 5:
#line 342 "modXML.rl"
	{
		xml_parser_enter_element(the, parser);
	}
	break;
	case 6:
#line 346 "modXML.rl"
	{
		xml_parser_exit_element(the, parser);
	}
	break;
	case 7:
#line 350 "modXML.rl"
	{
		xml_parser_processing_instruction(the, parser);
	}
	break;
	case 8:
#line 354 "modXML.rl"
	{
		xml_parser_start_text(the, parser, 0);
	}
	break;
	case 9:
#line 358 "modXML.rl"
	{
		xml_parser_start_text(the, parser, 1);
	}
	break;
	case 10:
#line 362 "modXML.rl"
	{
		xml_parser_stop_text(the, parser, 0);
	}
	break;
	case 11:
#line 366 "modXML.rl"
	{
		xml_parser_stop_text(the, parser, -1);
	}
	break;
	case 12:
#line 370 "modXML.rl"
	{
		xml_parser_stop_text(the, parser, -2);
	}
	break;
	case 13:
#line 374 "modXML.rl"
	{
		xml_parser_text(the, parser, 1);
	}
	break;
	case 14:
#line 394 "modXML.rl"
	{ {
		if (parser->fsm.top == XML_STACK_SIZE)
			xsUnknownError("xml_parser: stack overflow");
	{ parser->fsm.stack[ parser->fsm.top++] =  parser->fsm.cs;  parser->fsm.cs = 34;goto _again;}} }
	break;
	case 15:
#line 400 "modXML.rl"
	{ ( parser->fsm.p)--; }
	break;
	case 16:
#line 404 "modXML.rl"
	{ { parser->fsm.cs =  parser->fsm.stack[-- parser->fsm.top]; goto _again;} }
	break;
#line 702 "modXML.c"
		}
	}

_again:
	if (  parser->fsm.cs == 0 )
		goto _out;
	if ( ++( parser->fsm.p) != ( parser->fsm.pe) )
		goto _resume;
	_test_eof: {}
	_out: {}
	}

#line 444 "modXML.rl"
#if mxXMLCopyInput
	}
	xsCatch {
		c_free(parser->copy);
		xsThrow(xsException);
	}
	c_free(parser->copy);
	parser->copy = NULL;
#endif
	if (parser->fsm.cs < parser_first_final)
		xsUnknownError("xml_parser: error");
}

// xml text scanner

static xsStringValue xml_text_scanner_base(xsMachine* the, XMLTextScanner scanner)
{
	return scanner->copy ? scanner->copy : xsmcToString(xsArg(0));
}

static void xml_text_scanner_rebase(xsMachine* the, XMLTextScanner scanner)
{
	xsStringValue base;
	ptrdiff_t delta;
	if (scanner->copy)
		return;
	base = xsmcToString(xsArg(0));
	delta = base - scanner->base;
	if (delta) {
		scanner->fsm.p += delta;
		scanner->fsm.pe += delta;
		scanner->fsm.eof += delta;
		if (scanner->fsm.ts)
			scanner->fsm.ts += delta;
		if (scanner->fsm.te)
			scanner->fsm.te += delta;
		scanner->base = base;
	}
}

static void xml_text_scanner_append(xsMachine* the, XMLTextScanner scanner, xsSlot* slot)
{
	if (xsmcTest(xsResult))
		xsmcCall(xsResult, xsResult, xsID_concat, slot, NULL);
	else
		xsResult = *slot;
	xml_text_scanner_rebase(the, scanner);
}

static void xml_text_scanner_append_static(xsMachine* the, XMLTextScanner scanner, const char* text)
{
	xsmcSetString(xsVar(4), (xsStringValue)text);
	xml_text_scanner_append(the, scanner, &xsVar(4));
}

static void xml_text_scanner_append_slice(xsMachine* the, XMLTextScanner scanner, xsStringValue ts, xsStringValue te)
{
	xsIntegerValue offset = (xsIntegerValue)(ts - scanner->base);
	xsIntegerValue length = (xsIntegerValue)(te - ts);
	xsStringValue src, dst;
	xsmcSetStringBuffer(xsVar(4), NULL, length);
	src = xml_text_scanner_base(the, scanner) + offset;
	dst = xsmcToString(xsVar(4));
	xml_copy(dst, src, length, (scanner->mode == 3) ? 1 : 0);
	xml_text_scanner_append(the, scanner, &xsVar(4));
}

static void xml_text_scanner_append_codepoint(xsMachine* the, XMLTextScanner scanner, xsIntegerValue codepoint)
{
	xsmcGet(xsVar(4), xsGlobal, xsID_String);
	xsmcSetInteger(xsVar(5), codepoint);
	xsmcCall(xsVar(4), xsVar(4), xsID_fromCodePoint, &xsVar(5), NULL);
	xml_text_scanner_append(the, scanner, &xsVar(4));
}

static void xml_text_scanner_append_codepoint_reference(xsMachine* the, XMLTextScanner scanner, xsStringValue ts, xsStringValue te)
{
	// '&#' codepoint ';'
	xsIntegerValue offset = (xsIntegerValue)(ts - scanner->base);
	xsIntegerValue length = (xsIntegerValue)(te - ts);
	xsStringValue base = xml_text_scanner_base(the, scanner);
	xsIntegerValue codepoint = 0;
	xsIntegerValue i = 2;
	char c = c_read8(base + offset + i);
	// stop accumulating once out of range: the value cannot become valid again and
	// this keeps the intermediate below 0x10FFFF * 16 + 15, so no integer overflow
	if ((c == 'X') || (c == 'x')) {
		i++;
		while ((i < length - 1) && (codepoint <= XML_MAX_CODEPOINT)) {
			codepoint = (codepoint * 0x10) + unescapeHexa(c_read8(base + offset + i));
			i++;
		}
	}
	else {
		while ((i < length - 1) && (codepoint <= XML_MAX_CODEPOINT)) {
			codepoint = (codepoint * 10) + (c_read8(base + offset + i) - '0');
			i++;
		}
	}
	if ((codepoint == 0) || (codepoint > XML_MAX_CODEPOINT) || ((codepoint >= 0xD800) && (codepoint <= 0xDFFF)))
		codepoint = XML_REPLACEMENT_CHARACTER; // invalid reference: substitute rather than fail
	xml_text_scanner_append_codepoint(the, scanner, codepoint);
}

static void xml_text_scanner_append_character_reference(xsMachine* the, XMLTextScanner scanner, xsStringValue ts, xsStringValue te)
{
	xsIntegerValue offset = (xsIntegerValue)(ts - scanner->base);
	xsIntegerValue length = (xsIntegerValue)(te - ts);
	xsIntegerValue i;
	char reference[7] = "&#x??;";
	for (i = 0; i < length; i++) {
		char c = c_read8(xml_text_scanner_base(the, scanner) + offset + i);
		reference[3] = escapeHexa(c >> 4);
		reference[4] = escapeHexa(c);
		xml_text_scanner_append_static(the, scanner, reference);
	}
}

// xml text scanner specification


#line 666 "modXML.rl"



#line 841 "modXML.c"
static const char _scanner_actions[] ICACHE_XS6RO_ATTR = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11, 1, 12, 1, 13, 1, 14, 1, 
	15, 1, 16, 1, 17, 1, 18, 1, 
	19, 1, 20, 1, 21, 1, 22
};

static const char _scanner_key_offsets[] ICACHE_XS6RO_ATTR = {
	0, 4, 7, 13, 20, 22, 23, 24, 
	25, 26, 27, 28, 29, 30, 31, 32, 
	33, 34, 35, 36, 37, 38, 43, 53, 
	63, 68, 69, 70
};

static const char _scanner_trans_keys[] ICACHE_XS6RO_ATTR = {
	88, 120, 48, 57, 59, 48, 57, 48, 
	57, 65, 70, 97, 102, 59, 48, 57, 
	65, 70, 97, 102, 109, 112, 112, 59, 
	111, 115, 59, 116, 59, 116, 59, 117, 
	111, 116, 59, 62, 38, 38, 35, 97, 
	103, 108, 113, 34, 38, 39, 60, 62, 
	127, 1, 8, 14, 31, 34, 60, 62, 
	127, 1, 8, 14, 31, 38, 39, 127, 
	1, 8, 14, 31, 93, 93, 93, 0
};

static const char _scanner_single_lengths[] ICACHE_XS6RO_ATTR = {
	2, 1, 0, 1, 2, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 1, 5, 6, 4, 
	1, 1, 1, 1
};

static const char _scanner_range_lengths[] ICACHE_XS6RO_ATTR = {
	1, 1, 3, 3, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 2, 3, 
	2, 0, 0, 0
};

static const char _scanner_index_offsets[] ICACHE_XS6RO_ATTR = {
	0, 4, 7, 11, 16, 19, 21, 23, 
	25, 27, 29, 31, 33, 35, 37, 39, 
	41, 43, 45, 47, 49, 51, 57, 66, 
	74, 78, 80, 82
};

static const char _scanner_trans_targs[] ICACHE_XS6RO_ATTR = {
	2, 2, 1, 19, 19, 1, 19, 3, 
	3, 3, 19, 19, 3, 3, 3, 19, 
	5, 7, 19, 6, 19, 19, 19, 8, 
	19, 9, 19, 19, 19, 11, 19, 19, 
	19, 13, 19, 19, 19, 15, 19, 16, 
	19, 17, 19, 19, 19, 25, 25, 21, 
	20, 19, 20, 0, 4, 10, 12, 14, 
	19, 22, 22, 22, 22, 22, 24, 24, 
	24, 23, 22, 22, 22, 22, 22, 22, 
	22, 23, 24, 24, 24, 22, 27, 26, 
	25, 26, 18, 25, 19, 19, 19, 19, 
	19, 19, 19, 19, 19, 19, 19, 19, 
	19, 19, 19, 19, 19, 19, 25, 19, 
	19, 22, 22, 25, 25, 0
};

static const char _scanner_trans_actions[] ICACHE_XS6RO_ATTR = {
	0, 0, 0, 45, 29, 0, 45, 0, 
	0, 0, 45, 29, 0, 0, 0, 45, 
	0, 0, 45, 0, 45, 35, 45, 0, 
	45, 0, 45, 31, 45, 0, 45, 39, 
	45, 0, 45, 37, 45, 0, 45, 0, 
	45, 0, 45, 33, 45, 21, 27, 5, 
	0, 43, 0, 0, 0, 0, 0, 0, 
	41, 9, 11, 7, 13, 15, 0, 0, 
	0, 0, 19, 19, 19, 19, 19, 19, 
	19, 0, 0, 0, 0, 17, 5, 0, 
	25, 0, 0, 23, 45, 45, 45, 45, 
	45, 45, 45, 45, 45, 45, 45, 45, 
	45, 45, 45, 45, 45, 45, 27, 43, 
	41, 19, 17, 25, 23, 0
};

static const char _scanner_to_state_actions[] ICACHE_XS6RO_ATTR = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 1, 0, 
	0, 1, 0, 0
};

static const char _scanner_from_state_actions[] ICACHE_XS6RO_ATTR = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 3, 0, 0, 3, 0, 
	0, 3, 0, 0
};

static const char _scanner_eof_trans[] ICACHE_XS6RO_ATTR = {
	102, 102, 102, 102, 102, 102, 102, 102, 
	102, 102, 102, 102, 102, 102, 102, 102, 
	102, 102, 103, 0, 104, 105, 0, 106, 
	107, 0, 108, 109
};

static const int scanner_start = 19;
static const int scanner_first_final = 19;
static const int scanner_error = -1;

static const int scanner_en_escape = 22;
static const int scanner_en_escape_cdata_section = 25;
static const int scanner_en_unescape = 19;


#line 669 "modXML.rl"

#pragma unused (scanner_error)
#pragma unused (scanner_start)

static void xml_text_scanner(xsMachine* the, xsIntegerValue offset, xsIntegerValue length, xsIntegerValue mode, xsStringValue copy)
{
	XMLTextScannerRecord scannerRecord;
	XMLTextScanner scanner = &scannerRecord;
	xsmcSetString(xsResult, "");
	scanner->mode = mode;
	scanner->copy = copy;
	if (mode == 3)
		scanner->fsm.cs = scanner_en_unescape;
	else if (mode == 2)
		scanner->fsm.cs = scanner_en_escape_cdata_section;
	else
		scanner->fsm.cs = scanner_en_escape; // attribute value (1), normal text (0)
	scanner->base = copy ? copy : xsmcToString(xsArg(0));
	scanner->fsm.p = scanner->base + offset;
	scanner->fsm.pe = scanner->fsm.p + length;
	scanner->fsm.eof = scanner->fsm.pe;
	
#line 978 "modXML.c"
	{
	( scanner->fsm.ts) = 0;
	( scanner->fsm.te) = 0;
	 scanner->fsm.act = 0;
	}

#line 691 "modXML.rl"
	
#line 987 "modXML.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( ( scanner->fsm.p) == ( scanner->fsm.pe) )
		goto _test_eof;
_resume:
	_acts = _scanner_actions + _scanner_from_state_actions[ scanner->fsm.cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 1:
#line 1 "NONE"
	{( scanner->fsm.ts) = ( scanner->fsm.p);}
	break;
#line 1006 "modXML.c"
		}
	}

	_keys = _scanner_trans_keys + _scanner_key_offsets[ scanner->fsm.cs];
	_trans = _scanner_index_offsets[ scanner->fsm.cs];

	_klen = _scanner_single_lengths[ scanner->fsm.cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*( scanner->fsm.p)) < *_mid )
				_upper = _mid - 1;
			else if ( (*( scanner->fsm.p)) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _scanner_range_lengths[ scanner->fsm.cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*( scanner->fsm.p)) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*( scanner->fsm.p)) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
_eof_trans:
	 scanner->fsm.cs = _scanner_trans_targs[_trans];

	if ( _scanner_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _scanner_actions + _scanner_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 2:
#line 1 "NONE"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;}
	break;
	case 3:
#line 609 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		if (scanner->mode)
			xml_text_scanner_append_static(the, scanner, "&apos;");
		else
			xml_text_scanner_append_static(the, scanner, "'");
	}}
	break;
	case 4:
#line 624 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		if (scanner->mode)
			xml_text_scanner_append_static(the, scanner, "&quot;");
		else
			xml_text_scanner_append_static(the, scanner, "\"");
	}}
	break;
	case 5:
#line 605 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_static(the, scanner, "&amp;");
	}}
	break;
	case 6:
#line 616 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_static(the, scanner, "&lt;");
	}}
	break;
	case 7:
#line 620 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_static(the, scanner, "&gt;");
	}}
	break;
	case 8:
#line 601 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p);( scanner->fsm.p)--;{
		xml_text_scanner_append_character_reference(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
	case 9:
#line 631 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p);( scanner->fsm.p)--;{
		xml_text_scanner_append_slice(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
	case 10:
#line 573 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_static(the, scanner, "]]]]><![CDATA[>"); // or throw like KPR/Expat?
	}}
	break;
	case 11:
#line 631 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p);( scanner->fsm.p)--;{
		xml_text_scanner_append_slice(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
	case 12:
#line 631 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p);( scanner->fsm.p)--;{
		xml_text_scanner_append_slice(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
	case 13:
#line 631 "modXML.rl"
	{{( scanner->fsm.p) = ((( scanner->fsm.te)))-1;}{
		xml_text_scanner_append_slice(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
	case 14:
#line 577 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_codepoint_reference(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
	case 15:
#line 589 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_codepoint(the, scanner, 0x00027); // '
	}}
	break;
	case 16:
#line 581 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_codepoint(the, scanner, 0x00022); // "
	}}
	break;
	case 17:
#line 585 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_codepoint(the, scanner, 0x00026); // &
	}}
	break;
	case 18:
#line 593 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_codepoint(the, scanner, 0x0003C); // <
	}}
	break;
	case 19:
#line 597 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_codepoint(the, scanner, 0x0003E); // >
	}}
	break;
	case 20:
#line 631 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p);( scanner->fsm.p)--;{
		xml_text_scanner_append_slice(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
	case 21:
#line 631 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p);( scanner->fsm.p)--;{
		xml_text_scanner_append_slice(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
	case 22:
#line 631 "modXML.rl"
	{{( scanner->fsm.p) = ((( scanner->fsm.te)))-1;}{
		xml_text_scanner_append_slice(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
#line 1201 "modXML.c"
		}
	}

_again:
	_acts = _scanner_actions + _scanner_to_state_actions[ scanner->fsm.cs];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 ) {
		switch ( *_acts++ ) {
	case 0:
#line 1 "NONE"
	{( scanner->fsm.ts) = 0;}
	break;
#line 1214 "modXML.c"
		}
	}

	if ( ++( scanner->fsm.p) != ( scanner->fsm.pe) )
		goto _resume;
	_test_eof: {}
	if ( ( scanner->fsm.p) == ( scanner->fsm.eof) )
	{
	if ( _scanner_eof_trans[ scanner->fsm.cs] > 0 ) {
		_trans = _scanner_eof_trans[ scanner->fsm.cs] - 1;
		goto _eof_trans;
	}
	}

	}

#line 692 "modXML.rl"
	if (scanner->fsm.cs < scanner_first_final)
		xsUnknownError("xml_text_scanner: error");
}

void xs_xml_escape(xsMachine* the)
{
	xsIntegerValue mode = (xsmcArgc > 1) ? xsmcToInteger(xsArg(1)) : 0;
	xsIntegerValue length;
	xsStringValue copy = NULL;

	xsmcVars(6);
	length = (xsIntegerValue)c_strlen(xsmcToString(xsArg(0)));
#if mxXMLCopyInput
	copy = c_malloc(length + 1);
	if (!copy)
		xsUnknownError("not enough memory");
	c_memcpy(copy, xsmcToString(xsArg(0)), length + 1);
	xsTry {
#endif
	xml_text_scanner(the, 0, length, mode, copy);
#if mxXMLCopyInput
	}
	xsCatch {
		c_free(copy);
		xsThrow(xsException);
	}
	c_free(copy);
#endif
}
