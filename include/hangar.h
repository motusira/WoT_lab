#ifndef HANGAR_H
#define HANGAR_H

#include <libpq-fe.h>
#include <stdbool.h>

bool create_hangars_table(PGconn *conn);
bool create_modifications_table(PGconn *conn);
bool create_tank_info_table(PGconn *conn);
bool create_tanks_table(PGconn *conn);
bool fill_hangars_table(PGconn *conn);
bool fill_tank_info_table(PGconn *conn);
bool fill_modifications_table(PGconn *conn);
bool fill_tanks_table(PGconn *conn);

#endif
