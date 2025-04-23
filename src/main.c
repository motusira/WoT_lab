#include <libpq-fe.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "../include/players.h"

PGconn *connect_to_db(const char *conninfo) {
  PGconn *conn = PQconnectdb(conninfo);
  if (PQstatus(conn) != CONNECTION_OK) {
    printf("Error while connecting to the database server: %s\n",
           PQerrorMessage(conn));
    return NULL;
  }
  return conn;
}

int main(void) {
  char *conninfo = "dbname=lab1db user=lab1 "
                   "password=lab1 host=localhost port=5432";

  PGconn *conn = connect_to_db(conninfo);

  if (conn == NULL) {
    fprintf(stderr, "Error: Connection failed\n");
    exit(1);
  } else {
    printf("Connection Established\n");
    printf("Port: %s\n", PQport(conn));
    printf("Host: %s\n", PQhost(conn));
    printf("DBName: %s\n", PQdb(conn));
  }

  create_players_table(conn);
  insert_random_players(conn, 5);
  getchar();
  clear_players_table(conn);

  PQfinish(conn);

  return 0;
}
