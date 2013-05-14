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

#include "nxml.h"
#include "mrss.h"
#include "tidy.h"

#define SELFOSS_VERSION		"2.7"
#define MY_VERSION		"0.1"

bool __debug_enable = true;
#define debug(fmt, ...)		if (__debug_enable) fprintf(stderr, "%s: " fmt "\n", __func__, ##__VA_ARGS__)

#define IDSIZE			255

/* -*- Feed reader -*- */

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

	debug("1: in_sz=%zu, out_sz=%zu", in_sz, out_sz);

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

	debug("2. in_sz=%zu, out_sz=%zu, ret_sz=%zu, out/in=%zu",
			in_sz, out_sz, ret_sz, out_sz / in_sz);

	free(*field);
	*field = buf;
}

size_t simplepie_get_id(mrss_t *rss, mrss_item_t *item, char *buf, size_t sz)
{

}

void selfoss_getId(mrss_t *rss, mrss_item_t *item, char buf[IDSIZE + 1])
{
	char sp_buf[4096];
	size_t sz;

	sz = simplepie_get_id(rss, item, sp_buf, sizeof(sp_buf));
	if (sz > IDSIZE) {
		/* do md5 hex digest sp_buf > md5 */
	}
	else {
		memcpy(buf, sp_buf, sz);
		buf[sz] = '\0';
	}
}

int parse_feed(char *feed_url)
{
	mrss_t *rssdata;
	mrss_error_t mret;
	mrss_item_t *rssitem;
	CURLcode ccode;
	iconv_t iconv_cd;

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

	debug("Generic:");
	debug("\tfile url: %s", rssdata->file);
	debug("\tencoding: %s", rssdata->encoding);
	debug("\tsize: %zu", rssdata->size);
	debug("\ttype: %d", rssdata->version);
	debug("Channel:");
	debug("\ttitle: %s", rssdata->title);
	debug("\tdescription: %s", rssdata->description);
	debug("\tlink: %s", rssdata->link);
	debug("Image:");
	debug("\ttitle: %s", rssdata->image_title);
	debug("\tdescription: %s", rssdata->image_description);
	debug("\turl: %s", rssdata->image_url);
	debug("\tlink: %s", rssdata->image_link);
	debug("\tW x H: %d x %d", rssdata->image_width, rssdata->image_height);

	debug("Items:");
	rssitem = rssdata->item;
	while (rssitem) {
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

		rssitem = rssitem->next;
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

	parse_feed(argv[1]);

	return 0;
}

