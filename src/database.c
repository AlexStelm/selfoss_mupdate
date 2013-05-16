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


int db_item_exists(sqlite3 *db, char *uid, bool *result)
{
	sqlite3_stmt *stmt;
	char sql[] = "SELECT COUNT(*) AS amount FROM items WHERE uid=:uid";
	int rc;

	rc = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmt, NULL);
	if (rc == SQLITE_OK) rc = sqlite3_bind_text(stmt, 1, uid, -1, SQLITE_STATIC);

	if (rc == SQLITE_OK)
		rc = sqlite3_step(stmt);

	if (rc == SQLITE_ROW) {
		rc = sqlite3_column_int(stmt, 0);
		*result = rc > 0;
		debug3("amount=%d", rc);
	}
	else {
		sqlite3_finalize(stmt);
		*result = 0;
		debug3("failed");
		return -1;
	}

	return sqlite3_finalize(stmt);
}

int db_item_add(sqlite3 *db, int source_id,
		char *title, char *content, char *uid, char *link,
		char *thumb, char *icon, struct tm *pub_tm)
{
	sqlite3_stmt *stmt;
	char datetime[256];
	char sql[] = "INSERT INTO items ("
		"datetime, title, content, unread, "
		"starred, source, thumbnail, icon, "
		"uid, link) "
		"VALUES ("
		":datetime, :title, :content, :unread, "
		":starred, :source, :thumbnail, :icon, "
		":uid, :link )";
	int rc;

	/* stored in localtime */
	time_t t = mktime(pub_tm);
	localtime_r(&t, pub_tm);

	size_t dt_sz = strftime(datetime, sizeof(datetime), "%F %T", pub_tm);

	rc = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmt, NULL);
	if (rc == SQLITE_OK) rc = sqlite3_bind_text(stmt, 1, datetime, dt_sz, SQLITE_STATIC);
	if (rc == SQLITE_OK) rc = sqlite3_bind_text(stmt, 2, title, -1, SQLITE_STATIC);
	if (rc == SQLITE_OK) rc = sqlite3_bind_text(stmt, 3, content, -1, SQLITE_STATIC);
	if (rc == SQLITE_OK) rc = sqlite3_bind_int (stmt, 4, 1); /* :unread */
	if (rc == SQLITE_OK) rc = sqlite3_bind_int (stmt, 5, 0); /* :starred */
	if (rc == SQLITE_OK) rc = sqlite3_bind_int (stmt, 6, source_id);
	if (rc == SQLITE_OK) rc = sqlite3_bind_text(stmt, 7, thumb, -1, SQLITE_STATIC);
	if (rc == SQLITE_OK) rc = sqlite3_bind_text(stmt, 8, icon, -1, SQLITE_STATIC);
	if (rc == SQLITE_OK) rc = sqlite3_bind_text(stmt, 9, uid, -1, SQLITE_STATIC);
	if (rc == SQLITE_OK) rc = sqlite3_bind_text(stmt, 10, link, -1, SQLITE_STATIC);

	if (rc == SQLITE_OK)
		rc = sqlite3_step(stmt);

	return sqlite3_finalize(stmt);
}

int db_source_set_lastupdate(sqlite3 *db, int source_id, time_t lastupdate)
{
	sqlite3_stmt *stmt;
	char sql[] = "UPDATE sources SET lastupdate=:lastupdate WHERE id=:id";
	int rc;

	if (lastupdate == 0)
		lastupdate = time(NULL);

	rc = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmt, NULL);
	if (rc == SQLITE_OK) rc = sqlite3_bind_int(stmt, 1, lastupdate);
	if (rc == SQLITE_OK) rc = sqlite3_bind_int(stmt, 2, source_id);

	if (rc == SQLITE_OK)
		rc = sqlite3_step(stmt);

	return sqlite3_finalize(stmt);
}

int db_source_get_all_by_lastupdate(sqlite3 *db)
{
	/* "SELECT id, title, tags, spout, params, error FROM sources ORDER BY lastupdate ASC" */

	return 0;
}

int db_source_get(sqlite3 *db, int source_id,
		char **title, char **params, char **spouts,
		char **params, char **error)
{
	sqlite3_stmt *stmt;
	char sql[] = "SELECT id, title, tags, spout, params, error FROM sources WHERE id=:id";
	int rc;

	rc = sqlite3_prepare_v2(db, sql, sizeof(sql), &stmt, NULL);
	if (rc == SQLITE_OK) rc = sqlite3_bind_int(stmt, 1, source_id);

	if (rc == SQLITE_OK)
		rc = sqlite3_step(stmt);

	if (rc == SQLITE_ROW) {
		sqlite3_column_bytes(stmt, 1);
		*title = strdup(sqlite3_column_text(stmt, 1));

		sqlite3_column_bytes(stmt, 2);
		*tags = strdup(sqlite3_column_text(stmt, 2));

		sqlite3_column_bytes(stmt, 3);
		*spout = strdup(sqlite3_column_text(stmt, 3));

		sqlite3_column_bytes(stmt, 4);
		*params = strdup(sqlite3_column_text(stmt, 4));

		sqlite3_column_bytes(stmt, 5);
		*error = strdup(sqlite3_column_text(stmt, 5));

		debug3("source: #%d title: %s tags: %s spout: %s params: %s error: %s",
				source_id, *title, *tags, *spout, *params, *error);
	}
	else {
		sqlite3_finalize(stmt);
		debug3("failed");
		return -1;
	}

	return sqlite3_finalize(stmt);
}

