#include <libpq-fe.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "../include/hangar.h"
#include "../include/players.h"
#include "../include/ui.h"

PGconn *connect_to_db(const char *conninfo) {
  PGconn *conn = PQconnectdb(conninfo);
  if (PQstatus(conn) != CONNECTION_OK) {
    fprintf(stderr, "Error while connecting to the database server: %s\n",
            PQerrorMessage(conn));
    return NULL;
  }
  return conn;
}

int main(void) {
  char *conninfo = "dbname=lab1db user=lab1 "
                   "password=lab1 host=localhost port=5432";

  PGconn *conn = connect_to_db(conninfo);

  int exit_code = 0;

  if (conn == NULL) {
    fprintf(stderr, "Error: Connection failed\n");
    exit(1);
  } else {
    printf("Connection Established\n");
    printf("Port: %s\n", PQport(conn));
    printf("Host: %s\n", PQhost(conn));
    printf("DBName: %s\n", PQdb(conn));
  }

  if (!create_players_table(conn)) {
    exit_code = 1;
    goto cleanup;
  }
  if (!create_modifications_table(conn)) {
    exit_code = 1;
    goto cleanup;
  }
  if (!create_tank_info_table(conn)) {
    exit_code = 1;
    goto cleanup;
  }
  if (!create_tanks_table(conn)) {
    exit_code = 1;
    goto cleanup;
  }
  if (!create_hangars_table(conn)) {
    exit_code = 1;
    goto cleanup;
  }
  if (!fill_players_table(conn)) {
    exit_code = 1;
    goto cleanup;
  }
  if (!fill_tank_info_table(conn)) {
    exit_code = 1;
    goto cleanup;
  }
  if (!fill_modifications_table(conn)) {
    exit_code = 1;
    goto cleanup;
  }
  if (!fill_tanks_table(conn)) {
    exit_code = 1;
    goto cleanup;
  }
  if (!fill_hangars_table(conn)) {
    exit_code = 1;
    goto cleanup;
  }
  // getchar();
  // if (!clear_players_table(conn)) {
  //   exit_code = 1;
  //   goto cleanup;
  // }

  // get_player_vehicles(conn, "BlitzKing");
  // get_player_vehicles(conn, "IronDuke");
  // get_player_vehicles(conn, "SteelHunter");
  // get_player_vehicles(conn, "RedBaron");

  ui_start(conn);

cleanup:
  PQfinish(conn);
  return exit_code;
}
