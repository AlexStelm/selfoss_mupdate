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
#include <iconv.h>
#include <time.h>

#include "nxml.h"
#include "mrss.h"
#include "bb_md5_sha.h"


#define SELFOSS_VERSION		"2.7"
#define MY_VERSION		"0.1"

int __debug_level = 3;

#define IDSIZE			255

/* -*- content preparation -*- */

/* nxml internal, modifies tmp, return new str  */
char *__nxml_trim(char *);

static void trim_replace(char **field)
{
	char *p = __nxml_trim(*field);
	free(*field);
	*field = p;
}

static void iconv_replace(iconv_t cd, char **field)
{
	char *in, *out, *buf;
	size_t in_sz, out_sz;
	size_t _in_sz, _out_sz, ret_sz;

	if (cd == (iconv_t) -1 || *field == NULL)
		return;

	in_sz = _in_sz = strlen(*field);

	/* NOTE: optimization for cp1251/koi8-r -> utf-8 */
	out_sz = _out_sz = in_sz * 2;

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

	free(*field);
	*field = buf;
}

/* -*- Feed process -*- */

static size_t simplepie_get_id(mrss_t *rss, mrss_item_t *item, char *buf, size_t sz)
{
	if (item->guid != NULL) {
		strncpy(buf, item->guid, sz - 1);
		debug2("choose guid%s: %s", (item->guid_isPermaLink) ? " [permalink]" : "", item->guid);
	}
	else if (item->link != NULL) {
		strncpy(buf, item->link, sz - 1);
		debug2("choose link: %s", item->link);
	}
	else if (item->enclosure_url != NULL) {
		strncpy(buf, item->enclosure_url, sz - 1);
		debug2("choose encloseure url: %s", item->enclosure_url);
	}
	else if (item->title != NULL) {
		strncpy(buf, item->title, sz - 1);
		debug2("choose title: %s", item->title);
	}
	else {
		fprintf(stderr, "%s BUG\n", __func__);
		return 0;
	}

	return strnlen(buf, sz);
}

static size_t selfoss_getId(mrss_t *rss, mrss_item_t *item, char *buf_256)
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

static int fetch_feed(char *feed_url)
{
	mrss_t *rssdata;
	mrss_error_t mret;
	mrss_item_t *rssitem;
	CURLcode ccode;
	iconv_t iconv_cd;
	time_t item_time;
	struct tm item_tm;
	size_t n;

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
	if (rssdata->pubDate != NULL)
		strptime(rssdata->pubDate, "%a, %d %b %Y %H:%M:%S %z", &item_tm);

	debug("Generic:");
	debug("\tfile url: %s", rssdata->file);
	debug("\tencoding: %s", rssdata->encoding);
	debug2("\tsize: %zu", rssdata->size);
	debug2("\ttype: %d", rssdata->version);
	debug("Channel:");
	debug("\ttitle: %s", rssdata->title);
	debug("\tdescription: %s", rssdata->description);
	debug("\tlink: %s", rssdata->link);
	debug("\tpub date: %s", rssdata->pubDate);
	debug2("Image:");
	debug2("\ttitle: %s", rssdata->image_title);
	debug2("\tdescription: %s", rssdata->image_description);
	debug2("\turl: %s", rssdata->image_url);
	debug2("\tlink: %s", rssdata->image_link);
	debug2("\tW x H: %d x %d", rssdata->image_width, rssdata->image_height);

	debug("Items:");
	for (rssitem = rssdata->item, n = 0;
		rssitem != NULL;
		rssitem = rssitem->next, n++) {

		char uid_buf[IDSIZE + 1];
		int rc;

		iconv_replace(iconv_cd, &rssitem->title);
		iconv_replace(iconv_cd, &rssitem->description);
		iconv_replace(iconv_cd, &rssitem->link);
		iconv_replace(iconv_cd, &rssitem->guid);
		iconv_replace(iconv_cd, &rssitem->enclosure_url);

		debug("\tItem %zu:", n);
		debug("\t\ttitle: %s", rssitem->title);
		debug2("\t\tdescription: %s", rssitem->description);
		debug2("\t\tlink: %s", rssitem->link);
		debug2("\t\tguid: %s", rssitem->guid);
		debug2("\t\tenclosure_url: %s", rssitem->enclosure_url);
		debug("\t\tpub date: %s", rssitem->pubDate);

		selfoss_getId(rssdata, rssitem, uid_buf);
		/* check zero: SELECT count(*) FROM items WHERE uid=:uid */
		/* continue */

		if (rssitem->pubDate != NULL)
			strptime(rssitem->pubDate, "%a, %d %b %Y %H:%M:%S %z", &item_tm);

		sanitize_text_only(&rssitem->title);
		trim_replace(&rssitem->title);
		if (strlen(rssitem->title) < 2) {
			free(rssitem->title);
			rssitem->title = strdup("[ NO TITLE ]");
		}

		rc = sanitize_content(&rssitem->description);
		if (rc > 1)
			fprintf(stderr, "content sanitized with errors! item #%zu '%s' (rc=%d)",
					n, rssitem->title, rc);
		else if (rc >= 0)
			debug("sanitize ok #%zu '%s'", n, rssitem->title);
		else {
			fprintf(stderr, "content sanitization failed! skip item #%zu '%s'",
					n, rssitem->title);
			continue;
		}


	}

	iconv_close(iconv_cd);
	mrss_free(rssdata);

	return 0;
}

/* -*- Main -*- */

static void usage(char *argv0)
{
	fprintf(stderr, "Usage: %s <rss link>\n", argv0);
	exit(1);
}

static void version(char *argv0)
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

