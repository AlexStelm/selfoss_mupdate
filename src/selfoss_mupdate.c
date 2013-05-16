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
#include <unistd.h>

#include "nxml.h"
#include "mrss.h"
#include "tidy.h"
#include "bb_md5_sha.h"


#define PROGNAME		"selfoss_mupdate"
#define SELFOSS_VERSION		"2.7"
#define MY_VERSION		"0.1"

int __debug_level = 0;

#define IDSIZE			255
#define SPOUT0			"spouts\\rss\\feed"

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

static int fetch_feed(sqlite3 *db, int source_id, char *feed_url)
{
	mrss_t *rssdata;
	mrss_error_t mret;
	mrss_item_t *rssitem;
	CURLcode ccode;
	iconv_t iconv_cd;
	time_t item_time;
	struct tm item_tm;
	size_t n;
	int rc;

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

	debug ("Generic:");
	debug ("\tfile url: %s", rssdata->file);
	debug ("\tencoding: %s", rssdata->encoding);
	debug2("\tsize: %zu", rssdata->size);
	debug2("\ttype: %d", rssdata->version);
	debug ("Channel:");
	debug ("\ttitle: %s", rssdata->title);
	debug ("\tdescription: %s", rssdata->description);
	debug ("\tlink: %s", rssdata->link);
	debug ("\tpub date: %s", rssdata->pubDate);
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

		char *icon, *thumb;
		char uid_buf[IDSIZE + 1];
		bool exists;

		iconv_replace(iconv_cd, &rssitem->title);
		iconv_replace(iconv_cd, &rssitem->description);
		iconv_replace(iconv_cd, &rssitem->link);
		iconv_replace(iconv_cd, &rssitem->guid);
		iconv_replace(iconv_cd, &rssitem->enclosure_url);

		debug ("\tItem %zu:", n);
		debug ("\t\ttitle: %s", rssitem->title);
		debug2("\t\tdescription: %s", rssitem->description);
		debug2("\t\tlink: %s", rssitem->link);
		debug2("\t\tguid: %s", rssitem->guid);
		debug2("\t\tenclosure_url: %s", rssitem->enclosure_url);
		debug ("\t\tpub date: %s", rssitem->pubDate);

		selfoss_getId(rssdata, rssitem, uid_buf);
		rc = db_item_exists(db, uid_buf, &exists);
		if (rc != SQLITE_OK)
			errx(1, "sqlite fail");
		if (exists) {
			debug("item alredy exists. skipped");
			continue;
		}

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

		/* TODO: try download favicon.ico
		 * req. libgd or so .ico -> md5(url).png 30x30 */
		icon = NULL;

		/* TODO: try find thumb in enclosure
		 * req. same as for icon */
		thumb = NULL;

		rc = db_item_add(db, source_id,
				rssitem->title, rssitem->description, uid_buf, rssitem->link,
				thumb, icon, &item_tm);
		if (rc != SQLITE_OK)
			errx(1, "failed to add new item (title: %s) to source %d",
					rssitem->title, source_id);
	}

	rc = db_source_set_lastupdate(db, source_id, 0);
	if (rc != SQLITE_OK)
		errx(1, "db_source_update(db, %d, 0) NOT OK", source_id);

	iconv_close(iconv_cd);
	mrss_free(rssdata);

	return 0;
}

/* -*- Main -*- */

static void usage(FILE *fl, int ex)
{
	fprintf(fl, "Usage: %s [-dVh] [-s <source id>] <selfoss.sqlite.db> [<feed url>]\n", PROGNAME);
	fprintf(fl, "\n");
	fprintf(fl, "\t-s <source id>\tprocess only one source (required for <feed url>)\n");
	fprintf(fl, "\t-d\t\tdebug level (-ddd maximum)\n");
	fprintf(fl, "\t-h\t\tthis help\n");
	fprintf(fl, "\t-V\t\tversion info\n");
	exit(ex);
}

static void version()
{
	fprintf(stdout, "%s %s\n", PROGNAME, MY_VERSION);
	fprintf(stdout, "Compatible with selfoss version: %s\n\n", SELFOSS_VERSION);
	fprintf(stdout, "libNXML version: %s\n", LIBNXML_VERSION_STRING);
	fprintf(stdout, "libMRSS version: %s\n", LIBMRSS_VERSION_STRING);
	fprintf(stdout, "HTML Tidy version: %s\n", tidyReleaseDate());
}

int main(int argc, char *argv[])
{
	int opt, rc, fetch_rc = 1;
	sqlite3 *db;
	sqlite3_stmt *stmt;
	int source_id = -1;
	char *feed_url = NULL;
	bool single_source = false;

	while ((opt = getopt(argc, argv, "dVhs:")) != -1) {
		switch (opt) {
			case 'd':
				__debug_level += 1;
				break;

			case 's':
				source_id = atoi(optarg);
				single_source = true;
				break;

			case 'V':
				version();
				return 0;

			case 'h':
				usage(stdout, 0);

			default:
				usage(stderr, 1);
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "Expected database file\n");
		usage(stderr, 1);
	}

	if (optind + 1 <= argc)
		feed_url = argv[optind + 1];

	if (feed_url != NULL && !single_source)
		errx(1, "with <feed url> key -s required");

	rc = sqlite3_open(argv[optind + 0], &db);
	if (rc) {
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return 1;
	}

	if (single_source)
		rc = db_source_get_stmt(db, source_id, &stmt);
	else
		rc = db_source_get_all_by_lastupdate_stmt(db, &stmt);

	while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
		const char *title, *tags, *spout, *param, *error;

		db_source_stmt_to_data(stmt, &source_id, &title, &tags, &spout, &param, &error);

		debug("source #%d title: %s tags: %s spout: %s param: %s erorr: %s",
				source_id, title, tags, spout, param, error);

		if (strcmp(SPOUT0, spout) != 0) {
			debug("unsupported spout, skipped");
			continue;
		}

		if (!single_source)
			errx(1, "TODO fetch url");

		fetch_rc = fetch_feed(db, source_id, feed_url);

		/* db_source_get_stmt return one row, no break */
	}

	sqlite3_finalize(stmt);

	if (rc != SQLITE_DONE) {
		errx(1, "SQL error: %s %d", sqlite3_errmsg(db), rc);
	}

	sqlite3_close(db);

	return fetch_rc;
}

