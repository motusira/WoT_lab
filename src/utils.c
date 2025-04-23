#include "../include/utils.h"

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
