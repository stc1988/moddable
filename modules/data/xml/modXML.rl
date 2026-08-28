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

%%{
	machine parser;
	access parser->fsm.;
	variable p parser->fsm.p;
	variable pe parser->fsm.pe;
	variable eof parser->fsm.eof;

	prepush {
		if (parser->fsm.top == XML_STACK_SIZE)
			xsUnknownError("xml_parser: stack overflow");
	}

	action assign_value {
		xml_parser_assign_value(the, parser);
	}

	action attribute {
		xml_parser_attribute(the, parser);
	}

	action cdata_section {
		xml_parser_cdata_section(the, parser);
	}

	action comment {
		xml_parser_comment(the, parser);
	}

	action element {
		xml_parser_element(the, parser);
	}

	action enter_element {
		xml_parser_enter_element(the, parser);
	}

	action exit_element {
		xml_parser_exit_element(the, parser);
	}

	action processing_instruction {
		xml_parser_processing_instruction(the, parser);
	}

	action start_text {
		xml_parser_start_text(the, parser, 0);
	}

	action start_text_1 {
		xml_parser_start_text(the, parser, 1);
	}

	action stop_text {
		xml_parser_stop_text(the, parser, 0);
	}

	action stop_text_1 {
		xml_parser_stop_text(the, parser, -1);
	}

	action stop_text_2 {
		xml_parser_stop_text(the, parser, -2);
	}

	action text {
		xml_parser_text(the, parser, 1);
	}

	name_char = ( alnum | '-' | '_' | '.' | ':' | -128..-1 ); # any non-ASCII (UTF-8) byte; alphtype is signed char
	name_start_char = ( alpha | '_' | -128..-1 ); # any non-ASCII (UTF-8) byte; alphtype is signed char
	name = ( name_start_char name_char* );

	reference = ( '&' [^;]* ';' ); # not processed here

	value = ( '"' ( [^<&""] | reference )* '"' | "'" ( [^<&''] | reference )* "'" );

	attribute = ( name >start_text %stop_text %attribute ( '=' value >start_text_1 %stop_text_1 %assign_value )? );

	# bodies run up to the first terminator (finish-guarded ':>>'); the actions sit on the
	# terminator's final character so they fire once, not on every candidate start
	cdata_section = ( '<![CDATA[' ( any* ) >start_text :>> ']]>' @stop_text_2 @cdata_section );

	comment = ( '<!--' ( any* ) >start_text :>> '-->' @stop_text_2 @comment );

	element = ( '<' name >start_text %stop_text %element ( space+ attribute )* space* ( '/>' | '>' @enter_element @{ fcall inner; } ));

	processing_instruction = ( '<?' ( any* ) >start_text :>> '?>' @stop_text_1 @processing_instruction );

	doctype = ( '<!DOCTYPE' [^\[>]* ( '[' [^\]]* ']' space* )? '>' ); # skipped, not processed

	text = ( [^<]+ >start_text %stop_text %text '<' @{ fhold; } );

	content = ( cdata_section | comment | element | processing_instruction | text );

	inner := ( content* '</' name >start_text %stop_text space* '>' @exit_element @{ fret; } );

	bom = ( (-17) (-69) (-65) ); # UTF-8 byte order mark EF BB BF, skipped if present (parenthesized: bare '-' is the difference operator)

	main := ( bom? space* (( processing_instruction | comment | doctype ) space* )* element space* );
}%%

%% write data;

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
	%% write init;
	%% write exec;
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

%%{
	machine scanner;
	access scanner->fsm.;
	variable p scanner->fsm.p;
	variable pe scanner->fsm.pe;
	variable eof scanner->fsm.eof;
	variable ts scanner->fsm.ts;
	variable te scanner->fsm.te;

	action append_cdata_escape {
		xml_text_scanner_append_static(the, scanner, "]]]]><![CDATA[>"); // or throw like KPR/Expat?
	}

	action append_codepoint {
		xml_text_scanner_append_codepoint_reference(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}

	action append_codepoint_22 {
		xml_text_scanner_append_codepoint(the, scanner, 0x00022); // "
	}

	action append_codepoint_26 {
		xml_text_scanner_append_codepoint(the, scanner, 0x00026); // &
	}

	action append_codepoint_27 {
		xml_text_scanner_append_codepoint(the, scanner, 0x00027); // '
	}

	action append_codepoint_3C {
		xml_text_scanner_append_codepoint(the, scanner, 0x0003C); // <
	}

	action append_codepoint_3E {
		xml_text_scanner_append_codepoint(the, scanner, 0x0003E); // >
	}

	action append_character_reference {
		xml_text_scanner_append_character_reference(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}

	action append_entity_reference_amp {
		xml_text_scanner_append_static(the, scanner, "&amp;");
	}

	action append_entity_reference_apos {
		if (scanner->mode)
			xml_text_scanner_append_static(the, scanner, "&apos;");
		else
			xml_text_scanner_append_static(the, scanner, "'");
	}

	action append_entity_reference_lt {
		xml_text_scanner_append_static(the, scanner, "&lt;");
	}

	action append_entity_reference_gt {
		xml_text_scanner_append_static(the, scanner, "&gt;");
	}

	action append_entity_reference_quot {
		if (scanner->mode)
			xml_text_scanner_append_static(the, scanner, "&quot;");
		else
			xml_text_scanner_append_static(the, scanner, "\"");
	}

	action append_text {
		xml_text_scanner_append_slice(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}

	cntrl_noend = ( cntrl - 0 );
	cntrl_noendspace = ( cntrl_noend - space );

	codepoint = ( digit+ | [Xx] xdigit+ );

	escape := |*
		cntrl_noendspace+                   => append_character_reference;
		"'"                                 => append_entity_reference_apos;
		'"'                                 => append_entity_reference_quot;
		'&'                                 => append_entity_reference_amp;
		'<'                                 => append_entity_reference_lt;
		'>'                                 => append_entity_reference_gt;
		^( cntrl_noendspace | [''""&<>] )+	=> append_text;
	*|;

	escape_cdata_section := |*
		']]>'                               => append_cdata_escape;
		']'                                 => append_text;
		[^\]]+                              => append_text;
	*|;

	unescape := |*
		'&#' codepoint ';'                  => append_codepoint;
		'&apos;'                            => append_codepoint_27;
		'&quot;'                            => append_codepoint_22;
		'&amp;'                             => append_codepoint_26;
		'&lt;'                              => append_codepoint_3C;
		'&gt;'                              => append_codepoint_3E;
		'&'                                 => append_text;
		[^&]+                               => append_text;
	*|;
}%%

%% write data;

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
	%% write init nocs;
	%% write exec;
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
