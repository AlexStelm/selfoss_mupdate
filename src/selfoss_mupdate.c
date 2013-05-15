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

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <iconv.h>
#include <errno.h>
#include <err.h>
#include <time.h>

#include "nxml.h"
#include "mrss.h"
#include "tidy.h"
#include "buffio.h"
#include "bb_md5_sha.h"

#define SELFOSS_VERSION		"2.7"
#define MY_VERSION		"0.1"

bool __debug_enable = true;
#define debug(fmt, ...)		if (__debug_enable) fprintf(stderr, "%s: " fmt "\n", __func__, ##__VA_ARGS__)
#define debug2(fmt, ...)
#define debug3(fmt, ...)

#define IDSIZE			255

/* -*- content preparation -*- */

void iconv_replace(iconv_t cd, char **field)
{
	char *in, *out, *buf;
	size_t in_sz, out_sz;
	size_t _in_sz, _out_sz, ret_sz;

	if (cd == (iconv_t) -1 || *field == NULL)
		return;

	in_sz = _in_sz = strlen(*field);

	/* NOTE: optimization for cp1251/koi8-r -> utf-8 */
	//out_sz = _out_sz = (in_sz <= PAGE/2) ? PAGE : in_sz * 2;
	out_sz = _out_sz = in_sz * 2;

	debug3("1: in_sz=%zu, out_sz=%zu", in_sz, out_sz);

	if ((buf = calloc(out_sz, 1)) == NULL)
		err(1, "out of memory\n");

	in = *field;
	out = buf;

	ret_sz = iconv(cd, &in, &_in_sz, &out, &_out_sz);
	if (ret_sz == -1) {
		if (errno == E2BIG) {
			debug("E2BIG. in_sz=%zu", _in_sz);
		}

		err(1, "iconv()");
	}

	debug3("2. in_sz=%zu, out_sz=%zu, ret_sz=%zu, out/in=%zu",
			in_sz, out_sz, ret_sz, out_sz / in_sz);

	free(*field);
	*field = buf;
}

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
	tidyBufInit(&errbuf);
	tidyBufInit(&outbuf);

	rc = tidyOptSetBool(tdoc, TidyXhtmlOut, no);
	if (rc >= 0)
		rc = tidySetErrorBuffer(tdoc, &errbuf);
	if (rc >= 0)
		rc = tidySetCharEncoding(tdoc, "utf8");
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
			debug("errbuf: %s", errbuf.bp);

		debug("source: len=%d\n%s", strlen(*content), *content);
		debug("-------------------------------");
		debug("result: len=%d (sz=%d)\n%s", strlen(outbuf.bp), outbuf.size, outbuf.bp);
		debug("-------------------------------");

		char *p = *content;
		*content = outbuf.bp;
		outbuf.bp = p;
	}

	tidyBufFree(&errbuf);
	tidyBufFree(&outbuf);
	tidyRelease(tdoc);

	return rc;
}

/* -*- Feed process -*- */

size_t simplepie_get_id(mrss_t *rss, mrss_item_t *item, char *buf, size_t sz)
{
	if (item->guid != NULL) {
		strncpy(buf, item->guid, sz - 1);
		debug("choose guid%s: %s", (item->guid_isPermaLink) ? " [permalink]" : "", item->guid);
	}
	else if (item->link != NULL) {
		strncpy(buf, item->link, sz - 1);
		debug("choose link: %s", item->link);
	}
	else if (item->enclosure_url != NULL) {
		strncpy(buf, item->enclosure_url, sz - 1);
		debug("choose encloseure url: %s", item->enclosure_url);
	}
	else if (item->title != NULL) {
		strncpy(buf, item->title, sz - 1);
		debug("choose title: %s", item->title);
	}
	else {
		debug("BUG!");
		return 0;
	}

	return strnlen(buf, sz);
}

size_t selfoss_getId(mrss_t *rss, mrss_item_t *item, char *buf_256)
{
	char sp_buf[4096];
	size_t sz;

	sz = simplepie_get_id(rss, item, sp_buf, sizeof(sp_buf));
	if (sz > IDSIZE) {
		md5_ctx_t ctx;
		char digest[16];

		md5_begin(&ctx);
		md5_hash(&ctx, sp_buf, sz);
		md5_end(&ctx, &digest);

		sz = snprintf(buf_256, IDSIZE + 1,
				"%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
				digest[0], digest[1], digest[2], digest[3],
				digest[4], digest[5], digest[6], digest[7],
				digest[8], digest[9], digest[10], digest[11],
				digest[12], digest[13], digest[14], digest[15]);
	}
	else {
		memcpy(buf_256, sp_buf, sz);
		buf_256[sz] = '\0';
	}

	debug("sz=%zu, id: %s", sz, buf_256);

	return sz;
}

int fetch_feed(char *feed_url)
{
	mrss_t *rssdata;
	mrss_error_t mret;
	mrss_item_t *rssitem;
	CURLcode ccode;
	iconv_t iconv_cd;
	time_t item_time;
	struct tm item_tm;

	if (!strncmp(feed_url, "http://", 7) || !strncmp(feed_url, "https://", 8))
		mret = mrss_parse_url_with_options_and_error(feed_url, &rssdata, NULL, &ccode);
	else
		mret = mrss_parse_file(feed_url, &rssdata);

	if (mret) {
		fprintf(stderr, "MRSS Error: %s\n",
			(mret == MRSS_ERR_DOWNLOAD) ?
				mrss_curl_strerror(ccode) :
				mrss_strerror(mret));
		return 1;
	}

	if (rssdata->encoding != NULL && \
			strcasecmp(rssdata->encoding, "utf-8") != 0) {

		iconv_cd = iconv_open("utf-8", rssdata->encoding);
		if (iconv_cd == (iconv_t) -1) {
			fprintf(stderr, "iconv_open(utf-8, %s): %s\n",
				rssdata->encoding, strerror(errno));
			return 1;
		}

		debug("Iconv hack enabled");
	}
	else
		iconv_cd = (iconv_t) -1;

	iconv_replace(iconv_cd, &rssdata->title);
	iconv_replace(iconv_cd, &rssdata->description);
	iconv_replace(iconv_cd, &rssdata->link);
	iconv_replace(iconv_cd, &rssdata->image_title);
	iconv_replace(iconv_cd, &rssdata->image_url);
	iconv_replace(iconv_cd, &rssdata->image_link);
	iconv_replace(iconv_cd, &rssdata->image_description);

	item_time = time(NULL);
	gmtime_r(&item_time, &item_tm);
	if (rssdata->pubDate != NULL) {
		strptime(rssdata->pubDate, "%a, %d %b %Y %H:%M:%S %z", &item_tm);
	}

	debug("Generic:");
	debug("\tfile url: %s", rssdata->file);
	debug("\tencoding: %s", rssdata->encoding);
	debug("\tsize: %zu", rssdata->size);
	debug("\ttype: %d", rssdata->version);
	debug("Channel:");
	debug("\ttitle: %s", rssdata->title);
	debug("\tdescription: %s", rssdata->description);
	debug("\tlink: %s", rssdata->link);
	debug("\tpub date: %s", rssdata->pubDate);
	debug("Image:");
	debug("\ttitle: %s", rssdata->image_title);
	debug("\tdescription: %s", rssdata->image_description);
	debug("\turl: %s", rssdata->image_url);
	debug("\tlink: %s", rssdata->image_link);
	debug("\tW x H: %d x %d", rssdata->image_width, rssdata->image_height);

	debug("Items:");
	for (rssitem = rssdata->item; rssitem != NULL; rssitem = rssitem->next) {
		char uid_buf[IDSIZE + 1];
		int rc;

		iconv_replace(iconv_cd, &rssitem->title);
		iconv_replace(iconv_cd, &rssitem->description);
		iconv_replace(iconv_cd, &rssitem->link);
		iconv_replace(iconv_cd, &rssitem->guid);
		iconv_replace(iconv_cd, &rssitem->enclosure_url);

		debug("\tItem %p:", rssitem);
		debug("\t\ttitle: %s", rssitem->title);
		debug("\t\tdescription: %s", rssitem->description);
		debug("\t\tlink: %s", rssitem->link);
		debug("\t\tguid: %s", rssitem->guid);
		debug("\t\tenclosure_url: %s", rssitem->enclosure_url);
		debug("\t\tpub date: %s", rssitem->pubDate);

		selfoss_getId(rssdata, rssitem, uid_buf);
		/* check zero: SELECT count(*) FROM items WHERE uid=:uid */
		/* continue */

		/* prepare to insert */

		if (rssitem->pubDate != NULL) {
			strptime(rssitem->pubDate, "%a, %d %b %Y %H:%M:%S %z", &item_tm);
		}

		rc = sanitize_content(&rssitem->description);

		if (rc > 1)
			debug("sanitize with errors (rc=%d)", rc);
		else if (rc >= 0)
			debug("sanitize ok");
		else
			debug("danitize fail!");


	}

	iconv_close(iconv_cd);
	mrss_free(rssdata);

	return 0;
}

/* -*- Main -*- */

void usage(char *argv0)
{
	fprintf(stderr, "Usage: %s <rss link>\n", argv0);
	exit(1);
}

void version(char *argv0)
{
	fprintf(stdout, "%s %s\n", argv0, MY_VERSION);
	fprintf(stdout, "Compatible with selfoss version: %s\n\n", SELFOSS_VERSION);
	fprintf(stdout, "libNXML version: %s\n", LIBNXML_VERSION_STRING);
	fprintf(stdout, "libMRSS version: %s\n", LIBMRSS_VERSION_STRING);
	fprintf(stdout, "HTML Tidy version: %s\n", "TODO");
}

int main(int argc, char *argv[])
{
	if (argc != 2) usage(argv[0]);
	//version(argv[0]);

	fetch_feed(argv[1]);

	return 0;
}

