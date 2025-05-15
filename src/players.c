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
    players[i].rating =
        players[i].total_damage / 1000 + players[i].destroyed_vehicles * 10;
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
  case BY_DAMAGE:
    result = (p1->total_damage > p2->total_damage)   ? 1
             : (p1->total_damage < p2->total_damage) ? -1
                                                     : 0;
    break;
  case BY_DESTROYED_VEHICLES:
    result = (p1->destroyed_vehicles > p2->destroyed_vehicles)   ? 1
             : (p1->destroyed_vehicles < p2->destroyed_vehicles) ? -1
                                                                 : 0;
    break;
  case BY_CURRENCY_AMOUNT:
    result = (p1->currency_amount > p2->currency_amount)   ? 1
             : (p1->currency_amount < p2->currency_amount) ? -1
                                                           : 0;
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

int compare_by_damage_asc(const void *a, const void *b) {
  return compare_players(a, b, BY_DAMAGE, SORT_ASC);
}

int compare_by_damage_desc(const void *a, const void *b) {
  return compare_players(a, b, BY_DAMAGE, SORT_DESC);
}

int compare_by_destroyed_vehicles_asc(const void *a, const void *b) {
  return compare_players(a, b, BY_DESTROYED_VEHICLES, SORT_ASC);
}

int compare_by_destroyed_vehicles_desc(const void *a, const void *b) {
  return compare_players(a, b, BY_DESTROYED_VEHICLES, SORT_DESC);
}

int compare_by_currency_amount_asc(const void *a, const void *b) {
  return compare_players(a, b, BY_CURRENCY_AMOUNT, SORT_ASC);
}

int compare_by_currency_amount_desc(const void *a, const void *b) {
  return compare_players(a, b, BY_CURRENCY_AMOUNT, SORT_DESC);
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
  case BY_DAMAGE:
    qsort(players, count, sizeof(Player),
          (order == SORT_ASC) ? compare_by_damage_asc : compare_by_damage_desc);
    break;
  case BY_DESTROYED_VEHICLES:
    qsort(players, count, sizeof(Player),
          (order == SORT_ASC) ? compare_by_destroyed_vehicles_asc
                              : compare_by_destroyed_vehicles_desc);
    break;
  case BY_CURRENCY_AMOUNT:
    qsort(players, count, sizeof(Player),
          (order == SORT_ASC) ? compare_by_currency_amount_asc
                              : compare_by_currency_amount_desc);
    break;
  }
}

bool create_player(PGconn *conn, const char *login) {
  if (strlen(login) == 0 || strlen(login) > 32) {
    fprintf(stderr, "Invalid login length\n");
    return false;
  }

  PGresult *res;
  bool success = false;

  PQexec(conn, "BEGIN");

  const char *check_query = "SELECT player_id FROM players WHERE login = $1";
  const char *check_params[1] = {login};

  res = PQexecParams(conn, check_query, 1, NULL, check_params, NULL, NULL, 0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "Check query failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    PQexec(conn, "ROLLBACK");
    return false;
  }

  if (PQntuples(res) > 0) {
    fprintf(stderr, "Player with login '%s' already exists\n", login);
    PQclear(res);
    PQexec(conn, "ROLLBACK");
    return false;
  }
  PQclear(res);

  const char *insert_query =
      "INSERT INTO players (login, status, currency_amount) "
      "VALUES ($1, 'online', 30000) "
      "RETURNING player_id";

  const char *insert_params[1] = {login};

  res = PQexecParams(conn, insert_query, 1, NULL, insert_params, NULL, NULL, 0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "Insert failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    PQexec(conn, "ROLLBACK");
    return false;
  }

  PQexec(conn, "COMMIT");
  PQclear(res);
  return true;
}

GamesStat *get_player_stats(PGconn *conn, int *count) {
  // const char *query =
  //     "SELECT p.player_id, p.login, "
  //     "COUNT(CASE WHEN m.result = 1 THEN 1 END) AS wins, "
  //     "COUNT(CASE WHEN m.result = 2 THEN 1 END) AS losses, "
  //     "COUNT(CASE WHEN m.result = 0 THEN 1 END) AS draws "
  //     "FROM players p "
  //     "LEFT JOIN participants part ON p.player_id = part.player_id "
  //     "LEFT JOIN matches m ON part.match_id = m.match_id "
  //     "GROUP BY p.player_id, p.login "
  //     "ORDER BY p.player_id";
const char *query =
    "SELECT p.player_id, p.login, "
    "COUNT(CASE WHEN (part.team = 1 AND m.result = 1) OR (part.team = 2 AND m.result = 2) THEN 1 END) AS wins, "
    "COUNT(CASE WHEN (part.team = 1 AND m.result = 2) OR (part.team = 2 AND m.result = 1) THEN 1 END) AS losses, "
    "COUNT(CASE WHEN m.result = 0 THEN 1 END) AS draws "
    "FROM players p "
    "LEFT JOIN participants part ON p.player_id = part.player_id "
    "LEFT JOIN matches m ON part.participant_id IN (m.participant1, m.participant2, m.participant3, m.participant4, m.participant5, m.participant6) "
    "GROUP BY p.player_id, p.login "
    "ORDER BY p.player_id";
  PGresult *res = PQexec(conn, query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    printf("%s\n", PQerrorMessage(conn));
    *count = 0;
    PQclear(res);
    return NULL;
  }

  int rows = PQntuples(res);
  GamesStat *stats = malloc(rows * sizeof(GamesStat));
  *count = rows;

  for (int i = 0; i < rows; i++) {
    stats[i].player_id = atoi(PQgetvalue(res, i, 0));
    snprintf(stats[i].login, 50, "%s", PQgetvalue(res, i, 1));
    stats[i].wins = atoi(PQgetvalue(res, i, 2));
    stats[i].losses = atoi(PQgetvalue(res, i, 3));
    stats[i].draws = atoi(PQgetvalue(res, i, 4));
  }

  PQclear(res);
  return stats;
}

int compare_player_stats(const void *a, const void *b,
                         GamesStatCriteria criteria, SortOrder order) {
  const GamesStat *p1 = (const GamesStat *)a;
  const GamesStat *p2 = (const GamesStat *)b;
  int result = 0;

  switch (criteria) {
  case BY_WINS:
    result = p1->wins - p2->wins;
    break;
  case BY_LOSSES:
    result = p1->losses - p2->losses;
    break;
  case BY_DRAWS:
    result = p1->draws - p2->draws;
    break;
  }

  return (order == SORT_DESC) ? -result : result;
}

int compare_by_wins_asc(const void *a, const void *b) {
  return compare_player_stats(a, b, BY_WINS, SORT_ASC);
}

int compare_by_wins_desc(const void *a, const void *b) {
  return compare_player_stats(a, b, BY_WINS, SORT_DESC);
}

int compare_by_losses_asc(const void *a, const void *b) {
  return compare_player_stats(a, b, BY_LOSSES, SORT_ASC);
}

int compare_by_losses_desc(const void *a, const void *b) {
  return compare_player_stats(a, b, BY_LOSSES, SORT_DESC);
}

int compare_by_draws_asc(const void *a, const void *b) {
  return compare_player_stats(a, b, BY_DRAWS, SORT_ASC);
}

int compare_by_draws_desc(const void *a, const void *b) {
  return compare_player_stats(a, b, BY_DRAWS, SORT_DESC);
}

void sort_player_stats(GamesStat *stats, int count, GamesStatCriteria criteria,
                       SortOrder order) {
  switch (criteria) {
  case BY_WINS:
    qsort(stats, count, sizeof(GamesStat),
          (order == SORT_ASC) ? compare_by_wins_asc : compare_by_wins_desc);
    break;
  case BY_LOSSES:
    qsort(stats, count, sizeof(GamesStat),
          (order == SORT_ASC) ? compare_by_losses_asc : compare_by_losses_desc);
    break;
  case BY_DRAWS:
    qsort(stats, count, sizeof(GamesStat),
          (order == SORT_ASC) ? compare_by_draws_asc : compare_by_draws_desc);
    break;
  }
}
