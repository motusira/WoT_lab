#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>
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

void free_players(Player *players, int count) {
  for (int i = 0; i < count; i++) {
    free(players[i].login);
    free(players[i].status);
  }
  free(players);
}

Player *fetch_all_players(PGconn *conn, int *player_count) {
  const char *query = "SELECT * FROM players";
  PGresult *res = PQexec(conn, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "Query failed: %s\n", PQerrorMessage(conn));
    *player_count = 0;
    PQclear(res);
    return NULL;
  }

  int rows = PQntuples(res);
  *player_count = rows;

  Player *players = malloc(rows * sizeof(Player));
  if (!players) {
    perror("Memory allocation failed");
    *player_count = 0;
    PQclear(res);
    return NULL;
  }

  for (int i = 0; i < rows; i++) {
    players[i].player_id = atoi(PQgetvalue(res, i, 0));

    players[i].login = strdup(PQgetvalue(res, i, 1));
    players[i].status = strdup(PQgetvalue(res, i, 2));

    players[i].currency_amount = atof(PQgetvalue(res, i, 3));
    players[i].total_damage = atoi(PQgetvalue(res, i, 4));
    players[i].destroyed_vehicles = atoi(PQgetvalue(res, i, 5));
    players[i].rating = players[i].total_damage / 1000 + players[i].destroyed_vehicles * 10;
  }

  PQclear(res);
  return players;
}

int compare_players(const void *a, const void *b, SortCriteria criteria,
                    SortOrder order) {
  const Player *p1 = (const Player *)a;
  const Player *p2 = (const Player *)b;
  int result = 0;

  switch (criteria) {
  case BY_ID:
    result = p1->player_id - p2->player_id;
    break;
  case BY_RATING:
    result = (p1->rating > p2->rating) ? 1 : (p1->rating < p2->rating) ? -1 : 0;
    break;
  }

  return (order == SORT_DESC) ? -result : result;
}

int compare_by_id_asc(const void *a, const void *b) {
  return compare_players(a, b, BY_ID, SORT_ASC);
}

int compare_by_id_desc(const void *a, const void *b) {
  return compare_players(a, b, BY_ID, SORT_DESC);
}

int compare_by_rating_asc(const void *a, const void *b) {
  return compare_players(a, b, BY_RATING, SORT_ASC);
}

int compare_by_rating_desc(const void *a, const void *b) {
  return compare_players(a, b, BY_RATING, SORT_DESC);
}

void sort_players(Player *players, int count, SortCriteria criteria,
                  SortOrder order) {
  switch (criteria) {
  case BY_ID:
    qsort(players, count, sizeof(Player),
          (order == SORT_ASC) ? compare_by_id_asc : compare_by_id_desc);
    break;
  case BY_RATING:
    qsort(players, count, sizeof(Player),
          (order == SORT_ASC) ? compare_by_rating_asc : compare_by_rating_desc);
    break;
  }
}
