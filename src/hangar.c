#include "../include/hangar.h"
#include "../include/utils.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

bool create_hangars_table(PGconn *conn) {
  PGresult *res = PQexec(
      conn,
      "CREATE TABLE IF NOT EXISTS hangars ("
      "hangar_id SERIAL PRIMARY KEY,"
      "player_id INT NOT NULL REFERENCES players(player_id) ON DELETE CASCADE,"
      "tank_id INT NOT NULL REFERENCES tanks(tank_id),"
      "game_points INT NOT NULL DEFAULT 0,"
      "status VARCHAR(20) NOT NULL CHECK(status IN ('operational', "
      "'needs_repair')),"
      "is_sold BOOLEAN NOT NULL DEFAULT FALSE,"
      "UNIQUE(player_id, tank_id))");

  return handle_res_command(conn, res);
}

bool create_modifications_table(PGconn *conn) {
  PGresult *res = PQexec(conn, "CREATE TABLE IF NOT EXISTS modifications ("
                               "mod_id SERIAL PRIMARY KEY"
                               ")");
  return handle_res_command(conn, res);
}

bool create_tank_info_table(PGconn *conn) {
  PGresult *res = PQexec(conn, "CREATE TABLE IF NOT EXISTS tank_info ("
                               "data_id SERIAL PRIMARY KEY,"
                               "country VARCHAR(50) NOT NULL CHECK (country IN "
                               "('USSR', 'USA', 'GERMANY')),"
                               "type VARCHAR(50) NOT NULL CHECK(type IN "
                               "('light', 'medium', 'heavy', 'TD', 'SPG')),"
                               "tier INT NOT NULL CHECK(tier BETWEEN 1 AND 5)"
                               ")");
  return handle_res_command(conn, res);
}

bool create_tanks_table(PGconn *conn) {
  PGresult *res =
      PQexec(conn, "CREATE TABLE IF NOT EXISTS tanks ("
                   "tank_id SERIAL PRIMARY KEY,"
                   "data_id INT NOT NULL REFERENCES tank_info(data_id),"
                   "mod_id INT NOT NULL REFERENCES modifications(mod_id),"
                   "price INT NOT NULL,"
                   "required_points INT NOT NULL,"
                   "UNIQUE(data_id, mod_id)"
                   ")");
  return handle_res_command(conn, res);
}

bool fill_modifications_table(PGconn *conn) {
  if (is_table_empty(conn, "modifications")) {
    return import_from_csv(conn, "modifications", "assets/modifications.csv");
  } else {
    return true;
  }
}

bool fill_tank_info_table(PGconn *conn) {
  if (is_table_empty(conn, "tank_info")) {
    return import_from_csv(conn, "tank_info", "assets/tank_info.csv");
  } else {
    return true;
  }
}

bool fill_hangars_table(PGconn *conn) {
  if (is_table_empty(conn, "hangars")) {
    return import_from_csv(conn, "hangars", "assets/hangars.csv");
  } else {
    return true;
  }
}

bool _fill_tank_modifications(PGconn *conn) {
  const char *select_query = "SELECT data_id, tier FROM tank_info";
  PGresult *res = PQexec(conn, select_query);

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "Error fetching tank info: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return false;
  }

  int rows = PQntuples(res);

  PGresult *begin_res = PQexec(conn, "BEGIN");
  if (!handle_res_command(conn, begin_res)) {
    PQclear(res);
    return false;
  }

  bool success = true;

  for (int i = 0; i < rows; i++) {
    char *data_id_str = PQgetvalue(res, i, 0);
    char *tier_str = PQgetvalue(res, i, 1);
    int tier = atoi(tier_str);
    int data_id = atoi(data_id_str);

    for (int mod_id = 1; mod_id <= 3; mod_id++) {
      int base_price = tier * 2000;
      int base_points = tier * 200;
      int final_price, final_points;

      switch (mod_id) {
      case 1:
        final_price = base_price;
        final_points = 0;
        break;
      case 2:
        final_price = round(base_price * 2.5);
        final_points = round(base_points * 2.5);
        break;
      case 3:
        final_price = base_price * 5;
        final_points = base_points * 5;
        break;
      default:
        final_price = 0;
        final_points = 0;
      }

      char price_str[20], points_str[20];
      snprintf(price_str, sizeof(price_str), "%d", final_price);
      snprintf(points_str, sizeof(points_str), "%d", final_points);

      const char *params[4] = {
          data_id_str, (char[2]){mod_id + '0', '\0'}, // mod_id как строка
          price_str, points_str};

      PGresult *insert_res = PQexecParams(conn,
                                          "INSERT INTO tanks(data_id, mod_id, "
                                          "price, required_points) "
                                          "VALUES($1, $2, $3, $4)",
                                          4, NULL, params, NULL, NULL, 0);

      if (PQresultStatus(insert_res) != PGRES_COMMAND_OK) {
        fprintf(stderr, "Insert failed: %s\n", PQerrorMessage(conn));
        success = false;
      }
      PQclear(insert_res);

      if (!success)
        break;
    }
    if (!success)
      break;
  }

  PQclear(res);

  if (success) {
    PGresult *commit_res = PQexec(conn, "COMMIT");
    success = handle_res_command(conn, commit_res);
  } else {
    PQexec(conn, "ROLLBACK");
  }

  return success;
}

bool fill_tanks_table(PGconn *conn) {
  if (is_table_empty(conn, "tanks")) {
    return _fill_tank_modifications(conn);
  } else {
    return true;
  }
}

TankInfo *get_player_tanks(PGconn *conn, const char *login, int *count) {
  const char *query = "SELECT "
                      "h.tank_id, "
                      "ti.tier, "
                      "ti.country, "
                      "h.status, "
                      "ti.type, "
                      "m.mod_id, "
                      "h.game_points, "
                      "t.price, "
                      "t.required_points, "
                      "h.hangar_id "
                      "FROM players p "
                      "JOIN hangars h USING(player_id) "
                      "JOIN tanks t USING(tank_id) "
                      "JOIN tank_info ti ON t.data_id = ti.data_id "
                      "JOIN modifications m ON t.mod_id = m.mod_id "
                      "WHERE p.login = $1 AND h.is_sold = FALSE";

  const char *params[1] = {login};
  *count = 0;

  PGresult *res = PQexecParams(conn, query, 1, NULL, params, NULL, NULL, 0);

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "Query failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return NULL;
  }

  int rows = PQntuples(res);
  if (rows == 0) {
    PQclear(res);
    return NULL;
  }

  TankInfo *tanks = malloc(rows * sizeof(TankInfo));
  if (!tanks) {
    PQclear(res);
    return NULL;
  }

  for (int i = 0; i < rows; i++) {
    tanks[i].tank_id = atoi(PQgetvalue(res, i, 0));
    tanks[i].tier = atoi(PQgetvalue(res, i, 1));

    strncpy(tanks[i].country, PQgetvalue(res, i, 2),
            sizeof(tanks[i].country) - 1);
    tanks[i].country[sizeof(tanks[i].country) - 1] = '\0';

    strncpy(tanks[i].hangar_status, PQgetvalue(res, i, 3),
            sizeof(tanks[i].hangar_status) - 1);
    tanks[i].hangar_status[sizeof(tanks[i].hangar_status) - 1] = '\0';

    strncpy(tanks[i].type, PQgetvalue(res, i, 4), sizeof(tanks[i].type) - 1);
    tanks[i].type[sizeof(tanks[i].type) - 1] = '\0';

    tanks[i].mod_id = atoi(PQgetvalue(res, i, 5));
    tanks[i].game_points = atoi(PQgetvalue(res, i, 6));
    tanks[i].price = atoi(PQgetvalue(res, i, 7));
    tanks[i].required_points = atoi(PQgetvalue(res, i, 8));
    tanks[i].hangar_id = atoi(PQgetvalue(res, i, 9));
  }

  *count = rows;
  PQclear(res);
  return tanks;
}

bool repair_tank_by_login(PGconn *conn, const char *login, int h_id,
                          int r_cost) {
  PGresult *res;
  bool success = false;

  PQexec(conn, "BEGIN");

  char hangar_id[20];
  snprintf(hangar_id, 20, "%d", h_id);

  char repair_cost[20];
  snprintf(repair_cost, 20, "%d", r_cost);

  const char *check_balance_query =
      "SELECT p.player_id, p.currency_amount "
      "FROM players p "
      "JOIN hangars h ON p.player_id = h.player_id "
      "WHERE p.login = $1 AND h.hangar_id = $2";
  const char *params[2] = {login, hangar_id};

  res = PQexecParams(conn, check_balance_query, 2, NULL, params, NULL, NULL, 0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
    PQclear(res);
    PQexec(conn, "ROLLBACK");
    return false;
  }

  int p_id = atoi(PQgetvalue(res, 0, 0));
  int balance = atoi(PQgetvalue(res, 0, 1));
  PQclear(res);

  char player_id[20];
  snprintf(player_id, 20, "%d", p_id);

  if (balance < r_cost) {
    PQexec(conn, "ROLLBACK");
    return false;
  }

  const char *update_balance_query =
      "UPDATE players SET currency_amount = currency_amount - $1 "
      "WHERE login = $2";
  const char *update_params[2] = {repair_cost, login};

  res = PQexecParams(conn, update_balance_query, 2, NULL, update_params, NULL,
                     NULL, 0);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    PQclear(res);
    PQexec(conn, "ROLLBACK");
    return false;
  }
  PQclear(res);

  const char *update_hangar_query = "UPDATE hangars SET status = 'operational' "
                                    "WHERE hangar_id = $1 AND player_id = $2";
  const char *hangar_params[2] = {hangar_id, player_id};

  res = PQexecParams(conn, update_hangar_query, 2, NULL, hangar_params, NULL,
                     NULL, 0);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    PQclear(res);
    PQexec(conn, "ROLLBACK");
    return false;
  }
  PQclear(res);

  PQexec(conn, "COMMIT");
  return true;
}

bool sell_tank_by_login(PGconn *conn, const char *login, int h_id,
                        int s_price) {
  PGresult *res;

  if (s_price < 0) {
    fprintf(stderr, "Invalid price value: %d\n", s_price);
    return false;
  }

  PQexec(conn, "BEGIN");

  const char *check_ownership_query =
      "SELECT h.player_id "
      "FROM hangars h "
      "JOIN players p ON h.player_id = p.player_id "
      "WHERE h.hangar_id = $1 "
      "  AND p.login = $2 "
      "  AND h.is_sold = FALSE";

  char hangar_id[20];
  snprintf(hangar_id, 20, "%d", h_id);
  char sell_price[20];
  snprintf(sell_price, 20, "%d", s_price);

  const char *params[2] = {hangar_id, login};

  res =
      PQexecParams(conn, check_ownership_query, 2, NULL, params, NULL, NULL, 0);
  if (PQresultStatus(res) != PGRES_TUPLES_OK || PQntuples(res) == 0) {
    PQclear(res);
    PQexec(conn, "ROLLBACK");
    fprintf(stderr, "Ownership check failed\n");
    return false;
  }

  int p_id = atoi(PQgetvalue(res, 0, 0));

  char player_id[20];
  snprintf(player_id, 20, "%d", p_id);

  PQclear(res);

  const char *mark_sold_query = "UPDATE hangars SET "
                                "is_sold = TRUE, "
                                "status = 'needs_repair' "
                                "WHERE hangar_id = $1";

  const char *mark_params[1] = {hangar_id};
  res =
      PQexecParams(conn, mark_sold_query, 1, NULL, mark_params, NULL, NULL, 0);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    PQclear(res);
    PQexec(conn, "ROLLBACK");
    fprintf(stderr, "Mark as sold failed\n");
    return false;
  }
  PQclear(res);

  const char *add_balance_query =
      "UPDATE players SET currency_amount = currency_amount + $1 "
      "WHERE player_id = $2";

  const char *add_params[2] = {sell_price, player_id};
  res =
      PQexecParams(conn, add_balance_query, 2, NULL, add_params, NULL, NULL, 0);
  if (PQresultStatus(res) != PGRES_COMMAND_OK) {
    PQclear(res);
    PQexec(conn, "ROLLBACK");
    fprintf(stderr, "Balance update failed\n");
    return false;
  }
  PQclear(res);

  PQexec(conn, "COMMIT");
  return true;
}
