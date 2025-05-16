#ifndef MATCHES_H
#define MATCHES_H

#include <libpq-fe.h>
#include <stdbool.h>

typedef struct {
  int match_id;
  char start_time[20];
  int result;
  int tech_level;
  int participant_ids[6];
} Match;

bool create_participants_table(PGconn *conn);
bool create_matches_table(PGconn *conn);
int find_and_create_match(PGconn *conn);
void process_completed_matches(PGconn *conn);
Match *fetch_all_matches(PGconn *conn, int *match_count);
void free_matches(Match *matches);
char *get_nickname_by_participant_id(PGconn *conn, int participant_id);
int create_match_with_tech_level(PGconn *conn, int tech_level);
bool generate_match_for_player(PGconn *conn, const char *login, int tank_id);
int get_last_match_result(PGconn *conn, const char *login);

#endif
