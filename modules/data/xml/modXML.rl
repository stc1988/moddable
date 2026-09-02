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
			else if (remain >= 1) take = 1;
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
		if (dst) {	// U+FFFD
			*dst++ = (char)0xEF;
			*dst++ = (char)0xBF;
			*dst++ = (char)0xBD;
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

static void xml_parser_rebase(xsMachine* the, XMLParser parser)
{
	xsStringValue base;
	ptrdiff_t delta;
	if (!parser->input.relocatable)
		return;
	base = xml_input_base(the, &parser->input);
	delta = base - parser->base;
	if (delta) {
		parser->fsm.p += delta;
		parser->fsm.pe += delta;
		parser->fsm.eof += delta;
		parser->base = base;
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

static void xml_parser_xsVar2_text_helper(xsMachine* the, XMLParser parser, xsIntegerValue mode)
{
	if (mode) {
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
	alphtype unsigned char; # keys sort as bytes on every target, whatever the platform's char signedness
	getkey (unsigned char)(*parser->fsm.p);
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

	name_char = ( alnum | '-' | '_' | '.' | ':' | 0x80..0xFF ); # any non-ASCII (UTF-8) byte
	name_start_char = ( alpha | '_' | 0x80..0xFF ); # any non-ASCII (UTF-8) byte
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

	doctype = ( '<!DOCTYPE' [^\[>]* ( '[' ( [^\]"'] | '"' [^"]* '"' | "'" [^']* "'" )* ']' space* )? '>' ); # skipped, not processed; ']' inside a quoted literal does not end the subset (2.8)

	text = ( [^<]+ >start_text %stop_text %text '<' @{ fhold; } );

	content = ( cdata_section | comment | element | processing_instruction | text );

	inner := ( content* '</' name >start_text %stop_text space* '>' @exit_element @{ fret; } );

	bom = ( 0xEF 0xBB 0xBF ); # UTF-8 byte order mark, skipped if present

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
	%% write init;
	%% write exec;
	if (parser->fsm.cs < parser_first_final)
		xsUnknownError("xml_parser: error");
}

// xml text scanner

static xsStringValue xml_text_scanner_base(xsMachine* the, XMLTextScanner scanner)
{
	return xml_input_base(the, &scanner->input);
}

static void xml_text_scanner_rebase(xsMachine* the, XMLTextScanner scanner)
{
	xsStringValue base;
	ptrdiff_t delta;
	if (!scanner->input.relocatable)
		return;
	base = xml_input_base(the, &scanner->input);
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
	xsStringValue base = xml_text_scanner_base(the, scanner);
	xsIntegerValue codepoint = 0;
	xsIntegerValue i = 2;
	unsigned char c = (unsigned char)c_read8(base + offset + i);
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

// xml text scanner specification

%%{
	machine scanner;
	alphtype unsigned char;
	getkey (unsigned char)(*scanner->fsm.p);
	access scanner->fsm.;
	variable p scanner->fsm.p;
	variable pe scanner->fsm.pe;
	variable eof scanner->fsm.eof;
	variable ts scanner->fsm.ts;
	variable te scanner->fsm.te;

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

	action append_text {
		xml_text_scanner_append_slice(the, scanner, scanner->fsm.ts, scanner->fsm.te);
	}

	codepoint = ( digit+ | [Xx] xdigit+ );

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

// decodes entity and character references: the parser's mode 3; escaping lives in xml_writer_escape
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
	%% write init nocs;
	%% write exec;
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

// the same rules as the escape scanner: mode 0 text, 1 attribute value, 2 CDATA section
static void xml_writer_escape(xsMachine* the, XMLWriter writer, xsStringValue text, xsIntegerValue mode)
{
	xsIntegerValue length = (xsIntegerValue)c_strlen(text);
	xsIntegerValue i, start = 0;
	if (mode == 2) {
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
			case '"': replacement = mode ? "&quot;" : NULL; break;
			case '\'': replacement = mode ? "&apos;" : NULL; break;
			// XML 1.0 §2.11: readers turn literal CR into LF before parsing, so CR must be a
			// reference to survive; §3.3.3: in attribute values readers turn literal TAB and LF
			// into spaces, so those too. Canonical XML (C14N 1.0) writes the same references.
			case '\r': replacement = "&#x0D;"; break;
			case '\t': replacement = mode ? "&#x09;" : NULL; break;
			case '\n': replacement = mode ? "&#x0A;" : NULL; break;
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
	xsIntegerValue mode = 0;

	xsmcVars(6);
	if ((xsmcArgc > 1) && (xsmcTypeOf(xsArg(1)) != xsUndefinedType)) {
		xsStringValue kind = xsmcToString(xsArg(1));
		if (!c_strcmp(kind, "attribute"))
			mode = 1;
		else if (!c_strcmp(kind, "cdata"))
			mode = 2;
		else if (c_strcmp(kind, "text"))
			xsUnknownError("xml_escape: kind must be \"text\", \"attribute\", or \"cdata\"");
	}
	{
		XMLWriterRecord writerRecord = {NULL, NULL, 0};
		XMLWriter writer = &writerRecord;
		XMLWriterChunk chunk;
		xsStringValue dst;
		xml_writer_escape(the, writer, xsmcToString(xsArg(0)), mode);
		xsmcSetStringBuffer(xsResult, NULL, writer->total);
		dst = xsmcToString(xsResult);
		for (chunk = writer->first; chunk; chunk = chunk->next) {
			c_memcpy(dst, chunk->data, chunk->length);
			dst += chunk->length;
		}
		xml_writer_free(writer);
	}
}

// XML.unescape: entity and character references become characters, through the scanner the
// parser itself uses, so the semantics are identical to parsing
void xs_xml_unescape(xsMachine* the)
{
	XMLInputRecord input = { 0, 1, 0 };	// a string: relocatable, no transcoding
	xsIntegerValue length;

	xsmcVars(6);
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
	xml_writer_escape(the, writer, xsmcToString(*value), 1);
	xml_writer_append_static(the, writer, "\"");
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

	xsmcGet(xsVar(2), *item, xsID_elements);
	if (xsmcIsInstanceOf(xsVar(2), xsArrayPrototype)) {
		xsmcGet(xsVar(3), xsVar(2), xsID_length);
		count = xsmcToInteger(xsVar(3));
	}
	else
		count = 0;
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
		xml_writer_escape(the, writer, xsmcToString(xsVar(2)), 0);
	xsmcGet(xsVar(2), *item, xsID_TEXT);
	if (xsmcTypeOf(xsVar(2)) != xsUndefinedType) {
		xml_writer_append_static(the, writer, "<![CDATA[");
		xml_writer_escape(the, writer, xsmcToString(xsVar(2)), 2);
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
		xsmcGet(xsVar(2), xsVar(1), xsID_elements);
		if (xsmcIsInstanceOf(xsVar(2), xsArrayPrototype)) {
			xsmcGet(xsVar(3), xsVar(2), xsID_length);
			count = xsmcToInteger(xsVar(3));
		}
		else
			count = 0;
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
	XMLWriterChunk chunk;
	xsStringValue dst;

	xsmcVars(6);
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
	// allocate once, then fill from the fragments with no XS calls in between
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