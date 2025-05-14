#ifndef HANGAR_H
#define HANGAR_H

#include <libpq-fe.h>
#include <stdbool.h>

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

#endif
