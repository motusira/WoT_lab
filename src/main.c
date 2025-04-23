#include <libpq-fe.h>
#include <stdio.h>
#include <stdlib.h>

PGconn *connect_to_db(const char *conninfo) {
  PGconn *conn = PQconnectdb(conninfo);
  if (PQstatus(conn) != CONNECTION_OK) {
    printf("Error while connecting to the database server: %s\n",
           PQerrorMessage(conn));
    return NULL;
  }
  return conn;
}

PGresult *create_players_table(PGconn *conn) {
  PGresult *res =
      PQexec(conn, "CREATE TABLE IF NOT EXISTS players ("
                   "login VARCHAR(50) PRIMARY KEY,"
                   "status VARCHAR(20) NOT NULL CHECK (status IN ('online', "
                   "'in_game', 'offline')),"
                   "currency_amount DECIMAL(15, 2) DEFAULT 0.00,"
                   "total_damage INTEGER DEFAULT 0,"
                   "destroyed_vehicles INTEGER DEFAULT 0,"
                   "last_activity TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
                   ")");
  return res;
}

int main(void) {
  char *conninfo = "dbname=lab1db user=lab1 "
                   "password=lab1 host=localhost port=5432";

  PGconn *conn = connect_to_db(conninfo);

  if (conn == NULL) {
    exit(1);
  } else {
    printf("Connection Established\n");
    printf("Port: %s\n", PQport(conn));
    printf("Host: %s\n", PQhost(conn));
    printf("DBName: %s\n", PQdb(conn));
  }

  PQfinish(conn);

  return 0;
}
