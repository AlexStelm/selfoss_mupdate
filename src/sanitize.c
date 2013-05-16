/**
 * Selfoss RSS reader micro updater
 *
 *   Copyright (C) 2013 Vladimir Ermakov <vooon341@gmail.com>
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "selfoss_mupdate.h"
#include "tidy.h"
#include "buffio.h"

/* -*- tidy internal data manipulation. be carefull -*- */
#include "tidy-int.h"

/* -*- private -*- */

static int configure_tidy(TidyDoc tdoc, TidyBuffer *out, TidyBuffer *err)
{
	int rc;

	tidyBufInit(out);
	tidyBufInit(err);

	rc = tidyOptSetBool(tdoc, TidyXhtmlOut, yes);
	if (rc >= 0)
		rc = tidyOptSetInt(tdoc, TidyBodyOnly, yes);
	if (rc >= 0)
		rc = tidyOptSetBool(tdoc, TidyOmitOptionalTags, yes);
	if (rc >= 0)
		rc = tidyOptSetBool(tdoc, TidyMakeClean, yes);
	if (rc >= 0)
		rc = tidyOptSetBool(tdoc, TidyDropFontTags, yes);
	if (rc >= 0)
		rc = tidyOptSetBool(tdoc, TidyDropPropAttrs, yes);
	if (rc >= 0)
		rc = tidySetErrorBuffer(tdoc, err);
	if (rc >= 0)
		rc = tidySetCharEncoding(tdoc, "utf8");

	return rc;
}

static void dump_node(TidyNode tnod, int indent)
{
	TidyNode child;

	for (child = tidyGetChild(tnod);
		child;
		child = tidyGetNext(child)) {

		ctmbstr name;

		switch (tidyNodeGetType(child)) {
			case TidyNode_Root:       name = "*Root";                    break;
			case TidyNode_DocType:    name = "*DOCTYPE";                 break;
			case TidyNode_Comment:    name = "*Comment";                 break;
			case TidyNode_ProcIns:    name = "*Processing Instruction";  break;
			case TidyNode_Text:       name = "*Text";                    break;
			case TidyNode_CDATA:      name = "*CDATA";                   break;
			case TidyNode_Section:    name = "*XML Section";             break;
			case TidyNode_Asp:        name = "*ASP";                     break;
			case TidyNode_Jste:       name = "*JSTE";                    break;
			case TidyNode_Php:        name = "*PHP";                     break;
			case TidyNode_XmlDecl:    name = "*XML Declaration";         break;

			case TidyNode_Start:
			case TidyNode_End:
			case TidyNode_StartEnd:
			default:
						  name = tidyNodeGetName( child );
						  break;
		}

		assert(name != NULL);
		debug("%*.*sNode: %s", indent, indent, " ", name);
		dump_node(child, indent + 4);
	}
}

static inline bool is_acceptable_content_tag(TidyTagId tid)
{
	/* tag list defined at selfoss/helpers/ContentLoader.php 179
	 * in function sanitizeContent() */
	return (
			tid == TidyTag_DIV	||
			tid == TidyTag_P	||
			tid == TidyTag_UL	||
			tid == TidyTag_LI	||
			tid == TidyTag_A	||
			tid == TidyTag_IMG	||
			tid == TidyTag_DL	||
			tid == TidyTag_DT	||
			tid == TidyTag_H1	||
			tid == TidyTag_H2	||
			tid == TidyTag_H3	||
			tid == TidyTag_H4	||
			tid == TidyTag_H5	||
			tid == TidyTag_H6	||
			tid == TidyTag_OL	||
			tid == TidyTag_BR	||
			tid == TidyTag_TABLE	||
			tid == TidyTag_TR	||
			tid == TidyTag_TD	||
			tid == TidyTag_BLOCKQUOTE	||
			tid == TidyTag_PRE	||
			tid == TidyTag_INS	||
			tid == TidyTag_DEL	||
			tid == TidyTag_TH	||
			tid == TidyTag_THEAD	||
			tid == TidyTag_TBODY	||
			tid == TidyTag_B	||
			tid == TidyTag_I	||
			tid == TidyTag_STRONG	||
			tid == TidyTag_EM	||
			tid == TidyTag_TT
	       );
}

static inline bool is_acceptable_content_attr(AttVal *attr)
{
	/* attrib list defined at selfoss/helpers/ContentLoader.php 175
	 * in function sanitizeContent() */
	return (
			attrIsALT(attr)		||
			attrIsTITLE(attr)	||
			attrIsSRC(attr)		||
			attrIsNAME(attr)	||
			attrIsREL(attr)	||
			attrIsHREF(attr)
	       );
}

static void sanitize_attributes(TidyDocImpl* doc, Node* node)
{
	AttVal *attr, *next, *prev = NULL;
	bool bad_href = false;

	for (attr = node->attributes; attr; attr = next) {
		next = attr->next;

		/* deny href="javascript: alert('powned')" */
		bad_href = ( attrIsHREF(attr) && attr->value &&
				!strncmp("javascript", attr->value , 10) );

		if (bad_href || !is_acceptable_content_attr(attr) ) {
			if (prev)
				prev->next = next;
			else
				node->attributes = next;

			debug2("drop %s attr %s%s",
					tidyNodeGetName(tidyImplToNode(node)),
					tidyAttrName(tidyImplToAttr(attr)),
					(bad_href) ? " [bad proto]" : "");

			TY_(FreeAttribute)(doc, attr);
		}
		else
			prev = attr;
	}
}

static void walk_and_remove(TidyDoc tdoc, TidyNode tnod)
{
	TidyNode child, next_child;
	TidyTagId tid;
	TidyNodeType nt;

	for (child = tidyGetChild(tnod);
			child;
			child = next_child) {
		next_child = tidyGetNext(child);

		nt = tidyNodeGetType(child);
		if (nt == TidyNode_Start ||
			nt == TidyNode_End ||
			nt == TidyNode_StartEnd) {

			Node *np = tidyNodeToImpl(child);

			tid = tidyNodeGetId(child);
			if (!is_acceptable_content_tag(tid)) {
				/* remove subtree */
				debug2("drop node %s", tidyNodeGetName(child));

				/* fix list pointers */
				if (np->prev) np->prev->next = np->next;
				if (np->next) np->next->prev = np->prev;
				if (np->parent && np->parent->content == np)
					np->parent->content = np->next;
				np->next = NULL;

				TY_(FreeNode)(tidyDocToImpl(tdoc), np);

				continue; /* prevent access to fereed data */
			}
			else {
				/* acceptable tag, now remove bad attributes */
				sanitize_attributes(tidyDocToImpl(tdoc), np);
			}
		}

		walk_and_remove(tdoc, child);
	}
}

static int sanitize_nodes(TidyDoc tdoc)
{
	int rc;
	TidyNode body = tidyGetBody(tdoc);

	debug3("before:\n----------------------------------------");
	if (__debug_level > 2) dump_node(body, 0);
	debug3("----------------------------------------");

	walk_and_remove(tdoc, body);

	debug3("after:\n----------------------------------------");
	if (__debug_level > 2) dump_node(body, 0);
	debug3("----------------------------------------");

	return 0;
}

/* -*- public -*- */

enum pstate {
	ST_IN,
	ST_OUT
};

/* adopted version from
 * http://c.happycodings.com/Small_Programs/code28.html */
void sanitize_text_only(char **field)
{
	enum pstate state = ST_OUT;
	enum pstate tstate = ST_OUT;
	char tagbuff[2048];
	char *tagp = NULL;
	char *rdp, *wrp, *ob;
	size_t ob_sz;

	if (!(field && *field)) return;

	ob_sz = strlen(*field);
	ob = calloc(ob_sz + 1, 1);
	if (ob == NULL)
		err(1, "out of memory!");

	tagp = tagbuff;
	rdp = *field;
	wrp = ob;

	for (size_t n = ob_sz; rdp && n; rdp++, n--) {
		/* copy tag into tagbuff */
		if (*rdp == '<' || *rdp == '&') state = ST_IN;
		if (state == ST_IN) *tagp++ = *rdp;
		if (*rdp == '>' || *rdp == ';') {
			state = ST_OUT; *tagp++ = '\0';

			/* search tagbuff, javascript, style tags */
			if (strstr(tagbuff, "<s") != 0 || strstr(tagbuff, "<S") != 0)
				tstate = ST_IN;
			if (strstr(tagbuff, "</") != 0)
				tstate = ST_OUT;

			/*   ? */
			if (strstr(tagbuff, "nbsp") != 0 || strstr(tagbuff, "NBSP") != 0)
				*wrp++ = ' ';

			tagp = tagbuff;
		} /* end if */

		/* not in a tag, print character */
		if (state == ST_OUT && tstate == ST_OUT && *rdp != '>' && *rdp != ';')
			*wrp++ = *rdp;
	}

	/* ob null terminated by calloc() */

	debug3("source: len=%zu '%s'", ob_sz, *field);
	debug3("result: len=%zu '%s'", strlen(ob), ob);

	free(*field);
	*field = ob;
}

int sanitize_content(char **content)
{
	int rc;
	TidyDoc tdoc;
	TidyBuffer errbuf;
	TidyBuffer outbuf;

	/* tidy doc */
	tdoc = tidyCreate();
	rc = configure_tidy(tdoc, &outbuf, &errbuf);
	if (rc >= 0)
		rc = tidyParseString(tdoc, *content);
	if (rc >= 0)
		rc = tidyCleanAndRepair(tdoc);
	if (rc >= 0)
		rc = sanitize_nodes(tdoc);
	if (rc >= 0)
		rc = tidyRunDiagnostics(tdoc);
	if (rc > 1) {
		/* warnings */
		debug("ugh errors! force output. rc=%d", rc);
		rc = tidyOptSetBool(tdoc, TidyForceOutput, yes) ? rc : -1;
	}
	if (rc >= 0)
		rc = tidySaveBuffer(tdoc, &outbuf);

	if (rc >= 0) {
		if (rc > 0)
			debug2("errbuf len=%u:\n"
				"--------------------------------\n"
				"%s"
				"--------------------------------",
				errbuf.size, errbuf.bp);

		debug3("source: len=%zu\n"
			"--------------------------------\n"
			"%s\n"
			"--------------------------------",
			strlen(*content), *content);
		debug3("result: len=%zu (sz=%u)\n"
			"--------------------------------\n"
			"%s\n"
			"--------------------------------",
			strlen(outbuf.bp), outbuf.size, outbuf.bp);

		char *p = *content;
		*content = outbuf.bp;
		outbuf.bp = p;
	}

	tidyBufFree(&errbuf);
	tidyBufFree(&outbuf);
	tidyRelease(tdoc);

	return rc;
}

