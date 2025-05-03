#ifndef PLAYERS_TABLE_H
#define PLAYERS_TABLE_H

#include <libpq-fe.h>
#include <stdbool.h>

bool create_players_table(PGconn *conn);
bool insert_random_players(PGconn *conn, int count);
bool fill_players_table(PGconn *conn);
bool clear_players_table(PGconn *conn);

#endif
