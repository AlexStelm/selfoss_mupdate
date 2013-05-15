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

/* -*- private -*- */

static int configure_tidy(TidyDoc tdoc, TidyBuffer *err)
{
	int rc;

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

static int node_walk(TidyDoc tdoc)
{
	TidyNode node;

	return 0;
}

/* -*- public -*- */

void sanitize_drop(char **field)
{
}

int sanitize_content(char **content)
{
	int rc;
	TidyDoc tdoc;
	TidyBuffer errbuf = {0};
	TidyBuffer outbuf = {0};

	/* tidy doc */
	tdoc = tidyCreate();
	rc = configure_tidy(tdoc, &errbuf);
	if (rc >= 0)
		rc = tidyParseString(tdoc, *content);
	if (rc >= 0)
		rc = tidyCleanAndRepair(tdoc);
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

