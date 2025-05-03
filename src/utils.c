#include "../include/utils.h"
#include <string.h>

bool handle_res_command(PGconn *conn, PGresult *res) {
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    PQclear(res);
    fprintf(stderr, "Error: %s\n", PQerrorMessage(conn));
    return false;
  }
  PQclear(res);
  return true;
}

bool handle_res_tuples(PGconn *conn, PGresult *res) {
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    PQclear(res);
    fprintf(stderr, "Error: %s\n", PQerrorMessage(conn));
    return false;
  }
  PQclear(res);
  return true;
}

bool is_table_empty(PGconn *conn, const char *table_name) {
  char query[256];
  snprintf(query, sizeof(query),
           "SELECT NOT EXISTS (SELECT 1 FROM %s LIMIT 1);", table_name);

  PGresult *res = PQexec(conn, query);

  if (res == NULL) {
    fprintf(stderr, "Query returned NULL for table %s\n", table_name);
    return false;
  }

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "Query failed for table %s: %s\n", table_name,
            PQerrorMessage(conn));
    PQclear(res);
    return false;
  }

  if (PQntuples(res) < 1 || PQnfields(res) < 1) {
    fprintf(stderr, "Unexpected result format for table %s\n", table_name);
    PQclear(res);
    return false;
  }

  char *value = PQgetvalue(res, 0, 0);
  bool empty = (value != NULL && value[0] == 't');

  PQclear(res);
  return empty;
}

bool import_from_csv(PGconn *conn, const char *table_name,
                     const char *file_path) {
  FILE *file = fopen(file_path, "r");
  if (!file) {
    fprintf(stderr, "Cannot open file %s\n", file_path);
    return false;
  }

  char sql[512];
  snprintf(sql, sizeof(sql), "COPY %s FROM STDIN WITH CSV HEADER", table_name);

  PGresult *res = PQexec(conn, sql);
  if (PQresultStatus(res) != PGRES_COPY_IN) {
    fprintf(stderr, "COPY command failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    fclose(file);
    return false;
  }

  char buffer[1024];
  while (fgets(buffer, sizeof(buffer), file)) {
    if (PQputCopyData(conn, buffer, strlen(buffer)) != 1) {
      fprintf(stderr, "PQputCopyData failed: %s\n", PQerrorMessage(conn));
      fclose(file);
      return false;
    }
  }

  if (PQputCopyEnd(conn, NULL) != 1) {
    fprintf(stderr, "PQputCopyEnd failed: %s\n", PQerrorMessage(conn));
    fclose(file);
    return false;
  }

  res = PQgetResult(conn);
  fclose(file);
  return handle_res_command(conn, res);
}
