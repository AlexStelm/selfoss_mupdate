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

#ifndef SELFOSS_MUPDATE_H
#define SELFOSS_MUPDATE_H

#define _GNU_SOURCE

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

#include <time.h>
#include <sqlite3.h>

#ifndef _NDEBUG

extern int __debug_level;
#define debug(fmt, ...)		if (__debug_level > 0) fprintf(stderr, "%s.%03d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#define debug2(fmt, ...)	if (__debug_level > 1) fprintf(stderr, "%s.%03d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)
#define debug3(fmt, ...)	if (__debug_level > 2) fprintf(stderr, "%s.%03d: " fmt "\n", __func__, __LINE__, ##__VA_ARGS__)

#else

#define debug(fmt, ...)
#define debug2(fmt, ...)
#define debug3(fmt, ...)

#endif

/* prototypes */
void sanitize_text_only(char **field);
int sanitize_content(char **content);

int db_item_exists(sqlite3 *db, char *uid, bool *result);
int db_item_add(sqlite3 *db, int source_id,
		char *title, char *content, char *uid, char *link,
		char *thumb, char *icon, struct tm *pub_tm);
int db_source_set_lastupdate(sqlite3 *db, int source_id, time_t lastupdate);
int db_source_get_all_by_lastupdate(sqlite3 *db);
int db_source_get(sqlite3 *db, int source_id);

#endif /* SELFOSS_MUPDATE_H */
