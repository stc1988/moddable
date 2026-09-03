
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

static const char gHexa[] ICACHE_XS6RO2_ATTR = "0123456789ABCDEF";
#define escapeHexa(X) (char)c_read8(gHexa + ((X) & 15))

static xsIntegerValue xml_hex_value(unsigned char c)
{
	if (('0' <= c) && (c <= '9'))
		return c - '0';
	if (('a' <= c) && (c <= 'f'))
		return 10 + c - 'a';
	return 10 + c - 'A';
}

// Ragel call stack: one entry per open element (see header note). Override from a
// manifest with "defines": { "xml": { "stack_size": 64 } }.
#ifdef MODDEF_XML_STACK_SIZE
	#define XML_STACK_SIZE MODDEF_XML_STACK_SIZE
#else
	#define XML_STACK_SIZE 32
#endif
#define XML_MAX_CODEPOINT 0x10FFFF
#define XML_REPLACEMENT_CHARACTER 0xFFFD

// XS slots every native reserves; the helpers use xsVar(4) and xsVar(5) as scratch
#define XML_VARS 6

// escape contexts: xml_writer_escape, and the XML.escape kinds in the same order
#define XML_ESCAPE_TEXT 0
#define XML_ESCAPE_ATTRIBUTE 1
#define XML_ESCAPE_CDATA 2

// text copy-out: as written (names, CDATA) or with references decoded (text, attribute values)
#define XML_TEXT_VERBATIM 0
#define XML_TEXT_UNESCAPE 1

// Serializer output chunk. Override from a manifest with
// "defines": { "xml": { "writer_chunk": 1024 } }.
#ifdef MODDEF_XML_WRITER_CHUNK
	#define XML_WRITER_CHUNK MODDEF_XML_WRITER_CHUNK
#else
	#define XML_WRITER_CHUNK 512
#endif

// The input: xsArg(0) as bytes. A string is always relocatable (chunk heap). A buffer is
// relocatable unless it is a host buffer; parsing works in place either way. transcode is set by
// the pre-scan when slices need repair (invalid UTF-8 becomes U+FFFD) or, on mxCESU8 builds,
// supplementary-plane re-encoding; clean BMP-only input copies raw.
typedef struct {
	xsIntegerValue buffer;
	xsIntegerValue relocatable;
	xsIntegerValue transcode;
} XMLInputRecord, *XMLInput;

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
	XMLInputRecord input;
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
	XMLInputRecord input;
} XMLTextScannerRecord, *XMLTextScanner;

static void xml_text_scanner(xsMachine* the, xsIntegerValue offset, xsIntegerValue length, XMLInput input);

// shared

static xsStringValue xml_input_base(xsMachine* the, XMLInput input)
{
	if (input->buffer) {
		void* data;
		xsUnsignedValue count;
		xsmcGetBufferReadable(xsArg(0), &data, &count);
		return (xsStringValue)data;
	}
	return xsmcToString(xsArg(0));
}

static unsigned char xml_normalize(xsStringValue src, xsIntegerValue* i, xsIntegerValue length, xsIntegerValue normalize);

// One routine, three uses: scan the whole input (dst NULL, flags out), measure a slice
// (dst NULL), write a slice. Invalid sequences follow the maximal-subpart rule (one U+FFFD
// per maximal invalid subpart, as TextDecoder): strict UTF-8 — overlongs, surrogates,
// F5..FF, lone or missing continuations, and NUL (not a legal XML character) all repair.
// Valid supplementary-plane sequences pass through, re-encoded as surrogate pairs on
// mxCESU8 builds. Slices never split a sequence: slice boundaries are ASCII delimiters and
// every byte of a multi-byte sequence is above 0x7F.
static xsIntegerValue xml_transcode(xsStringValue dst, xsStringValue src, xsIntegerValue length, xsIntegerValue normalize, xsIntegerValue* anyInvalid, xsIntegerValue* anySupplementary)
{
	xsStringValue start = dst;
	xsIntegerValue i = 0, out = 0;
	while (i < length) {
		unsigned char c = (unsigned char)c_read8(src + i);
		xsIntegerValue remain = length - i;
		xsIntegerValue take = 1, valid = 0, supplementary = 0;
		if (c < 0x80) {
			if (c == 0)
				take = 1; // NUL: repair
			else {
				c = xml_normalize(src, &i, length, normalize);
				if (dst)
					*dst++ = (char)c;
				out++;
				continue;
			}
		}
		else if ((c >= 0xC2) && (c <= 0xDF)) {
			if ((remain >= 2) && ((c_read8(src + i + 1) & 0xC0) == 0x80)) { take = 2; valid = 1; }
		}
		else if ((c >= 0xE0) && (c <= 0xEF)) {
			unsigned char lo = 0x80, hi = 0xBF, b1;
			if (c == 0xE0) lo = 0xA0;			// overlong
			else if (c == 0xED) hi = 0x9F;		// surrogates
			b1 = (remain >= 2) ? (unsigned char)c_read8(src + i + 1) : 0;
			if ((remain >= 2) && (b1 >= lo) && (b1 <= hi)) {
				if ((remain >= 3) && ((c_read8(src + i + 2) & 0xC0) == 0x80)) { take = 3; valid = 1; }
				else take = 2;
			}
		}
		else if ((c >= 0xF0) && (c <= 0xF4)) {
			unsigned char lo = 0x80, hi = 0xBF, b1;
			if (c == 0xF0) lo = 0x90;			// overlong
			else if (c == 0xF4) hi = 0x8F;		// above U+10FFFF
			b1 = (remain >= 2) ? (unsigned char)c_read8(src + i + 1) : 0;
			if ((remain >= 2) && (b1 >= lo) && (b1 <= hi)) {
				if ((remain >= 3) && ((c_read8(src + i + 2) & 0xC0) == 0x80)) {
					if ((remain >= 4) && ((c_read8(src + i + 3) & 0xC0) == 0x80)) { take = 4; valid = 1; supplementary = 1; }
					else take = 3;
				}
				else take = 2;
			}
		}
		// else: 0x80..0xC1 continuation or overlong lead, 0xF5..0xFF: take = 1, invalid
		if (valid) {
#if mxCESU8
			if (supplementary) {
				// re-encode as a surrogate pair, the engine's internal form
				xsIntegerValue codepoint = ((c & 0x07) << 18)
					| ((c_read8(src + i + 1) & 0x3F) << 12)
					| ((c_read8(src + i + 2) & 0x3F) << 6)
					| (c_read8(src + i + 3) & 0x3F);
				xsIntegerValue high = 0xD800 | ((codepoint - 0x10000) >> 10);
				xsIntegerValue low = 0xDC00 | ((codepoint - 0x10000) & 0x3FF);
				if (dst) {
					*dst++ = (char)(0xE0 | (high >> 12));
					*dst++ = (char)(0x80 | ((high >> 6) & 0x3F));
					*dst++ = (char)(0x80 | (high & 0x3F));
					*dst++ = (char)(0xE0 | (low >> 12));
					*dst++ = (char)(0x80 | ((low >> 6) & 0x3F));
					*dst++ = (char)(0x80 | (low & 0x3F));
				}
				out += 6;
				i += take;
				continue;
			}
#endif
			if (supplementary && anySupplementary)
				*anySupplementary = 1;
			if (dst) {
				xsIntegerValue j;
				for (j = 0; j < take; j++)
					*dst++ = c_read8(src + i + j);
			}
			out += take;
			i += take;
			continue;
		}
		if (anyInvalid)
			*anyInvalid = 1;
		if (dst) {	// U+FFFD as UTF-8
			*dst++ = (char)(0xE0 | (XML_REPLACEMENT_CHARACTER >> 12));
			*dst++ = (char)(0x80 | ((XML_REPLACEMENT_CHARACTER >> 6) & 0x3F));
			*dst++ = (char)(0x80 | (XML_REPLACEMENT_CHARACTER & 0x3F));
		}
		out += 3;
		i += take;
	}
	if (dst)
		*dst = 0;
	return dst ? (xsIntegerValue)(dst - start) : out;
}

// CR and CR LF become LF; advances *i past the sequence and returns the byte to emit
static unsigned char xml_normalize(xsStringValue src, xsIntegerValue* i, xsIntegerValue length, xsIntegerValue normalize)
{
	unsigned char c = (unsigned char)c_read8(src + *i);
	(*i)++;
	if (normalize && (c == '\r')) {
		if ((*i < length) && (c_read8(src + *i) == '\n'))
			(*i)++;
		c = '\n';
	}
	return c;
}

// the raw copy for clean input. String input always takes this path: XS strings may hold the
// engine's internal encodings (CESU-8 surrogate pairs, C0 80 for NUL), which the strict
// transcoder would "repair"; only scanned buffer input routes through xml_transcode.
static xsIntegerValue xml_copy(xsStringValue dst, xsStringValue src, xsIntegerValue length, xsIntegerValue normalize)
{
	xsStringValue start = dst;
	xsIntegerValue i = 0;
	while (i < length)
		*dst++ = (char)xml_normalize(src, &i, length, normalize);
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

// how far the input moved since *base was taken; updates *base. Zero when the input
// cannot move or did not.
static ptrdiff_t xml_input_delta(xsMachine* the, XMLInput input, xsStringValue* base)
{
	xsStringValue current;
	ptrdiff_t delta;
	if (!input->relocatable)
		return 0;
	current = xml_input_base(the, input);
	delta = current - *base;
	*base = current;
	return delta;
}

static void xml_parser_rebase(xsMachine* the, XMLParser parser)
{
	ptrdiff_t delta = xml_input_delta(the, &parser->input, &parser->base);
	if (delta) {
		parser->fsm.p += delta;
		parser->fsm.pe += delta;
		parser->fsm.eof += delta;
	}
}

// measure (when transcoding), allocate, re-fetch the base, copy: the one slice copy-out
static void xml_slice_to_slot(xsMachine* the, XMLInput input, xsSlot* slot, xsIntegerValue offset, xsIntegerValue length, xsIntegerValue normalize)
{
	xsStringValue src, dst;
	xsIntegerValue need = length;
	if (input->transcode)
		need = xml_transcode(NULL, xml_input_base(the, input) + offset, length, normalize, NULL, NULL);
	xsmcSetStringBuffer(*slot, NULL, need);
	src = xml_input_base(the, input) + offset;
	dst = xsmcToString(*slot);
	if (input->transcode)
		xml_transcode(dst, src, length, normalize, NULL, NULL);
	else
		xml_copy(dst, src, length, normalize);
}

static void xml_parser_start_text(xsMachine* the, XMLParser parser, xsIntegerValue offset)
{
	parser->textOffset = (xsIntegerValue)(parser->fsm.p - parser->base) + offset;
}

static void xml_parser_stop_text(xsMachine* the, XMLParser parser, xsIntegerValue offset)
{
	parser->textLength = (xsIntegerValue)(parser->fsm.p - parser->base) + offset - parser->textOffset;
}

// the current text run into xsVar(2): as written, or with references decoded
static void xml_parser_text_value(xsMachine* the, XMLParser parser, xsIntegerValue unescape)
{
	if (unescape) {
		xsVar(3) = xsResult;
		xml_text_scanner(the, parser->textOffset, parser->textLength, &parser->input);
		xsVar(2) = xsResult;
		xsResult = xsVar(3);
	}
	else
		xml_slice_to_slot(the, &parser->input, &xsVar(2), parser->textOffset, parser->textLength, 1);
	xml_parser_rebase(the, parser);
}

static void xml_parser_assign_value(xsMachine* the, XMLParser parser)
{
	xml_parser_text_value(the, parser, XML_TEXT_UNESCAPE);
	xsmcSet(xsVar(1), xsID_value, xsVar(2));
	xml_parser_rebase(the, parser);
}

static void xml_parser_attribute(xsMachine* the, XMLParser parser)
{
	xml_parser_text_value(the, parser, XML_TEXT_VERBATIM);
	xsmcSetNewObject(xsVar(1));
	xsmcSet(xsVar(1), xsID_name, xsVar(2));
	xml_array_push_property(the, &xsResult, xsID_attributes, &xsVar(1));
	xml_parser_rebase(the, parser);
}

// the innermost open element (the top of the xsVar(0) stack) into slot; returns the
// depth, and leaves slot alone when it is zero
static xsIntegerValue xml_parser_top(xsMachine* the, xsSlot* slot)
{
	xsIntegerValue length = xml_array_length(the, &xsVar(0));
	if (length > 0) {
		xsmcSetInteger(xsVar(5), length - 1);
		xsmcGetAt(*slot, xsVar(0), xsVar(5));
	}
	return length;
}

static void xml_parser_element(xsMachine* the, XMLParser parser)
{
	xsIntegerValue length = xml_parser_top(the, &xsVar(1));
	xsmcSetNewObject(xsResult);
	xml_parser_text_value(the, parser, XML_TEXT_VERBATIM);
	xsmcSet(xsResult, xsID_name, xsVar(2));
	if (length > 0)
		xml_array_push_property(the, &xsVar(1), xsID_elements, &xsResult);
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
	xsIntegerValue length = xml_parser_top(the, &xsResult);
	xsmcSetInteger(xsVar(5), length - 1);
	xsmcSet(xsVar(0), xsID_length, xsVar(5)); // pop
	xml_parser_text_value(the, parser, XML_TEXT_VERBATIM);
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

static void xml_parser_text(xsMachine* the, XMLParser parser, xsIntegerValue unescape)
{
	if (0 == xml_parser_top(the, &xsVar(1)))
		xsUnknownError("xml_parser: text outside element");
	xml_parser_text_value(the, parser, unescape);
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
	xml_parser_text(the, parser, XML_TEXT_VERBATIM);
}

// xml parser specification


#line 563 "modXML.rl"



#line 477 "modXML.c"
static const char _parser_actions[] ICACHE_XS6RO_ATTR = {
	0, 1, 6, 1, 7, 1, 8, 2, 
	4, 12, 2, 5, 14, 2, 8, 1, 
	2, 8, 3, 2, 9, 0, 2, 10, 
	2, 3, 8, 5, 14, 3, 8, 11, 
	13, 4, 8, 1, 4, 12, 4, 8, 
	3, 4, 12, 4, 9, 0, 4, 12
	
};

static const unsigned char _parser_key_offsets[] ICACHE_XS6RO_ATTR = {
	0, 0, 5, 9, 18, 20, 21, 22, 
	23, 25, 26, 27, 28, 29, 30, 31, 
	33, 36, 37, 38, 42, 43, 45, 61, 
	75, 76, 93, 95, 98, 103, 104, 107, 
	108, 109, 110, 111, 112, 122, 124, 125, 
	126, 127, 129, 130, 131, 132, 133, 134, 
	135, 136, 137, 138, 140, 147, 163, 167, 
	168, 170, 186, 200, 201, 218, 220, 223, 
	228, 229, 232, 233, 236
};

static const unsigned char _parser_trans_keys[] ICACHE_XS6RO_ATTR = {
	32u, 60u, 239u, 9u, 13u, 32u, 60u, 9u, 
	13u, 33u, 63u, 96u, 0u, 64u, 91u, 94u, 
	123u, 127u, 45u, 68u, 45u, 45u, 45u, 45u, 
	62u, 79u, 67u, 84u, 89u, 80u, 69u, 62u, 
	91u, 34u, 39u, 93u, 34u, 39u, 32u, 62u, 
	9u, 13u, 63u, 62u, 63u, 32u, 47u, 62u, 
	96u, 0u, 8u, 9u, 13u, 14u, 44u, 59u, 
	64u, 91u, 94u, 123u, 127u, 32u, 47u, 62u, 
	96u, 0u, 8u, 9u, 13u, 14u, 64u, 91u, 
	94u, 123u, 127u, 62u, 32u, 47u, 61u, 62u, 
	96u, 0u, 8u, 9u, 13u, 14u, 44u, 59u, 
	64u, 91u, 94u, 123u, 127u, 34u, 39u, 34u, 
	38u, 60u, 32u, 47u, 62u, 9u, 13u, 59u, 
	38u, 39u, 60u, 59u, 187u, 191u, 60u, 60u, 
	33u, 47u, 63u, 96u, 0u, 64u, 91u, 94u, 
	123u, 127u, 45u, 91u, 45u, 45u, 45u, 45u, 
	62u, 67u, 68u, 65u, 84u, 65u, 91u, 93u, 
	93u, 93u, 62u, 93u, 96u, 0u, 64u, 91u, 
	94u, 123u, 127u, 32u, 47u, 62u, 96u, 0u, 
	8u, 9u, 13u, 14u, 44u, 59u, 64u, 91u, 
	94u, 123u, 127u, 32u, 62u, 9u, 13u, 63u, 
	62u, 63u, 32u, 47u, 62u, 96u, 0u, 8u, 
	9u, 13u, 14u, 44u, 59u, 64u, 91u, 94u, 
	123u, 127u, 32u, 47u, 62u, 96u, 0u, 8u, 
	9u, 13u, 14u, 64u, 91u, 94u, 123u, 127u, 
	62u, 32u, 47u, 61u, 62u, 96u, 0u, 8u, 
	9u, 13u, 14u, 44u, 59u, 64u, 91u, 94u, 
	123u, 127u, 34u, 39u, 34u, 38u, 60u, 32u, 
	47u, 62u, 9u, 13u, 59u, 38u, 39u, 60u, 
	59u, 32u, 9u, 13u, 0
};

static const char _parser_single_lengths[] ICACHE_XS6RO_ATTR = {
	0, 3, 2, 3, 2, 1, 1, 1, 
	2, 1, 1, 1, 1, 1, 1, 2, 
	3, 1, 1, 2, 1, 2, 4, 4, 
	1, 5, 2, 3, 3, 1, 3, 1, 
	1, 1, 1, 1, 4, 2, 1, 1, 
	1, 2, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 2, 1, 4, 2, 1, 
	2, 4, 4, 1, 5, 2, 3, 3, 
	1, 3, 1, 1, 0
};

static const char _parser_range_lengths[] ICACHE_XS6RO_ATTR = {
	0, 1, 1, 3, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 1, 0, 0, 6, 5, 
	0, 6, 0, 0, 1, 0, 0, 0, 
	0, 0, 0, 0, 3, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 3, 6, 1, 0, 
	0, 6, 5, 0, 6, 0, 0, 1, 
	0, 0, 0, 1, 0
};

static const short _parser_index_offsets[] ICACHE_XS6RO_ATTR = {
	0, 0, 5, 9, 16, 19, 21, 23, 
	25, 28, 30, 32, 34, 36, 38, 40, 
	43, 47, 49, 51, 55, 57, 60, 71, 
	81, 83, 95, 98, 102, 107, 109, 113, 
	115, 117, 119, 121, 123, 131, 134, 136, 
	138, 140, 143, 145, 147, 149, 151, 153, 
	155, 157, 159, 161, 164, 169, 180, 184, 
	186, 189, 200, 210, 212, 224, 227, 231, 
	236, 238, 242, 244, 247
};

static const char _parser_indicies[] ICACHE_XS6RO_ATTR = {
	0, 2, 3, 0, 1, 0, 2, 0, 
	1, 4, 5, 1, 1, 1, 1, 6, 
	7, 8, 1, 9, 1, 10, 9, 11, 
	9, 11, 0, 9, 12, 1, 13, 1, 
	14, 1, 15, 1, 16, 1, 17, 1, 
	0, 18, 17, 19, 20, 21, 18, 18, 
	19, 18, 20, 21, 0, 21, 1, 22, 
	5, 0, 22, 5, 23, 25, 26, 1, 
	1, 23, 1, 1, 1, 1, 24, 27, 
	28, 29, 1, 1, 27, 1, 1, 1, 
	30, 31, 1, 32, 34, 35, 36, 1, 
	1, 32, 1, 1, 1, 1, 33, 37, 
	38, 1, 40, 41, 1, 39, 42, 43, 
	44, 42, 1, 39, 41, 46, 40, 1, 
	45, 45, 46, 47, 1, 0, 1, 49, 
	48, 51, 50, 52, 53, 54, 1, 1, 
	1, 1, 55, 56, 57, 1, 58, 1, 
	59, 58, 60, 58, 60, 61, 58, 62, 
	1, 63, 1, 64, 1, 65, 1, 66, 
	1, 67, 1, 69, 68, 71, 70, 72, 
	70, 73, 72, 70, 1, 1, 1, 1, 
	74, 75, 1, 77, 1, 1, 75, 1, 
	1, 1, 1, 76, 78, 79, 78, 1, 
	80, 54, 61, 80, 54, 81, 83, 84, 
	1, 1, 81, 1, 1, 1, 1, 82, 
	85, 86, 87, 1, 1, 85, 1, 1, 
	1, 88, 61, 1, 89, 91, 92, 93, 
	1, 1, 89, 1, 1, 1, 1, 90, 
	94, 95, 1, 97, 98, 1, 96, 99, 
	100, 101, 99, 1, 96, 98, 103, 97, 
	1, 102, 102, 103, 31, 31, 1, 1, 
	0
};

static const char _parser_trans_targs[] ICACHE_XS6RO_ATTR = {
	2, 0, 3, 32, 4, 20, 22, 5, 
	9, 6, 7, 8, 10, 11, 12, 13, 
	14, 15, 16, 17, 18, 19, 21, 23, 
	22, 24, 67, 23, 24, 67, 25, 67, 
	23, 25, 24, 26, 67, 27, 30, 27, 
	28, 29, 23, 24, 67, 30, 31, 33, 
	35, 36, 35, 34, 37, 52, 55, 57, 
	38, 42, 39, 40, 41, 34, 43, 44, 
	45, 46, 47, 48, 49, 50, 49, 50, 
	51, 34, 53, 54, 53, 68, 54, 68, 
	56, 58, 57, 59, 34, 58, 59, 34, 
	60, 58, 60, 59, 61, 34, 62, 65, 
	62, 63, 64, 58, 59, 34, 65, 66
};

static const char _parser_trans_actions[] ICACHE_XS6RO_ATTR = {
	0, 0, 0, 0, 0, 0, 1, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 16, 
	0, 16, 38, 0, 0, 7, 1, 0, 
	13, 0, 13, 13, 33, 3, 3, 0, 
	0, 0, 19, 19, 43, 0, 0, 0, 
	1, 0, 0, 29, 0, 0, 0, 1, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 1, 1, 0, 0, 
	0, 22, 1, 5, 0, 25, 0, 10, 
	0, 16, 0, 16, 38, 0, 0, 7, 
	1, 13, 0, 13, 13, 33, 3, 3, 
	0, 0, 0, 19, 19, 43, 0, 0
};

static const int parser_start = 1;
static const int parser_first_final = 67;
static const int parser_error = 0;

static const int parser_en_inner = 34;
static const int parser_en_main = 1;


#line 566 "modXML.rl"

#pragma unused (parser_en_inner)
#pragma unused (parser_en_main)
#pragma unused (parser_error)

void xs_xml_parse(xsMachine* the)
{
	XMLParserRecord parserRecord;
	XMLParser parser = &parserRecord;
	xsIntegerValue length;

	xsmcVars(XML_VARS);
	parser->compact = (xsmcArgc > 1) ? xsmcToBoolean(xsArg(1)) : 1;
	parser->textOffset = 0;
	parser->textLength = 0;
	parser->input.transcode = 0;
	xsmcSetNewArray(xsVar(0), 0);
	if (xsmcTypeOf(xsArg(0)) == xsStringType) {
		parser->input.buffer = 0;
		parser->input.relocatable = 1;
		parser->base = xsmcToString(xsArg(0));
		length = (xsIntegerValue)c_strlen(parser->base);
	}
	else {
		void* data;
		xsUnsignedValue count;
		xsIntegerValue anyInvalid = 0, anySupplementary = 0;
		parser->input.buffer = 1;
		parser->input.relocatable = (xsBufferRelocatable == xsmcGetBufferReadable(xsArg(0), &data, &count));
		parser->base = (xsStringValue)data;
		length = (xsIntegerValue)count;
		// classify only: parsing is in place either way; flagged input is repaired slice
		// by slice during copy-out (see xml_transcode)
		xml_transcode(NULL, parser->base, length, 0, &anyInvalid, &anySupplementary);
#if mxCESU8
		parser->input.transcode = anyInvalid || anySupplementary;
#else
		parser->input.transcode = anyInvalid;
#endif
	}
	parser->fsm.p = parser->base;
	parser->fsm.pe = parser->fsm.p + length;
	parser->fsm.eof = parser->fsm.pe;
	
#line 689 "modXML.c"
	{
	 parser->fsm.cs = parser_start;
	 parser->fsm.top = 0;
	}

#line 610 "modXML.rl"
	
#line 697 "modXML.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

	if ( ( parser->fsm.p) == ( parser->fsm.pe) )
		goto _test_eof;
	if (  parser->fsm.cs == 0 )
		goto _out;
_resume:
	_keys = _parser_trans_keys + _parser_key_offsets[ parser->fsm.cs];
	_trans = _parser_index_offsets[ parser->fsm.cs];

	_klen = _parser_single_lengths[ parser->fsm.cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( ( (unsigned char)(*parser->fsm.p)) < *_mid )
				_upper = _mid - 1;
			else if ( ( (unsigned char)(*parser->fsm.p)) > *_mid )
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
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( ( (unsigned char)(*parser->fsm.p)) < _mid[0] )
				_upper = _mid - 2;
			else if ( ( (unsigned char)(*parser->fsm.p)) > _mid[1] )
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
#line 483 "modXML.rl"
	{
		xml_parser_assign_value(the, parser);
	}
	break;
	case 1:
#line 487 "modXML.rl"
	{
		xml_parser_attribute(the, parser);
	}
	break;
	case 2:
#line 491 "modXML.rl"
	{
		xml_parser_cdata_section(the, parser);
	}
	break;
	case 3:
#line 495 "modXML.rl"
	{
		xml_parser_element(the, parser);
	}
	break;
	case 4:
#line 499 "modXML.rl"
	{
		xml_parser_enter_element(the, parser);
	}
	break;
	case 5:
#line 503 "modXML.rl"
	{
		xml_parser_exit_element(the, parser);
	}
	break;
	case 6:
#line 507 "modXML.rl"
	{
		xml_parser_start_text(the, parser, 0);
	}
	break;
	case 7:
#line 511 "modXML.rl"
	{
		xml_parser_start_text(the, parser, 1);
	}
	break;
	case 8:
#line 515 "modXML.rl"
	{
		xml_parser_stop_text(the, parser, 0);
	}
	break;
	case 9:
#line 519 "modXML.rl"
	{
		xml_parser_stop_text(the, parser, -1);
	}
	break;
	case 10:
#line 523 "modXML.rl"
	{
		xml_parser_stop_text(the, parser, -2);
	}
	break;
	case 11:
#line 527 "modXML.rl"
	{
		xml_parser_text(the, parser, XML_TEXT_UNESCAPE);
	}
	break;
	case 12:
#line 548 "modXML.rl"
	{ {
		if (parser->fsm.top == XML_STACK_SIZE)
			xsUnknownError("xml_parser: stack overflow");
	{ parser->fsm.stack[ parser->fsm.top++] =  parser->fsm.cs;  parser->fsm.cs = 34;goto _again;}} }
	break;
	case 13:
#line 554 "modXML.rl"
	{ ( parser->fsm.p)--; }
	break;
	case 14:
#line 558 "modXML.rl"
	{ { parser->fsm.cs =  parser->fsm.stack[-- parser->fsm.top]; goto _again;} }
	break;
#line 858 "modXML.c"
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

#line 611 "modXML.rl"
	if (parser->fsm.cs < parser_first_final)
		xsUnknownError("xml_parser: error");
}

// xml text scanner

static void xml_text_scanner_rebase(xsMachine* the, XMLTextScanner scanner)
{
	ptrdiff_t delta = xml_input_delta(the, &scanner->input, &scanner->base);
	if (delta) {
		scanner->fsm.p += delta;
		scanner->fsm.pe += delta;
		scanner->fsm.eof += delta;
		if (scanner->fsm.ts)
			scanner->fsm.ts += delta;
		if (scanner->fsm.te)
			scanner->fsm.te += delta;
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

static void xml_text_scanner_append_slice(xsMachine* the, XMLTextScanner scanner, xsStringValue ts, xsStringValue te)
{
	xsIntegerValue offset = (xsIntegerValue)(ts - scanner->base);
	xsIntegerValue length = (xsIntegerValue)(te - ts);
	xml_slice_to_slot(the, &scanner->input, &xsVar(4), offset, length, 1);
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
	xsStringValue base = xml_input_base(the, &scanner->input);
	xsIntegerValue codepoint = 0;
	xsIntegerValue i = 2;
	unsigned char c = (unsigned char)c_read8(base + offset + i);
	// stop accumulating once out of range: the value cannot become valid again and
	// this keeps the intermediate below 0x10FFFF * 16 + 15, so no integer overflow
	if ((c == 'X') || (c == 'x')) {
		i++;
		while ((i < length - 1) && (codepoint <= XML_MAX_CODEPOINT)) {
			codepoint = (codepoint * 0x10) + xml_hex_value((unsigned char)c_read8(base + offset + i));
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

// xml text scanner specification


#line 738 "modXML.rl"



#line 953 "modXML.c"
static const char _scanner_actions[] ICACHE_XS6RO_ATTR = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 1, 
	7, 1, 8, 1, 9, 1, 10, 1, 
	11
};

static const char _scanner_key_offsets[] ICACHE_XS6RO_ATTR = {
	0, 4, 7, 13, 20, 22, 23, 24, 
	25, 26, 27, 28, 29, 30, 31, 32, 
	33, 34, 35, 36, 37
};

static const unsigned char _scanner_trans_keys[] ICACHE_XS6RO_ATTR = {
	88u, 120u, 48u, 57u, 59u, 48u, 57u, 48u, 
	57u, 65u, 70u, 97u, 102u, 59u, 48u, 57u, 
	65u, 70u, 97u, 102u, 109u, 112u, 112u, 59u, 
	111u, 115u, 59u, 116u, 59u, 116u, 59u, 117u, 
	111u, 116u, 59u, 38u, 38u, 35u, 97u, 103u, 
	108u, 113u, 0
};

static const char _scanner_single_lengths[] ICACHE_XS6RO_ATTR = {
	2, 1, 0, 1, 2, 1, 1, 1, 
	1, 1, 1, 1, 1, 1, 1, 1, 
	1, 1, 1, 1, 5
};

static const char _scanner_range_lengths[] ICACHE_XS6RO_ATTR = {
	1, 1, 3, 3, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0
};

static const char _scanner_index_offsets[] ICACHE_XS6RO_ATTR = {
	0, 4, 7, 11, 16, 19, 21, 23, 
	25, 27, 29, 31, 33, 35, 37, 39, 
	41, 43, 45, 47, 49
};

static const char _scanner_trans_targs[] ICACHE_XS6RO_ATTR = {
	2, 2, 1, 18, 18, 1, 18, 3, 
	3, 3, 18, 18, 3, 3, 3, 18, 
	5, 7, 18, 6, 18, 18, 18, 8, 
	18, 9, 18, 18, 18, 11, 18, 18, 
	18, 13, 18, 18, 18, 15, 18, 16, 
	18, 17, 18, 18, 18, 20, 19, 18, 
	19, 0, 4, 10, 12, 14, 18, 18, 
	18, 18, 18, 18, 18, 18, 18, 18, 
	18, 18, 18, 18, 18, 18, 18, 18, 
	18, 18, 18, 0
};

static const char _scanner_trans_actions[] ICACHE_XS6RO_ATTR = {
	0, 0, 0, 23, 7, 0, 23, 0, 
	0, 0, 23, 7, 0, 0, 0, 23, 
	0, 0, 23, 0, 23, 13, 23, 0, 
	23, 0, 23, 9, 23, 0, 23, 17, 
	23, 0, 23, 15, 23, 0, 23, 0, 
	23, 0, 23, 11, 23, 5, 0, 21, 
	0, 0, 0, 0, 0, 0, 19, 23, 
	23, 23, 23, 23, 23, 23, 23, 23, 
	23, 23, 23, 23, 23, 23, 23, 23, 
	23, 21, 19, 0
};

static const char _scanner_to_state_actions[] ICACHE_XS6RO_ATTR = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 1, 0, 0
};

static const char _scanner_from_state_actions[] ICACHE_XS6RO_ATTR = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 3, 0, 0
};

static const char _scanner_eof_trans[] ICACHE_XS6RO_ATTR = {
	73, 73, 73, 73, 73, 73, 73, 73, 
	73, 73, 73, 73, 73, 73, 73, 73, 
	73, 73, 0, 74, 75
};

static const int scanner_start = 18;
static const int scanner_first_final = 18;
static const int scanner_error = -1;

static const int scanner_en_unescape = 18;


#line 741 "modXML.rl"

#pragma unused (scanner_error)
#pragma unused (scanner_start)

// decodes entity and character references (XML_TEXT_UNESCAPE); escaping lives in xml_writer_escape
static void xml_text_scanner(xsMachine* the, xsIntegerValue offset, xsIntegerValue length, XMLInput input)
{
	XMLTextScannerRecord scannerRecord;
	XMLTextScanner scanner = &scannerRecord;
	xsmcSetString(xsResult, "");
	scanner->input = *input;
	scanner->fsm.cs = scanner_en_unescape;
	scanner->base = xml_input_base(the, input);
	scanner->fsm.p = scanner->base + offset;
	scanner->fsm.pe = scanner->fsm.p + length;
	scanner->fsm.eof = scanner->fsm.pe;
	
#line 1063 "modXML.c"
	{
	( scanner->fsm.ts) = 0;
	( scanner->fsm.te) = 0;
	 scanner->fsm.act = 0;
	}

#line 758 "modXML.rl"
	
#line 1072 "modXML.c"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const unsigned char *_keys;

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
#line 1091 "modXML.c"
		}
	}

	_keys = _scanner_trans_keys + _scanner_key_offsets[ scanner->fsm.cs];
	_trans = _scanner_index_offsets[ scanner->fsm.cs];

	_klen = _scanner_single_lengths[ scanner->fsm.cs];
	if ( _klen > 0 ) {
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( ( (unsigned char)(*scanner->fsm.p)) < *_mid )
				_upper = _mid - 1;
			else if ( ( (unsigned char)(*scanner->fsm.p)) > *_mid )
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
		const unsigned char *_lower = _keys;
		const unsigned char *_mid;
		const unsigned char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( ( (unsigned char)(*scanner->fsm.p)) < _mid[0] )
				_upper = _mid - 2;
			else if ( ( (unsigned char)(*scanner->fsm.p)) > _mid[1] )
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
#line 698 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_codepoint_reference(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
	case 4:
#line 710 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_codepoint(the, scanner, 0x00027); // '
	}}
	break;
	case 5:
#line 702 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_codepoint(the, scanner, 0x00022); // "
	}}
	break;
	case 6:
#line 706 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_codepoint(the, scanner, 0x00026); // &
	}}
	break;
	case 7:
#line 714 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_codepoint(the, scanner, 0x0003C); // <
	}}
	break;
	case 8:
#line 718 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p)+1;{
		xml_text_scanner_append_codepoint(the, scanner, 0x0003E); // >
	}}
	break;
	case 9:
#line 722 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p);( scanner->fsm.p)--;{
		xml_text_scanner_append_slice(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
	case 10:
#line 722 "modXML.rl"
	{( scanner->fsm.te) = ( scanner->fsm.p);( scanner->fsm.p)--;{
		xml_text_scanner_append_slice(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
	case 11:
#line 722 "modXML.rl"
	{{( scanner->fsm.p) = ((( scanner->fsm.te)))-1;}{
		xml_text_scanner_append_slice(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}}
	break;
#line 1214 "modXML.c"
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
#line 1227 "modXML.c"
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

#line 759 "modXML.rl"
	if (scanner->fsm.cs < scanner_first_final)
		xsUnknownError("xml_text_scanner: error");
}

// xml serializer
//
// Walks the object tree that xs_xml_parse produces (or the application builds) and
// writes the document into one C buffer, then hands XS a single string. No XS
// allocation happens while a pointer into the tree is held: every xsmcToString
// result is consumed before the next XS call. The walk is iterative; open elements
// are kept in an XS array (xsVar(0)) so the tree stays rooted, and the depth is
// bounded by XML_STACK_SIZE like the parser.

// a linked list of fixed-size chunks: no realloc, no re-copy while writing; each byte is
// written once here and once into the final String or ArrayBuffer
typedef struct XMLWriterChunkStruct {
	struct XMLWriterChunkStruct* next;
	xsIntegerValue length;
	char data[XML_WRITER_CHUNK];
} XMLWriterChunkRecord, *XMLWriterChunk;

typedef struct {
	XMLWriterChunk first;
	XMLWriterChunk last;
	xsIntegerValue total;
} XMLWriterRecord, *XMLWriter;

static void xml_writer_free(XMLWriter writer)
{
	XMLWriterChunk chunk = writer->first;
	while (chunk) {
		XMLWriterChunk next = chunk->next;
		c_free(chunk);
		chunk = next;
	}
	writer->first = writer->last = NULL;
	writer->total = 0;
}

static void xml_writer_append(xsMachine* the, XMLWriter writer, const char* text, xsIntegerValue length)
{
	while (length > 0) {
		XMLWriterChunk chunk = writer->last;
		xsIntegerValue room = chunk ? (XML_WRITER_CHUNK - chunk->length) : 0;
		xsIntegerValue take;
		if (room == 0) {
			chunk = c_malloc(sizeof(XMLWriterChunkRecord));
			if (!chunk) {
				xml_writer_free(writer);
				xsUnknownError("not enough memory");
			}
			chunk->next = NULL;
			chunk->length = 0;
			if (writer->last)
				writer->last->next = chunk;
			else
				writer->first = chunk;
			writer->last = chunk;
			room = XML_WRITER_CHUNK;
		}
		take = (length < room) ? length : room;
		c_memcpy(chunk->data + chunk->length, text, take);
		chunk->length += take;
		writer->total += take;
		text += take;
		length -= take;
	}
}

static void xml_writer_append_static(xsMachine* the, XMLWriter writer, const char* text)
{
	xml_writer_append(the, writer, text, (xsIntegerValue)c_strlen(text));
}

// allocate the result once, fill it from the chunks with no XS call in between, release the chunks
static void xml_writer_to_result(xsMachine* the, XMLWriter writer, xsIntegerValue toBuffer)
{
	XMLWriterChunk chunk;
	xsStringValue dst;
	if (toBuffer) {
		xsmcSetArrayBuffer(xsResult, NULL, writer->total);
		dst = (xsStringValue)xsmcToArrayBuffer(xsResult);
	}
	else {
		xsmcSetStringBuffer(xsResult, NULL, writer->total);
		dst = xsmcToString(xsResult);
	}
	for (chunk = writer->first; chunk; chunk = chunk->next) {
		c_memcpy(dst, chunk->data, chunk->length);
		dst += chunk->length;
	}
	xml_writer_free(writer);
}

// escapes text for a context: XML_ESCAPE_TEXT, XML_ESCAPE_ATTRIBUTE (adds quotes and whitespace),
// XML_ESCAPE_CDATA (splits "]]>")
static void xml_writer_escape(xsMachine* the, XMLWriter writer, xsStringValue text, xsIntegerValue context)
{
	xsIntegerValue length = (xsIntegerValue)c_strlen(text);
	xsIntegerValue i, start = 0;
	xsIntegerValue attribute = (context == XML_ESCAPE_ATTRIBUTE);
	if (context == XML_ESCAPE_CDATA) {
		for (i = 0; i + 2 < length; i++) {
			if ((c_read8(text + i) == ']') && (c_read8(text + i + 1) == ']') && (c_read8(text + i + 2) == '>')) {
				xml_writer_append(the, writer, text + start, i - start);
				xml_writer_append_static(the, writer, "]]]]><![CDATA[>"); // split: CDATA has no escape
				i += 2;
				start = i + 1;
			}
		}
		xml_writer_append(the, writer, text + start, length - start);
		return;
	}
	for (i = 0; i < length; i++) {
		unsigned char c = (unsigned char)c_read8(text + i);
		const char* replacement = NULL;
		char reference[7];
		switch (c) {
			case '&': replacement = "&amp;"; break;
			case '<': replacement = "&lt;"; break;
			case '>': replacement = "&gt;"; break;
			case '"': replacement = attribute ? "&quot;" : NULL; break;
			case '\'': replacement = attribute ? "&apos;" : NULL; break;
			// XML 1.0 §2.11: readers turn literal CR into LF before parsing, so CR must be a
			// reference to survive; §3.3.3: in attribute values readers turn literal TAB and LF
			// into spaces, so those too. Canonical XML (C14N 1.0) writes the same references.
			case '\r': replacement = "&#x0D;"; break;
			case '\t': replacement = attribute ? "&#x09;" : NULL; break;
			case '\n': replacement = attribute ? "&#x0A;" : NULL; break;
			case '\v': case '\f': break;
			default:
				if ((c > 0) && (c < 32)) {	// bytes above 0x7F pass through: c is unsigned
					c_memcpy(reference, "&#x??;", 7); // not an initializer: GCC would memcpy from ROM, which faults on ESP8266; here in the rare branch, not once per byte
					reference[3] = escapeHexa(c >> 4);
					reference[4] = escapeHexa(c);
					replacement = reference;
				}
				break;
		}
		if (replacement) {
			xml_writer_append(the, writer, text + start, i - start);
			xml_writer_append_static(the, writer, replacement);
			start = i + 1;
		}
	}
	xml_writer_append(the, writer, text + start, length - start);
}

// XML.escape: kind "text" (default), "attribute", or "cdata" runs the writer's escaper,
// the single implementation of the escaping rules
void xs_xml_escape(xsMachine* the)
{
	XMLWriterRecord writerRecord = {NULL, NULL, 0};
	XMLWriter writer = &writerRecord;
	xsIntegerValue context = XML_ESCAPE_TEXT;

	xsmcVars(XML_VARS);
	if ((xsmcArgc > 1) && (xsmcTypeOf(xsArg(1)) != xsUndefinedType)) {
		xsStringValue kind = xsmcToString(xsArg(1));
		if (!c_strcmp(kind, "attribute"))
			context = XML_ESCAPE_ATTRIBUTE;
		else if (!c_strcmp(kind, "cdata"))
			context = XML_ESCAPE_CDATA;
		else if (c_strcmp(kind, "text"))
			xsUnknownError("xml_escape: kind must be \"text\", \"attribute\", or \"cdata\"");
	}
	xml_writer_escape(the, writer, xsmcToString(xsArg(0)), context);
	xml_writer_to_result(the, writer, 0);
}

// XML.unescape: entity and character references become characters, through the scanner the
// parser itself uses, so the semantics are identical to parsing
void xs_xml_unescape(xsMachine* the)
{
	XMLInputRecord input = { 0, 1, 0 };	// a string: relocatable, no transcoding
	xsIntegerValue length;

	xsmcVars(XML_VARS);
	length = (xsIntegerValue)c_strlen(xsmcToString(xsArg(0)));
	xml_text_scanner(the, 0, length, &input);
}

// ` name="value"` or ` name`; the value is escaped as an attribute value
static void xml_serialize_attribute(xsMachine* the, XMLWriter writer, xsSlot* name, xsSlot* value, xsBooleanValue bare)
{
	xml_writer_append_static(the, writer, " ");
	xml_writer_append_static(the, writer, xsmcToString(*name));
	if (bare)
		return;
	xml_writer_append_static(the, writer, "=\"");
	xml_writer_escape(the, writer, xsmcToString(*value), XML_ESCAPE_ATTRIBUTE);
	xml_writer_append_static(the, writer, "\"");
}

// item.elements into xsVar(2); returns its length, or 0 when it is not an array
static xsIntegerValue xml_serialize_elements(xsMachine* the, xsSlot* item)
{
	xsmcGet(xsVar(2), *item, xsID_elements);
	if (!xsmcIsInstanceOf(xsVar(2), xsArrayPrototype))
		return 0;
	xsmcGet(xsVar(3), xsVar(2), xsID_length);
	return xsmcToInteger(xsVar(3));
}

// `<name attributes` then `/>` for a leaf (returns 0) or `>` when children follow (returns 1)
static xsBooleanValue xml_serialize_open(xsMachine* the, XMLWriter writer, xsSlot* item)
{
	xsIntegerValue i, count;
	xml_writer_append_static(the, writer, "<");
	xsmcGet(xsVar(2), *item, xsID_name);
	xml_writer_append_static(the, writer, xsmcToString(xsVar(2)));

	xsmcGet(xsVar(2), *item, xsID_attributes);
	if (xsmcIsInstanceOf(xsVar(2), xsArrayPrototype)) {
		xsmcGet(xsVar(3), xsVar(2), xsID_length);
		count = xsmcToInteger(xsVar(3));
		for (i = 0; i < count; i++) {
			xsmcGetIndex(xsVar(3), xsVar(2), i);
			xsmcGet(xsVar(4), xsVar(3), xsID_name);
			xsmcGet(xsVar(5), xsVar(3), xsID_value);
			xml_serialize_attribute(the, writer, &xsVar(4), &xsVar(5), !xsmcHas(xsVar(3), xsID_value));
		}
	}
	else if (xsmcTypeOf(xsVar(2)) == xsReferenceType) {
		xsmcGet(xsVar(3), xsGlobal, xsID_Object);
		xsmcCall(xsVar(3), xsVar(3), xsID_keys, &xsVar(2), NULL);
		xsmcGet(xsVar(4), xsVar(3), xsID_length);
		count = xsmcToInteger(xsVar(4));
		for (i = 0; i < count; i++) {
			xsmcGetIndex(xsVar(4), xsVar(3), i);
			xsmcGetAt(xsVar(5), xsVar(2), xsVar(4));
			if (xsmcTypeOf(xsVar(5)) == xsBooleanType) {
				if (xsmcToBoolean(xsVar(5)))
					xml_serialize_attribute(the, writer, &xsVar(4), &xsVar(5), 1);
			}
			else
				xml_serialize_attribute(the, writer, &xsVar(4), &xsVar(5), 0);
		}
	}

	count = xml_serialize_elements(the, item);
	if ((count == 0) && !xsmcHas(*item, xsID_text) && !xsmcHas(*item, xsID_TEXT)) {
		xml_writer_append_static(the, writer, "/>");
		return 0;
	}
	// `text: undefined` and `TEXT: undefined` count as absent, as in JavaScript
	if (count == 0) {
		xsmcGet(xsVar(3), *item, xsID_text);
		xsmcGet(xsVar(4), *item, xsID_TEXT);
		if ((xsmcTypeOf(xsVar(3)) == xsUndefinedType) && (xsmcTypeOf(xsVar(4)) == xsUndefinedType)) {
			xml_writer_append_static(the, writer, "/>");
			return 0;
		}
	}
	xml_writer_append_static(the, writer, ">");
	return 1;
}

// text, CDATA section, and `</name>`
static void xml_serialize_close(xsMachine* the, XMLWriter writer, xsSlot* item)
{
	xsmcGet(xsVar(2), *item, xsID_text);
	if (xsmcTypeOf(xsVar(2)) != xsUndefinedType)
		xml_writer_escape(the, writer, xsmcToString(xsVar(2)), XML_ESCAPE_TEXT);
	xsmcGet(xsVar(2), *item, xsID_TEXT);
	if (xsmcTypeOf(xsVar(2)) != xsUndefinedType) {
		xml_writer_append_static(the, writer, "<![CDATA[");
		xml_writer_escape(the, writer, xsmcToString(xsVar(2)), XML_ESCAPE_CDATA);
		xml_writer_append_static(the, writer, "]]>");
	}
	xml_writer_append_static(the, writer, "</");
	xsmcGet(xsVar(2), *item, xsID_name);
	xml_writer_append_static(the, writer, xsmcToString(xsVar(2)));
	xml_writer_append_static(the, writer, ">");
}

static void xml_serialize(xsMachine* the, XMLWriter writer, xsBooleanValue declaration)
{
	xsIntegerValue depth = 0;
	if (declaration)
		xml_writer_append_static(the, writer, "<?xml version=\"1.0\" encoding=\"utf-8\"?>");
	xsmcSetNewArray(xsVar(0), 0);
	xsVar(1) = xsArg(0);
	if (!xml_serialize_open(the, writer, &xsVar(1)))
		return;
	// stack: item at 2 * depth, index of the next child at 2 * depth + 1
	xsmcSetIndex(xsVar(0), 0, xsVar(1));
	xsmcSetInteger(xsVar(1), 0);
	xsmcSetIndex(xsVar(0), 1, xsVar(1));
	depth = 1;
	while (depth > 0) {
		xsIntegerValue index, count;
		xsmcGetIndex(xsVar(1), xsVar(0), 2 * depth - 2);
		xsmcGetIndex(xsVar(2), xsVar(0), 2 * depth - 1);
		index = xsmcToInteger(xsVar(2));
		count = xml_serialize_elements(the, &xsVar(1));
		if (index < count) {
			xsmcSetInteger(xsVar(3), index + 1);
			xsmcSetIndex(xsVar(0), 2 * depth - 1, xsVar(3));
			xsmcGetIndex(xsVar(1), xsVar(2), index);
			if (xml_serialize_open(the, writer, &xsVar(1))) {
				if (depth == XML_STACK_SIZE)
					xsUnknownError("xml_serializer: stack overflow");
				xsmcSetIndex(xsVar(0), 2 * depth, xsVar(1));
				xsmcSetInteger(xsVar(1), 0);
				xsmcSetIndex(xsVar(0), 2 * depth + 1, xsVar(1));
				depth++;
			}
			continue;
		}
		xml_serialize_close(the, writer, &xsVar(1));
		depth--;
		xsmcSetInteger(xsVar(1), 2 * depth);
		xsmcSet(xsVar(0), xsID_length, xsVar(1));
	}
}

void xs_xml_serialize(xsMachine* the)
{
	XMLWriterRecord writerRecord = {NULL, NULL, 0};
	XMLWriter writer = &writerRecord;
	xsBooleanValue declaration = 1;
	xsIntegerValue toBuffer = 0;

	xsmcVars(XML_VARS);
	// options, ECMA-419 style: { format: "string" (default) | "buffer", declaration: true (default) }
	if ((xsmcArgc > 1) && (xsmcTypeOf(xsArg(1)) != xsUndefinedType)) {
		if (xsmcHas(xsArg(1), xsID_declaration)) {
			xsmcGet(xsVar(0), xsArg(1), xsID_declaration);
			declaration = xsmcToBoolean(xsVar(0));
		}
		if (xsmcHas(xsArg(1), xsID_format)) {
			xsStringValue format;
			xsmcGet(xsVar(0), xsArg(1), xsID_format);
			format = xsmcToString(xsVar(0));
			if (!c_strcmp(format, "buffer"))
				toBuffer = 1;
			else if (c_strcmp(format, "string"))
				xsUnknownError("xml_serializer: format must be \"string\" or \"buffer\"");
		}
	}
	xsTry {
		xml_serialize(the, writer, declaration);
	}
	xsCatch {
		xml_writer_free(writer);
		xsThrow(xsException);
	}
	xml_writer_to_result(the, writer, toBuffer);
}