/*
 * Copyright (c) 2016-2026  Moddable Tech, Inc.
 *
 *   This file is part of the Moddable SDK Tools.
 * 
 *   The Moddable SDK Tools is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 * 
 *   The Moddable SDK Tools is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 * 
 *   You should have received a copy of the GNU General Public License
 *   along with the Moddable SDK Tools.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "xsAll.h"
#include "xsScript.h"

static void Tool_prototype_listSpecifiersError(void* console, txString thePath, txInteger theLine, txString theFormat, c_va_list theArguments)
{
	if (thePath) {
		#if mxWindows
			fprintf(stderr, "%s(%d): error: ", thePath, (int)theLine);
		#else
			fprintf(stderr, "%s:%d: error: ", thePath, (int)theLine);
		#endif
	}
	else
		fprintf(stderr, "# error: ");
	vfprintf(stderr, theFormat, theArguments);
	fprintf(stderr, "!\n");
}

static void Tool_prototype_listSpecifiersWarning(void* console, txString thePath, txInteger theLine, txString theFormat, c_va_list theArguments)
{
	if (thePath) {
		#if mxWindows
			fprintf(stderr, "%s(%d): warning: ", thePath, (int)theLine);
		#else
			fprintf(stderr, "%s:%d: warning: ", thePath, (int)theLine);
		#endif
	}
	else
		fprintf(stderr, "# warning: ");
	vfprintf(stderr, theFormat, theArguments);
	fprintf(stderr, "!\n");
}

#define mxGlobalPathDepth 5

typedef struct {
	txMachine* the;
	txSlot* array;
} txGlobalPathWalker;

static void Tool_prototype_pushGlobal(txGlobalPathWalker* walker, txString string)
{
	txMachine* the = walker->the;
	mxPushStringC(string);
	fxArrayCacheItem(the, walker->array, the->stack);
	mxPop();
}

static txNode* Tool_prototype_unwrapNode(txNode* node)
{
	while (node && ((node->description->token == XS_TOKEN_OPTION) || (node->description->token == XS_TOKEN_CHAIN)))
		node = ((txUnaryExpressionNode*)node)->right;
	return node;
}

static void Tool_prototype_reportGlobalPath(txNode* it, txGlobalPathWalker* walker)
{
	txString parts[mxGlobalPathDepth];
	txInteger count = 0;
	txNode* node = it;
	char buffer[256];
	txInteger i, length = 0;
	while (node && (count < mxGlobalPathDepth)) {
		if (node->description->token == XS_TOKEN_MEMBER) {
			parts[count++] = ((txMemberNode*)node)->symbol->string;
			node = Tool_prototype_unwrapNode(((txMemberNode*)node)->reference);
		}
		else if (node->description->token == XS_TOKEN_MEMBER_AT) {
			txNode* at = ((txMemberAtNode*)node)->at;
			if (!at || (at->description->token != XS_TOKEN_STRING))
				return;
			parts[count++] = ((txStringNode*)at)->value;
			node = Tool_prototype_unwrapNode(((txMemberAtNode*)node)->reference);
		}
		else
			break;
	}
	if (!node || (count == 0) || (count >= mxGlobalPathDepth))
		return;
	if (node->description->token != XS_TOKEN_ACCESS)
		return;
	if (((txAccessNode*)node)->declaration != C_NULL)
		return;
	length = c_strlen(((txAccessNode*)node)->symbol->string);
	if (length >= (txInteger)sizeof(buffer))
		return;
	c_strcpy(buffer, ((txAccessNode*)node)->symbol->string);
	for (i = count; i > 0; i--) {
		txInteger part = c_strlen(parts[i - 1]);
		if ((length + 1 + part) >= (txInteger)sizeof(buffer))
			return;
		buffer[length++] = '.';
		c_strcpy(buffer + length, parts[i - 1]);
		length += part;
	}
	Tool_prototype_pushGlobal(walker, buffer);
}

static void Tool_prototype_walkGlobals(void* it, void* param)
{
	txNode* node = it;
	txToken token;
	if (!node)
		return;
	token = node->description->token;
	if ((token == XS_TOKEN_MEMBER) || (token == XS_TOKEN_MEMBER_AT))
		Tool_prototype_reportGlobalPath(node, param);
	else if (token == XS_TOKEN_ACCESS) {
		txAccessNode* access = (txAccessNode*)node;
		if (access->declaration == C_NULL)
			Tool_prototype_pushGlobal(param, access->symbol->string);
	}
	(*node->description->dispatch->distribute)(node, Tool_prototype_walkGlobals, param);
}

void Tool_prototype_listSpecifiers(txMachine* the)
{
	char *path = fxToString(the, mxArgv(0));
	txParser _parser;
	txParser* parser = &_parser;
	txParserJump jump;
	FILE* file = NULL;
	txString name = NULL;
	fxInitializeParser(parser, NULL, the->parserBufferSize, the->parserTableModulo);
	parser->firstJump = &jump;
	parser->reportError = Tool_prototype_listSpecifiersError;
	parser->reportWarning = Tool_prototype_listSpecifiersWarning;
	if (c_setjmp(jump.jmp_buf) == 0) {
		file = fopen(path, "r");
		mxParserThrowElse(file);
		parser->path = fxNewParserSymbol(parser, path);
		fxParserTree(parser, file, (txGetter)fgetc, mxDebugFlag | mxStrictFlag, &name);
		fclose(file);
		file = NULL;
		fxParserHoist(parser);
		fxParserBind(parser);
		if (parser->errorCount == 0) {
			txModuleNode* self = (txModuleNode*)(parser->root);
			txDeclareNode* node = self->scope->firstDeclareNode;
			txBoolean hasDefault = 0;
			txBoolean isAsync = (self->flags & mxAwaitingFlag) ? 1 : 0;

			fxNewObject(the);
			mxPullSlot(mxResult);

			mxPush(mxArrayPrototype);
			fxNewArrayInstance(the);
			fxArrayCacheBegin(the, the->stack);
			while (node) {
				if (node->flags & mxDeclareNodeUseClosureFlag) {
					txSpecifierNode* specifier = node->importSpecifier;
					if (specifier) {
						txSlot* slot = fxNewObject(the);
						slot = fxNextStringProperty(the, slot, ((txStringNode*)(specifier->from))->value, mxID(_from), XS_NO_FLAG);
						slot = fxNextIntegerProperty(the, slot, node->line, mxID(_line), XS_NO_FLAG);
						fxArrayCacheItem(the, the->stack + 1, the->stack);
						mxPop();
					}
					specifier = node->firstExportSpecifier;
					while (specifier) {
						if (specifier->asSymbol) {
							if (!c_strcmp(specifier->asSymbol->string, "default"))
								hasDefault = 1;
						}
						else if (specifier->symbol) {
							if (!c_strcmp(specifier->symbol->string, "default"))
								hasDefault = 1;
						}
						specifier = specifier->nextSpecifier;
					}
				}
				node = node->nextDeclareNode;
			}
			fxArrayCacheEnd(the, the->stack);
			mxPushSlot(mxResult);
			mxSetID(mxID(_from));
			mxPop();
			
			mxPush(mxArrayPrototype);
			fxNewArrayInstance(the);
			fxArrayCacheBegin(the, the->stack);
			{
				txGlobalPathWalker walker = { the, the->stack };
				Tool_prototype_walkGlobals(parser->root, &walker);
			}
			fxArrayCacheEnd(the, the->stack);
			mxPushSlot(mxResult);
			mxSetID(mxID(_global));
			mxPop();
			
			mxPushBoolean(hasDefault);
			mxPushSlot(mxResult);
			mxSetID(mxID(_default));
			mxPop();
			
			mxPushBoolean(isAsync);
			mxPushSlot(mxResult);
			mxSetID(fxID(the, "async"));
			mxPop();
		}
		else {
			txID id = fxID(the, "errorCount");
			txInteger c;
			mxPushSlot(mxThis);
			mxGetID(id);
			c = fxToInteger(the, the->stack);
			mxPop();
			c += parser->errorCount;
			mxPushInteger(c);
			mxPushSlot(mxThis);
			mxSetID(id);
			mxPop();
		}
	}
	if (file)
		fclose(file);
	fxTerminateParser(parser);
}
