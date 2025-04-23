#include <libpq-fe.h>
#include <stdlib.h>
#include <time.h>

#include "../include/players.h"
#include "../include/utils.h"

bool create_players_table(PGconn *conn) {
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
  return handle_res_command(conn, res);
}

bool clear_players_table(PGconn *conn) {
  PGresult *res =
      PQexec(conn, "TRUNCATE TABLE players RESTART IDENTITY CASCADE");

  return handle_res_command(conn, res);
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

  char count_str[20];

  snprintf(count_str, sizeof(count_str), "%d", count);

  PGresult *res =
      PQexecParams(conn, "SELECT 'Successfully inserted ' || $1 || ' players'",
                   1,    // 1 параметр
                   NULL, // типы параметров (NULL = автоматическое определение)
                   (const char *[]){count_str}, // значения параметров
                   NULL, // длины параметров (NULL = строки завершаются нулем)
                   NULL, // форматы параметров (0 = текст, 1 = бинарный)
                   0);   // формат результата (0 = текст)
  
  return handle_res_tuples(conn, res);
}
