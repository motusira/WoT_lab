#include "../include/hangar.h"
#include "../include/utils.h"

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
  PGresult *res = PQexec(conn, "CREATE TABLE IF NOT EXISTS tanks ("
                               "tank_id SERIAL PRIMARY KEY,"
                               "base_data_id INT NOT NULL REFERENCES "
                               "tank_info(data_id) ON DELETE CASCADE,"
                               "modification_id INT NOT NULL REFERENCES "
                               "modifications(mod_id) ON DELETE RESTRICT,"
                               "UNIQUE(base_data_id, modification_id)"
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

bool fill_tanks_table(PGconn *conn) { return true; }
