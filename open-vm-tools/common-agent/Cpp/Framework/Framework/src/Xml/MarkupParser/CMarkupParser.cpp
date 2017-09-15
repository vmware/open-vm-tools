/*
 *	 Author: mdonahue
 *  Created: Jan 20, 2011
 *
 *	Copyright (C) 2011-2016 VMware, Inc.  All rights reserved. -- VMware Confidential
 */

#include "stdafx.h"
#include "Xml/MarkupParser/CMarkupParser.h"
#include "Exception/CCafException.h"
#include <deque>
#include <algorithm>

namespace Caf { namespace MarkupParser {

std::string _xml;

struct SParserState {
	SParserState() :
		depth(0) {
	}

	SmartPtrElement root;
	std::deque<SmartPtrElement> stack;
	uint32 depth;

private:
	CAF_CM_DECLARE_NOCOPY(SParserState);
};

void cb_start_element(GMarkupParseContext *context,
					  const gchar *element_name,
					  const gchar **attribute_names,
					  const gchar **attribute_values,
					  gpointer user_data,
					  GError **error) {
	CAF_CM_STATIC_FUNC("MarkupParser", "cb_start_element");

	try {
		SParserState& state = *(reinterpret_cast<SParserState*>(user_data));
		CAF_CM_ASSERT(state.depth == state.stack.size());
		if (state.depth == 0) {
			//TODO-BLW: For some reason the root is already created, so temporarily
			// remove the assert and re-create the root.
			//CAF_CM_ASSERT(!state.root);
			state.root.CreateInstance();
			state.stack.push_back(state.root);
		}
		else {
			SmartPtrElement element;
			element.CreateInstance();
			state.stack.back()->children.push_back(element);
			state.stack.push_back(element);
		}

		++state.depth;
		SmartPtrElement element = state.stack.back();
		element->name = element_name;

		const gchar **attr_name_cursor = attribute_names;
		const gchar **attr_value_cursor = attribute_values;
		while (*attr_name_cursor) {
			element->attributes.push_back(Attribute(*attr_name_cursor, *attr_value_cursor));
			++attr_name_cursor;
			++attr_value_cursor;
		}
	}
	catch(CCafException *e) {
		std::string msg(e->getFullMsg());
		e->Release();
		*error = g_error_new_literal(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, msg.c_str());
	}
}

void cb_text(GMarkupParseContext *context,
			 const gchar *text,
			 gsize text_len,
			 gpointer user_data,
			 GError **error) {
	SParserState& state = *(reinterpret_cast<SParserState*>(user_data));
	if (text_len) {
		state.stack.back()->value.append(text, text_len);
	}
}

void cb_end_element(GMarkupParseContext *context,
					const gchar *element_name,
					gpointer user_data,
					GError **error) {
	CAF_CM_STATIC_FUNC("MarkupParser", "cb_end_element");

	try {
		SParserState& state = *(reinterpret_cast<SParserState*>(user_data));
		CAF_CM_ASSERT((state.depth) && (state.depth == state.stack.size()));
		--state.depth;
		state.stack.pop_back();
	}
	catch(CCafException *e) {
		std::string msg(e->getFullMsg());
		e->Release();
		*error = g_error_new_literal(G_MARKUP_ERROR, G_MARKUP_ERROR_INVALID_CONTENT, msg.c_str());
	}
}

void cb_destroy_user_data(gpointer data) {
	delete (SParserState*)data;
}

static GMarkupParser _markupParser = { cb_start_element,
									   cb_end_element,
									   cb_text,
									   NULL,
									   NULL };

SmartPtrElement parseString(const std::string& xml) {
	CAF_CM_STATIC_FUNC("MarkupParser", "parseString");
	CAF_CM_VALIDATE_STRINGPTRA(xml.c_str());

	_xml = xml;

	SParserState *parserState = new SParserState();
	GError *parserError = NULL;
	GMarkupParseContext *context =
			g_markup_parse_context_new(&_markupParser,
									   G_MARKUP_TREAT_CDATA_AS_TEXT,
									   parserState,
									   cb_destroy_user_data);

	SmartPtrElement root;
	try {
		if (g_markup_parse_context_parse(context,
										 xml.c_str(),
										 xml.length(),
										 &parserError)) {
			root = parserState->root;
		}
		else {
			CAF_CM_EXCEPTION_VA0(parserError->code, parserError->message);
		}
	}
	catch (...) {
		if (parserError) {
			g_error_free(parserError);
		}

		if (context) {
			g_markup_parse_context_free(context);
		}
		throw;
	}

	if (parserError) {
		g_error_free(parserError);
	}

	if (context) {
		g_markup_parse_context_free(context);
	}

	return root;
}

SmartPtrElement parseFile(const std::string& file) {
	CAF_CM_STATIC_FUNC("MarkupParser", "parseFile");
	CAF_CM_VALIDATE_STRINGPTRA(file.c_str());

	gchar* text = NULL;
	GError *fileError = NULL;
	SmartPtrElement root;
	try {
		if (g_file_get_contents(file.c_str(), &text, NULL, &fileError)) {
		    if (! text || (text[ 0 ] == L'\0' )) {
				CAF_CM_EXCEPTION_VA1(ERROR_INVALID_DATA, "File is empty - %s", file.c_str());
		    }
			root = parseString(text);
		}
		else {
			CAF_CM_EXCEPTION_VA0(fileError->code, fileError->message);
		}
	}
	catch (...) {
		if (fileError) {
			g_error_free(fileError);
		}

		if (text) {
			g_free(text);
		}
		throw;
	}

	if (fileError) {
		g_error_free(fileError);
	}

	if (text) {
		g_free(text);
	}

	return root;
}

ChildIterator findChild(SmartPtrElement& element, const std::string& name) {
	CAF_CM_STATIC_FUNC_VALIDATE("MarkupParser", "findChild");
	CAF_CM_VALIDATE_SMARTPTR(element);
	CAF_CM_VALIDATE_STRING(name);

	ChildIterator rc = element->children.end();
	for(ChildIterator childIter = element->children.begin();
		childIter != element->children.end();
		childIter++) {
		if((*childIter)->name.compare(name) == 0) {
			rc = childIter;
		}
	}
	return rc;
/*
	return std::find_if(element->children.begin(),
						element->children.end(),
						std::bind2nd(ElementName(), name));
*/
}

AttributeIterator findAttribute(Attributes& attributes, const std::string& name) {
	CAF_CM_STATIC_FUNC_VALIDATE("MarkupParser", "findAttribute");
	CAF_CM_VALIDATE_STRING(name);
	return std::find_if(attributes.begin(),
						attributes.end(),
						std::bind2nd(AttributeName(), name));
}

std::string getAttributeValue(SmartPtrElement& element, const std::string& name) {
	CAF_CM_STATIC_FUNC("MarkupParser", "getAttributeValue");
	CAF_CM_VALIDATE_SMARTPTR(element);
	CAF_CM_VALIDATE_STRING(name);
	std::string rc;

	AttributeIterator iter = findAttribute(element->attributes, name);
	if (iter != element->attributes.end()) {
		rc = iter->second;
	} else {
		CAF_CM_EXCEPTION_VA2(ERROR_TAG_NOT_FOUND,
							 "Element %s does not contain attribute %s",
							 element->name.c_str(),
							 name.c_str());
	}
	return rc;
}

}}
