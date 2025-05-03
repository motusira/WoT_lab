#include "../include/hangar.h"
#include "../include/utils.h"
#include <math.h>
#include <stdlib.h>

bool create_hangars_table(PGconn *conn) {
  PGresult *res = PQexec(
      conn,
      "CREATE TABLE IF NOT EXISTS hangars ("
      "hangar_id SERIAL PRIMARY KEY,"
      "player_id INT NOT NULL REFERENCES players(player_id) ON DELETE CASCASE,"
      "tank_id INT NOT NULL REFERENCES tanks(tank_id),"
      "game_points INT NOT NULL DEFAULT 0,"
      "status VARCHAR(20) NOT NULL CHECK(status IN ('operational', "
      "'needs_repair')),"
      "UNIQUE(player_id, tank_id)"
      ")");
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

      PGresult *insert_res =
          PQexecParams(conn,
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
