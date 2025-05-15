#ifndef HANGAR_H
#define HANGAR_H

#include <libpq-fe.h>
#include <stdbool.h>

#define CURRENCY_ERROR 1
#define POINTS_ERROR 2

#define SUCCESS 0
#define PLAYER_NOT_FOUND 1
#define TANK_NOT_FOUND 2
#define NOT_ENOUGH_CURRENCY 3
#define NOT_ENOUGH_POINTS 4
#define TANK_ALREADY_OWNED 5
#define UPDATE_ERROR 6
#define INSERT_ERROR 7

typedef struct {
  int tank_id;
  int tier;
  char country[50];
  char hangar_status[20];
  char type[20];
  int mod_id;
  int game_points;
  int price;
  int required_points;
  int hangar_id;
} TankInfo;

bool create_hangars_table(PGconn *conn);
bool create_modifications_table(PGconn *conn);
bool create_tank_info_table(PGconn *conn);
bool create_tanks_table(PGconn *conn);
bool fill_hangars_table(PGconn *conn);
bool fill_tank_info_table(PGconn *conn);
bool fill_modifications_table(PGconn *conn);
bool fill_tanks_table(PGconn *conn);
TankInfo *get_player_tanks(PGconn *conn, const char *login, int *count);
bool repair_tank_by_login(PGconn *conn, const char *login, int h_id,
                          int r_cost);
bool sell_tank_by_login(PGconn *conn, const char *login, int h_id, int t_price);
TankInfo *get_available_tanks(PGconn *conn, const char *login, int *count);
int buy_tank(PGconn *conn, const char *login, int t_id, int m_id);

#endif
