#include <libpq-fe.h>
#include <stdlib.h>
#include <time.h>

#include "../include/players.h"
#include "../include/utils.h"

bool create_players_table(PGconn *conn) {
  PGresult *res =
      PQexec(conn, "CREATE TABLE IF NOT EXISTS players ("
                   "player_id SERIAL PRIMARY KEY,"
                   "login VARCHAR(50) UNIQUE NOT NULL,"
                   "status VARCHAR(20) NOT NULL CHECK (status IN ('online', "
                   "'in_game', 'offline')),"
                   "currency_amount INTEGER DEFAULT 0,"
                   "total_damage INTEGER DEFAULT 0,"
                   "destroyed_vehicles INTEGER DEFAULT 0"
                   ")");
  return handle_res_command(conn, res);
}

bool clear_players_table(PGconn *conn) {
  PGresult *res =
      PQexec(conn, "TRUNCATE TABLE players RESTART IDENTITY CASCADE");

  return handle_res_command(conn, res);
}

bool fill_players_table(PGconn *conn) {
  if (is_table_empty(conn, "players")) {
    return import_from_csv(conn, "players", "assets/players.csv");
  } else {
    return true;
  }
}

bool insert_random_players(PGconn *conn, int count) {
  srand((unsigned int)time(NULL));

  const char *statuses[] = {"online", "in_game", "offline"};
  const char *login_prefixes[] = {"player", "gamer", "pro", "newbie", "legend"};
  const char *login_suffixes[] = {"123", "X", "99", "007", "42", "GH", "TM"};

  PGresult *begin_res = PQexec(conn, "BEGIN");

  if (!handle_res_command(conn, begin_res)) {
    return false;
  }

  for (int i = 0; i < count; i++) {
    char login[50];
    const char *prefix = login_prefixes[rand() % 5];
    const char *suffix = login_suffixes[rand() % 7];
    snprintf(login, sizeof(login), "%s_%s_%d", prefix, suffix, rand() % 1000);

    const char *status = statuses[rand() % 3];
    char currency[20];
    snprintf(currency, sizeof(currency), "%d.%02d", rand() % 10000,
             rand() % 100);
    char damage_str[20], vehicles_str[20];
    snprintf(damage_str, sizeof(damage_str), "%d", rand() % 100000);
    snprintf(vehicles_str, sizeof(vehicles_str), "%d", rand() % 500);

    const char *paramValues[5] = {login, status, currency, damage_str,
                                  vehicles_str};
    int paramLengths[5] = {0};
    int paramFormats[5] = {0};

    PGresult *res = PQexecParams(
        conn,
        "INSERT INTO players (login, status, currency_amount, total_damage, "
        "destroyed_vehicles) "
        "VALUES ($1, $2, $3::DECIMAL(15,2), $4::INTEGER, $5::INTEGER)",
        5, NULL, paramValues, paramLengths, paramFormats, 0);

    if (!handle_res_command(conn, res)) {
      PQexec(conn, "ROLLBACK");
      return false;
    }
  }

  PGresult *commit_res = PQexec(conn, "COMMIT");

  if (!handle_res_command(conn, commit_res)) {
    return false;
  }

  return true;
}

void get_player_vehicles(PGconn *conn, const char *login) {
  const char *query =
      "SELECT p.player_id, h.tank_id, ti.type, m.mod_id, h.game_points "
      "FROM players p "
      "JOIN hangars h USING(player_id) "
      "JOIN tanks t USING(tank_id) "
      "JOIN tank_info ti ON t.data_id = ti.data_id "
      "JOIN modifications m ON t.mod_id = m.mod_id "
      "WHERE p.login = $1";

  const char *params[1] = {login};

  PGresult *res = PQexecParams(conn, query, 1, NULL, params, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "Query failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return;
  }

  int rows = PQntuples(res);
  if (rows == 0) {
    printf("No vehicles found for player: %s\n", login);
    return;
  }

  printf("Player ID: %s\n", PQgetvalue(res, 0, 0));
  printf("| %-8s | %-12s | %-15s | %-10s |\n", "Tank ID", "Type",
         "Modification", "Points");

  for (int i = 0; i < rows; i++) {
    printf("| %-8s | %-12s | %-15s | %-10s |\n", PQgetvalue(res, i, 1),
           PQgetvalue(res, i, 2), PQgetvalue(res, i, 3), PQgetvalue(res, i, 4));
  }

  PQclear(res);
}
