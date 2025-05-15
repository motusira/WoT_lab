#ifndef PLAYERS_TABLE_H
#define PLAYERS_TABLE_H

#include <libpq-fe.h>
#include <stdbool.h>

typedef enum { SORT_ASC, SORT_DESC } SortOrder;

typedef enum { BY_ID, BY_RATING, BY_DAMAGE, BY_DESTROYED_VEHICLES, BY_CURRENCY_AMOUNT } SortCriteria;

typedef enum {
    BY_WINS,
    BY_LOSSES,
    BY_DRAWS
} GamesStatCriteria;

typedef struct {
    int player_id;
    char login[50];
    int wins;
    int losses;
    int draws;
} GamesStat;

typedef struct {
    int player_id;
    char *login;
    int total_damage;
    int destroyed_vehicles;
} PlayerTechStats;

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
GamesStat *get_player_stats(PGconn *conn, int *count);
void sort_player_stats(GamesStat *stats, int count, GamesStatCriteria criteria,
                       SortOrder order);

#endif
