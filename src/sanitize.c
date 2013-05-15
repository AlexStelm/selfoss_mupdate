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

static int walk_and_remove(TidyDoc tdoc, TidyNode tnod)
{
	TidyNode child;
	TidyTagId tid;
	TidyNodeType nt;

	for (child = tidyGetChild(tnod);
			child;
			child = tidyGetNext(child)) {

		nt = tidyNodeGetType(child);
		if (nt == TidyNode_Start ||
			nt == TidyNode_End ||
			nt == TidyNode_StartEnd) {

			tid = tidyNodeGetId(child);
			if (tid == TidyTag_SCRIPT) {
				/* remove subtree */
				debug("drop %s", tidyNodeGetName(child));

				Node *p = tidyNodeToImpl(child);

				if (p->prev) p->prev->next = p->next;
				if (p->next) p->next->prev = p->prev;
				if (p->parent && p->parent->content == p)
					p->parent->content = p->next;
				p->next = NULL;

				TY_(FreeNode)(tidyDocToImpl(tdoc), p);
			}
		}

		walk_and_remove(tdoc, child);
	}

	return 0;
}

static int sanitize_nodes(TidyDoc tdoc)
{
	int rc;
	TidyNode body = tidyGetBody(tdoc);

	debug("before:\n----------------------------------------");
	dump_node(body, 0);
	debug("----------------------------------------");

	rc = walk_and_remove(tdoc, body);

	debug("after:\n----------------------------------------");
	dump_node(body, 0);
	debug("----------------------------------------");

	return rc;
}

/* -*- public -*- */

void sanitize_drop(char **field)
{
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
			debug("errbuf len=%u:\n"
				"--------------------------------\n"
				"%s"
				"--------------------------------",
				errbuf.size, errbuf.bp);

		debug("source: len=%zu\n"
			"--------------------------------\n"
			"%s\n"
			"--------------------------------",
			strlen(*content), *content);
		debug("result: len=%zu (sz=%u)\n"
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

