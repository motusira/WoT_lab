#ifndef MATCHES_H
#define MATCHES_H

#include <libpq-fe.h>
#include <stdbool.h>

bool create_participants_table(PGconn *conn);
bool create_matches_table(PGconn *conn);
int find_and_create_match(PGconn *conn);
void process_completed_matches(PGconn *conn);

#endif
