#ifndef PLAYERS_TABLE_H
#define PLAYERS_TABLE_H

#include <libpq-fe.h>
#include <stdbool.h>

typedef enum { SORT_ASC, SORT_DESC } SortOrder;

typedef enum { BY_ID, BY_RATING } SortCriteria;

typedef struct {
  int player_id;
  char *login;
  char *status;
  int currency_amount;
  int total_damage;
  int destroyed_vehicles;
  int rating;
} Player;

bool create_players_table(PGconn *conn);
bool insert_random_players(PGconn *conn, int count);
bool fill_players_table(PGconn *conn);
bool clear_players_table(PGconn *conn);
void get_player_vehicles(PGconn *conn, const char *login);
void free_players(Player *players, int count);
Player *fetch_all_players(PGconn *conn, int *player_count);
void sort_players(Player *players, int count, SortCriteria criteria,
                  SortOrder order);
bool create_player(PGconn *conn, const char *login);

#endif
