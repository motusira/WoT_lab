#ifndef LAB_UTILS_H
#define LAB_UTILS_H

#include <libpq-fe.h>
#include <stdbool.h>

bool handle_res_command(PGconn *conn, PGresult *res);
bool handle_res_tuples(PGconn *conn, PGresult *res);
bool is_table_empty(PGconn *conn, const char *table_name);
bool import_from_csv(PGconn *conn, const char *table_name, const char *file_path);

#endif
