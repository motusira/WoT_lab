#include <libpq-fe.h>
#include <stdlib.h>
#include <string.h>

#include "../include/matches.h"
#include "../include/utils.h"

bool create_participants_table(PGconn *conn) {
  PGresult *res =
      PQexec(conn, "CREATE TABLE IF NOT EXISTS participants("
                   "participant_id SERIAL PRIMARY KEY,"
                   "player_id INT NOT NULL REFERENCES players(player_id),"
                   "hangar_id INT NOT NULL REFERENCES hangars(hangar_id),"
                   "is_killed BOOLEAN NOT NULL DEFAULT false,"
                   "damage_dealt INT NOT NULL CHECK(damage_dealt >= 0),"
                   "kills INT NOT NULL CHECK(kills >= 0),"
                   "team INT NOT NULL CHECK(team IN(1, 2)));");
  return handle_res_command(conn, res);
}

bool create_matches_table(PGconn *conn) {
  PGresult *res = PQexec(
      conn, "CREATE TABLE IF NOT EXISTS matches ("
            "match_id SERIAL PRIMARY KEY,"
            "start_time TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP,"
            "result INT NOT NULL CHECK(result IN (-1, 0, 1, 2)),"
            "tech_level INT NOT NULL CHECK(tech_level BETWEEN 1 AND 10),"
            "participant1 INT NOT NULL REFERENCES participants(participant_id),"
            "participant2 INT NOT NULL REFERENCES participants(participant_id),"
            "participant3 INT NOT NULL REFERENCES participants(participant_id),"
            "participant4 INT NOT NULL REFERENCES participants(participant_id),"
            "participant5 INT NOT NULL REFERENCES participants(participant_id),"
            "participant6 INT NOT NULL REFERENCES participants(participant_id)"
            ");");
  return handle_res_command(conn, res);
}

int find_and_create_match(PGconn *conn) {
  PGresult *res;
  char query[1024];

  snprintf(query, sizeof(query),
           "WITH available_players AS ("
           "  SELECT p.player_id, h.hangar_id, ti.tier "
           "  FROM players p "
           "  JOIN hangars h ON p.player_id = h.player_id "
           "  JOIN tanks t ON h.tank_id = t.tank_id "
           "  JOIN tank_info ti ON t.data_id = ti.data_id "
           "  WHERE p.status = 'online' "
           "    AND h.status = 'operational' "
           ") "
           "SELECT tier, COUNT(*) as cnt "
           "FROM available_players "
           "GROUP BY tier "
           "HAVING COUNT(*) >= 6 "
           "ORDER BY cnt DESC "
           "LIMIT 1");

  res = PQexec(conn, query);
  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
    PQclear(res);
    return -1;
  }

  int target_tier = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  snprintf(query, sizeof(query),
           "SELECT p.player_id, h.hangar_id "
           "FROM players p "
           "JOIN hangars h ON p.player_id = h.player_id "
           "JOIN tanks t ON h.tank_id = t.tank_id "
           "JOIN tank_info ti ON t.data_id = ti.data_id "
           "WHERE p.status = 'online' "
           "  AND h.status = 'operational' "
           "  AND ti.tier = %d "
           "ORDER BY RANDOM() "
           "LIMIT 6",
           target_tier);

  res = PQexec(conn, query);
  if (PQntuples(res) < 6) {
    PQclear(res);
    return -1;
  }

  PQexec(conn, "BEGIN");

  int participants[6];
  for (int i = 0; i < 6; i++) {
    int player_id = atoi(PQgetvalue(res, i, 0));
    int hangar_id = atoi(PQgetvalue(res, i, 1));
    int team = (i < 3) ? 1 : 2;

    snprintf(query, sizeof(query),
             "INSERT INTO participants "
             "(player_id, hangar_id, is_killed, damage_dealt, kills, team) "
             "VALUES (%d, %d, false, 0, 0, %d) RETURNING participant_id",
             player_id, hangar_id, team);

    PGresult *ires = PQexec(conn, query);
    if (PQresultStatus(ires) != PGRES_TUPLES_OK) {
      PQclear(ires);
      PQexec(conn, "ROLLBACK");
      PQclear(res);
      return -1;
    }
    participants[i] = atoi(PQgetvalue(ires, 0, 0));
    PQclear(ires);

    snprintf(query, sizeof(query),
             "UPDATE players SET status = 'in_game' WHERE player_id = %d",
             player_id);
    PGresult *ures = PQexec(conn, query);
    if (PQresultStatus(ures) != PGRES_COMMAND_OK) {
      PQclear(ures);
      PQexec(conn, "ROLLBACK");
      return -1;
    }
    PQclear(ures);
  }
  PQclear(res);

  snprintf(query, sizeof(query),
           "INSERT INTO matches "
           "(start_time, result, tech_level, "
           "participant1, participant2, participant3, "
           "participant4, participant5, participant6) "
           "VALUES (NOW(), -1, %d, %d, %d, %d, %d, %d, %d) RETURNING match_id",
           target_tier, participants[0], participants[1], participants[2],
           participants[3], participants[4], participants[5]);

  res = PQexec(conn, query);
  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    PQexec(conn, "ROLLBACK");
    PQclear(res);
    return -1;
  }

  int match_id = atoi(PQgetvalue(res, 0, 0));
  PQclear(res);

  PQexec(conn, "COMMIT");
  return match_id;
}

int *distribute_kills(int total_kills, int player_count) {
  int *kills = calloc(player_count, sizeof(int));
  for (int i = 0; i < total_kills; i++) {
    kills[rand() % player_count]++;
  }
  return kills;
}

void process_completed_matches(PGconn *conn) {
  const char *find_matches_query =
      "SELECT match_id, start_time, tech_level, "
      "participant1, participant2, participant3, "
      "participant4, participant5, participant6 "
      "FROM matches "
      "WHERE result = -1 "
      "  AND start_time < (NOW() - INTERVAL '30 seconds')";

  PGresult *res = PQexec(conn, find_matches_query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    PQclear(res);
    return;
  }

  int match_count = PQntuples(res);

  for (int i = 0; i < match_count; i++) {
    int match_id = atoi(PQgetvalue(res, i, 0));
    int tech_level = atoi(PQgetvalue(res, i, 2));

    int participants[6];
    for (int p = 0; p < 6; p++) {
      participants[p] = atoi(PQgetvalue(res, i, 3 + p));
    }

    PQexec(conn, "BEGIN");

    int result = rand() % 3;
    double multipliers[3] = {1.0, 1.5, 0.5};

    int team1_kills = rand() % 4;
    int team2_kills = rand() % 4;

    int team1_deaths = team2_kills;
    int team2_deaths = team1_kills;

    int *team1_kills_dist = distribute_kills(team1_kills, 3);
    int *team2_kills_dist = distribute_kills(team2_kills, 3);

    for (int p = 0; p < 6; p++) {
      int team = (p < 3) ? 1 : 2;
      int player_kills =
          (p < 3) ? team1_kills_dist[p] : team2_kills_dist[p - 3];
      int deaths = (team == 1) ? (player_kills > 0 ? 0 : (rand() % 2))
                               : (player_kills > 0 ? 0 : (rand() % 2));

      int damage = 500 + rand() % 4501 + player_kills * 500;

      char update_participant[512];
      snprintf(update_participant, sizeof(update_participant),
               "UPDATE participants SET "
               "damage_dealt = %d, "
               "kills = %d, "
               "is_killed = %s "
               "WHERE participant_id = %d",
               damage, player_kills, (deaths == 1) ? "true" : "false",
               participants[p]);

      PGresult *ures = PQexec(conn, update_participant);
      PQclear(ures);

      double multiplier = (result == 0)      ? multipliers[0]
                          : (team == result) ? multipliers[1]
                                             : multipliers[2];

      int currency = (int)((damage + player_kills * 1000) * multiplier);

      char update_player[512];
      snprintf(update_player, sizeof(update_player),
               "UPDATE players "
               "SET currency_amount = currency_amount + %d, "
               "status = 'online', "
               "total_damage = total_damage + %d, "
               "destroyed_vehicles = destroyed_vehicles + %d "
               "WHERE player_id = ("
               "  SELECT player_id FROM participants "
               "  WHERE participant_id = %d)",
               currency, damage, player_kills, participants[p]);

      PGresult *pures = PQexec(conn, update_player);
      PQclear(pures);

      if (deaths > 0 || (rand() % 100 < 30)) {
        char update_tank[512];
        snprintf(update_tank, sizeof(update_tank),
                 "UPDATE hangars SET status = 'needs_repair', "
                 "game_points = game_points + %d "
                 "WHERE hangar_id = ("
                 "  SELECT hangar_id FROM participants "
                 "  WHERE participant_id = %d)",
                 damage / 100, participants[p]);

        PGresult *tres = PQexec(conn, update_tank);
        PQclear(tres);
      } else {
        char update_tank[512];
        snprintf(update_tank, sizeof(update_tank),
                 "UPDATE hangars SET "
                 "game_points = game_points + %d "
                 "WHERE hangar_id = ("
                 "  SELECT hangar_id FROM participants "
                 "  WHERE participant_id = %d)",
                 damage / 100, participants[p]);

        PGresult *tres = PQexec(conn, update_tank);
        PQclear(tres);
      }
    }

    char update_match[256];
    snprintf(update_match, sizeof(update_match),
             "UPDATE matches SET result = %d WHERE match_id = %d", result,
             match_id);

    PGresult *mres = PQexec(conn, update_match);
    PQclear(mres);

    PQexec(conn, "COMMIT");
    free(team1_kills_dist);
    free(team2_kills_dist);
  }
  PQclear(res);
}

Match *fetch_all_matches(PGconn *conn, int *match_count) {
  const char *query = "SELECT match_id, "
                      "to_char(start_time, 'YYYY-MM-DD HH24:MI:SS'), "
                      "result, tech_level, "
                      "participant1, participant2, participant3, "
                      "participant4, participant5, participant6 "
                      "FROM matches "
                      "ORDER BY start_time DESC";

  PGresult *res = PQexec(conn, query);
  *match_count = 0;

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    PQclear(res);
    return NULL;
  }

  int rows = PQntuples(res);
  if (rows == 0) {
    PQclear(res);
    return NULL;
  }

  Match *matches = malloc(rows * sizeof(Match));
  if (!matches) {
    PQclear(res);
    return NULL;
  }

  for (int i = 0; i < rows; i++) {
    matches[i].match_id = atoi(PQgetvalue(res, i, 0));

    strncpy(matches[i].start_time, PQgetvalue(res, i, 1),
            sizeof(matches[i].start_time) - 1);
    matches[i].start_time[sizeof(matches[i].start_time) - 1] = '\0';

    matches[i].result = atoi(PQgetvalue(res, i, 2));
    matches[i].tech_level = atoi(PQgetvalue(res, i, 3));

    for (int p = 0; p < 6; p++) {
      matches[i].participant_ids[p] = atoi(PQgetvalue(res, i, 4 + p));
    }
  }

  *match_count = rows;
  PQclear(res);
  return matches;
}

void free_matches(Match *matches) { free(matches); }

#define MAX_NICKNAME_LENGTH 50

char *get_nickname_by_participant_id(PGconn *conn, int participant_id) {
  const char *query = "SELECT pl.login "
                      "FROM participants pa "
                      "JOIN players pl ON pa.player_id = pl.player_id "
                      "WHERE pa.participant_id = $1";

  char participant_id_str[16];
  snprintf(participant_id_str, sizeof(participant_id_str), "%d",
           participant_id);

  const char *params[1] = {participant_id_str};
  char *result_nickname = NULL;

  PGresult *res = PQexecParams(conn, query, 1, NULL, params, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "Query failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return NULL;
  }

  int rows = PQntuples(res);
  if (rows > 0) {
    result_nickname = malloc(MAX_NICKNAME_LENGTH);
    if (result_nickname) {
      strncpy(result_nickname, PQgetvalue(res, 0, 0), MAX_NICKNAME_LENGTH - 1);
      result_nickname[MAX_NICKNAME_LENGTH - 1] = '\0';
    }
  }

  PQclear(res);
  return result_nickname;
}
