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

#include <stdlib.h>
#include <stdio.h>
#include <nxml.h>
#include <mrss.h>
#include <iconv.h>


void usage()
{
	fprintf(stderr, "Usage: selfoss_mupdate <rss link>\n");
	exit(1);
}

int parse_feed(char *feed_url)
{
	mrss_t *rssdata;
	mrss_error_t mret;
	CURLcode ccode;

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

	printf("file: %s\n", rssdata->file);
	printf("enc: %s\n", rssdata->encoding);
	printf("size: %zu\n", rssdata->size);
	printf("type: %d\n", rssdata->version);
	printf("title: %s\n", rssdata->title);
	printf("description: %s\n", rssdata->description);

	mrss_free(rssdata);

	return 0;
}

int main(int argc, char *argv[])
{
	if (argc != 2) usage();

	printf("NXML Version: %s\n", LIBNXML_VERSION_STRING);
	printf("MRSS Version: %s\n", LIBMRSS_VERSION_STRING);

	parse_feed(argv[1]);

	return 0;
}

