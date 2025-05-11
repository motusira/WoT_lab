#include "../include/ui.h"
#include "../include/matches.h"
#include "../include/players.h"
#include <libpq-fe.h>

#define UI_LINUX
#define UI_IMPLEMENTATION

#include "../luigi/luigi.h"

PGconn *conn;

UIButton *button, *find_button, *clear_button, *login_button, *select_login_button,
    *make_match_button, *update_matches_button;
UILabel *label;
UIWindow *window;
UIPanel *login_parent, *login, *player_info, *pi_ui, *pi_result, *players_list, *match_making,
    *reports;
UITabPane *admin_pane;
UITextbox *pi_input, *login_input;
UITable *players_table, *matches_table;

int pl_count;
int id = 1, rating;
int selected = -1, match_selected = -1;
Player *pl;

int matches_count;
Match *matches;

char * user;

void draw_info(PGconn *conn, const char *l) {
  const char *query = "SELECT p.player_id, "
                      "p.status AS player_status, "
                      "h.status AS hangar_status, "
                      "ti.tier, "
                      "p.currency_amount, "
                      "p.total_damage, "
                      "p.destroyed_vehicles, "
                      "h.tank_id, "
                      "ti.type, "
                      "m.mod_id, "
                      "h.game_points "
                      "FROM players p "
                      "JOIN hangars h USING(player_id) "
                      "JOIN tanks t USING(tank_id) "
                      "JOIN tank_info ti ON t.data_id = ti.data_id "
                      "JOIN modifications m ON t.mod_id = m.mod_id "
                      "WHERE p.login = $1";

  const char *params[1] = {l};

  PGresult *res = PQexecParams(conn, query, 1, NULL, params, NULL, NULL, 0);

  char buff[1024];

  if (PQresultStatus(res) != PGRES_TUPLES_OK) {
    fprintf(stderr, "Query failed: %s\n", PQerrorMessage(conn));
    PQclear(res);
    return;
  }

  int rows = PQntuples(res);
  if (rows == 0) {
    snprintf(buff, 1024, "No vehicles found for player: %s", l);
    UILabelCreate(&pi_result->e, 0, buff, -1);
    return;
  }

  snprintf(buff, 1024, "Login: %s", l);
  UILabelCreate(&pi_result->e, 0, buff, -1);
  snprintf(buff, 1024, "ID: %s", PQgetvalue(res, 0, 0));
  UILabelCreate(&pi_result->e, 0, buff, -1);
  snprintf(buff, 1024, "Player Status: %s", PQgetvalue(res, 0, 1));
  UILabelCreate(&pi_result->e, 0, buff, -1);
  snprintf(buff, 1024, "Currency amount: %s", PQgetvalue(res, 0, 4));
  UILabelCreate(&pi_result->e, 0, buff, -1);
  snprintf(buff, 1024, "Total damage: %s", PQgetvalue(res, 0, 5));
  UILabelCreate(&pi_result->e, 0, buff, -1);
  snprintf(buff, 1024, "Destroyed vehicles: %s", PQgetvalue(res, 0, 6));
  UILabelCreate(&pi_result->e, 0, buff, -1);

  snprintf(buff, 1024, "| %-8s | %-5s | %-14s | %-12s | %-15s | %-10s |",
           "Tank ID", "Tier", "Hangar Status", "Type", "Modification",
           "Points");
  UILabelCreate(&pi_result->e, 0, buff, -1);

  for (int i = 0; i < rows; i++) {
    snprintf(buff, 1024, "| %-8s | %-5s | %-14s | %-12s | %-15s | %-10s |",
             PQgetvalue(res, i, 7),   // tank_id
             PQgetvalue(res, i, 3),   // tier
             PQgetvalue(res, i, 2),   // hangar_status
             PQgetvalue(res, i, 8),   // type
             PQgetvalue(res, i, 9),   // mod_id
             PQgetvalue(res, i, 10)); // game_points
    UILabelCreate(&pi_result->e, 0, buff, -1);
  }

  PQclear(res);
}

void update_pl(PGconn *conn) {
  free_players(pl, pl_count);
  pl = fetch_all_players(conn, &pl_count);
}

void update_matches(PGconn *conn) {
  free_matches(matches);
  matches = fetch_all_matches(conn, &matches_count);
}

int UpdateMatchesButtonMessage(UIElement *element, UIMessage message, int di,
                               void *dp);

int MatchesTableMessage(UIElement *element, UIMessage message, int di,
                        void *dp);
int MakeMatchButtonMessage(UIElement *element, UIMessage message, int di,
                           void *dp);
int PlayersTableMessage(UIElement *element, UIMessage message, int di,
                        void *dp);
int ClearButtonMessage(UIElement *element, UIMessage message, int di,
                       void *dp);
int FindButtonMessage(UIElement *element, UIMessage message, int di, void *dp);
int PIInputMessage(UIElement *element, UIMessage message, int di, void *dp);

void as_admin(void) {
  admin_pane = UITabPaneCreate(
      &window->e, 0, "Player info\tPlayers\tMatch making\tMatches\tReports");

  player_info =
      UIPanelCreate(&admin_pane->e, UI_PANEL_COLOR_1 | UI_PANEL_MEDIUM_SPACING |
                                        UI_PANEL_SCROLL);
  pi_ui =
      UIPanelCreate(&player_info->e, UI_PANEL_COLOR_1 | UI_PANEL_HORIZONTAL);
  pi_result = UIPanelCreate(&player_info->e,
                            UI_PANEL_COLOR_1 | UI_PANEL_MEDIUM_SPACING);
  pi_input = UITextboxCreate(&pi_ui->e, 0);
  pi_input->e.cp = (void *)conn;
  pi_input->e.messageUser = PIInputMessage;
  find_button = UIButtonCreate(&pi_ui->e, 0, "Find", -1);
  find_button->e.messageUser = FindButtonMessage;
  clear_button = UIButtonCreate(&pi_ui->e, 0, "Clear", -1);
  clear_button->e.messageUser = ClearButtonMessage;

  players_table = UITableCreate(&admin_pane->e, UI_ELEMENT_H_FILL,
                                "ID\tRating\tLogin\tStatus\tCurrency "
                                "amount\tTotal damage\tDestroyed vehicles");
  players_table->e.cp = (void *)pl;
  players_table->e.messageUser = PlayersTableMessage;
  players_table->itemCount = pl_count;
  UITableResizeColumns(players_table);

  match_making =
      UIPanelCreate(&admin_pane->e, UI_PANEL_COLOR_1 | UI_PANEL_MEDIUM_SPACING);
  make_match_button = UIButtonCreate(&match_making->e, 0, "Create match", -1);
  make_match_button->e.cp = conn;
  make_match_button->e.messageUser = MakeMatchButtonMessage;
  update_matches_button =
      UIButtonCreate(&match_making->e, 0, "Update matches", -1);
  update_matches_button->e.cp = conn;
  update_matches_button->e.messageUser = UpdateMatchesButtonMessage;

  matches = fetch_all_matches(conn, &matches_count);
  matches_table =
      UITableCreate(&admin_pane->e, UI_ELEMENT_H_FILL,
                    "Match ID\tStart time\tResult\tTech tier\tPlayer 1\tPlayer "
                    "2\tPlayer 3\tPlayer 4\tPlayer 5\tPlayer 6");
  matches_table->e.cp = (void *)matches;
  matches_table->e.messageUser = MatchesTableMessage;
  matches_table->itemCount = matches_count;
  UITableResizeColumns(matches_table);

  reports =
      UIPanelCreate(&admin_pane->e, UI_PANEL_COLOR_1 | UI_PANEL_MEDIUM_SPACING);
}

void as_user(void) {
  UIPanel *up = UIPanelCreate(&window->e, UI_PANEL_COLOR_1);
  char buff[512];
  snprintf(buff, 512, "Logged as %s", user);
  UILabelCreate(&up->e, 0, buff, -1);
}

void process_login(void) {
  if (!strcmp(user, "admin")) {
    as_admin();
  } else {
    as_user();
  }
}

int WindowMessage(UIElement *element, UIMessage message, int di, void *dp) {
  if (message == UI_MSG_PRESSED_DESCENDENT) {
    UIElementRelayout(element);
    UIElementRefresh(element);
  }
  return 0;
}

int LoginButtonMessage(UIElement *element, UIMessage message, int di,
                       void *dp) {
  if (message == UI_MSG_CLICKED) {
    UIElementDestroy(element);
    UIElementDestroy(element->parent);
    UIElementDestroy(&login_parent->e);
    process_login();
  }
  return 0;
}

void LoginMenuCallback(void *cp) {
  UIButtonSetLabel(select_login_button, cp, -1);
  user = cp;
}

int SelectLoginButtonMessage(UIElement *element, UIMessage message, int di,
                       void *dp) {
  if (message == UI_MSG_CLICKED) {
    UIMenu *login_menu = UIMenuCreate(element, 0);
    UIMenuAddItem(login_menu, 0, "admin", -1, LoginMenuCallback, "admin");
    for (int i = 0; i < pl_count; i++) {
      UIMenuAddItem(login_menu, 0, pl[i].login, -1, LoginMenuCallback, pl[i].login);
    }
    UIMenuShow(login_menu);
  }
  return 0;
}

int PIInputMessage(UIElement *element, UIMessage message, int di, void *dp) {
  if (message == UI_MSG_KEY_TYPED) {
    UIKeyTyped *k = (UIKeyTyped *)dp;
    if (k->code == UI_KEYCODE_ENTER) {
      draw_info(pi_input->e.cp, UITextboxToCString(pi_input));
      UITextboxClear(pi_input, 0);
    }
  } else if (message == UI_MSG_USER) {
    draw_info(pi_input->e.cp, UITextboxToCString(pi_input));
    UITextboxClear(pi_input, 0);
  }
  return 0;
}

int FindButtonMessage(UIElement *element, UIMessage message, int di, void *dp) {
  if (message == UI_MSG_CLICKED) {
    UIElementMessage(&pi_input->e, UI_MSG_USER, 0, NULL);
  }
  return 0;
}

int ClearButtonMessage(UIElement *element, UIMessage message, int di,
                       void *dp) {
  if (message == UI_MSG_CLICKED) {
    UIElementDestroyDescendents(&pi_result->e);
  }
  return 0;
}

int PlayersTableMessage(UIElement *element, UIMessage message, int di,
                        void *dp) {
  if (message == UI_MSG_TABLE_GET_ITEM) {
    UITableGetItem *m = (UITableGetItem *)dp;
    m->isSelected = selected == m->index;
    switch (m->column) {
    case 0:
      return snprintf(m->buffer, m->bufferBytes, "%d", pl[m->index].player_id);
    case 1:
      return snprintf(m->buffer, m->bufferBytes, "%d", pl[m->index].rating);
    case 2:
      return snprintf(m->buffer, m->bufferBytes, "%s", pl[m->index].login);
    case 3:
      return snprintf(m->buffer, m->bufferBytes, "%s", pl[m->index].status);
    case 4:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      pl[m->index].currency_amount);
    case 5:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      pl[m->index].total_damage);
    case 6:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      pl[m->index].destroyed_vehicles);
    }
  } else if (message == UI_MSG_LEFT_DOWN) {
    int hit = UITableHeaderHitTest((UITable *)element, element->window->cursorX,
                                   element->window->cursorY);
    switch (hit) {
    case 0:
      id = id == 0 ? 1 : 0;
      sort_players(pl, pl_count, BY_ID, id);
      break;
    case 1:
      rating = rating == 0 ? 1 : 0;
      sort_players(pl, pl_count, BY_RATING, rating);
      break;
    }

    int el_hit = UITableHitTest((UITable *)element, element->window->cursorX,
                                element->window->cursorY);
    if (selected != el_hit) {
      selected = el_hit;

      if (!UITableEnsureVisible((UITable *)element, selected)) {
        UIElementRepaint(element, NULL);
      }
    }
  }
  return 0;
}

int MakeMatchButtonMessage(UIElement *element, UIMessage message, int di,
                           void *dp) {
  if (message == UI_MSG_CLICKED) {
    find_and_create_match(element->cp);
    update_pl(element->cp);
    update_matches(conn);
  }
  return 0;
}

int UpdateMatchesButtonMessage(UIElement *element, UIMessage message, int di,
                               void *dp) {
  if (message == UI_MSG_CLICKED) {
    process_completed_matches(element->cp);
    update_pl(element->cp);
    update_matches(conn);
  }
  return 0;
}

int MatchesTableMessage(UIElement *element, UIMessage message, int di,
                        void *dp) {
  if (message == UI_MSG_TABLE_GET_ITEM) {
    UITableGetItem *m = (UITableGetItem *)dp;
    m->isSelected = match_selected == m->index;
    char *login;
    int res;
    switch (m->column) {
    case 0:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      matches[m->index].match_id);
    case 1:
      return snprintf(m->buffer, m->bufferBytes, "%s",
                      matches[m->index].start_time);
    case 2:
      return snprintf(m->buffer, m->bufferBytes, "%s",
                      matches[m->index].result == 1   ? "Team 1 won "
                      : matches[m->index].result == 2 ? "Team 2 won"
                      : matches[m->index].result == 0 ? "Draw"
                                                      : "In progress");
    case 3:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      matches[m->index].tech_level);
    case 4:
      login = get_nickname_by_participant_id(
          conn, matches[m->index].participant_ids[0]);
      res = snprintf(m->buffer, m->bufferBytes, "%s", login);
      free(login);
      return res;
    case 5:
      login = get_nickname_by_participant_id(
          conn, matches[m->index].participant_ids[1]);
      res = snprintf(m->buffer, m->bufferBytes, "%s", login);
      free(login);
      return res;
    case 6:
      login = get_nickname_by_participant_id(
          conn, matches[m->index].participant_ids[2]);
      res = snprintf(m->buffer, m->bufferBytes, "%s", login);
      free(login);
      return res;
    case 7:
      login = get_nickname_by_participant_id(
          conn, matches[m->index].participant_ids[3]);
      res = snprintf(m->buffer, m->bufferBytes, "%s", login);
      free(login);
      return res;
    case 8:
      login = get_nickname_by_participant_id(
          conn, matches[m->index].participant_ids[4]);
      res = snprintf(m->buffer, m->bufferBytes, "%s", login);
      free(login);
      return res;
    case 9:
      login = get_nickname_by_participant_id(
          conn, matches[m->index].participant_ids[5]);
      res = snprintf(m->buffer, m->bufferBytes, "%s", login);
      free(login);
      return res;
    }
  } else if (message == UI_MSG_LEFT_DOWN) {
    int el_hit = UITableHitTest((UITable *)element, element->window->cursorX,
                                element->window->cursorY);
    if (match_selected != el_hit) {
      match_selected = el_hit;

      if (!UITableEnsureVisible((UITable *)element, match_selected)) {
        UIElementRepaint(element, NULL);
      }
    }
  }
  return 0;
}

void init() {
  UIInitialise();
  ui.theme = uiThemeClassic;
  UIFontActivate(UIFontCreate("font.ttf", 16));

  pl = fetch_all_players(conn, &pl_count);

  window = UIWindowCreate(0, 0, "WoT", 640, 480);
  window->e.messageUser = WindowMessage;

  login_parent = UIPanelCreate(&window->e, UI_PANEL_COLOR_1 | UI_PANEL_MEDIUM_SPACING);
  login = UIPanelCreate(&login_parent->e, UI_PANEL_COLOR_1 | UI_PANEL_HORIZONTAL);
  login_button = UIButtonCreate(&login->e, 0, "Login as", -1);
  login_button->e.messageUser = LoginButtonMessage;
  select_login_button = UIButtonCreate(&login->e, 0, "Select login", -1);
  select_login_button->e.messageUser = SelectLoginButtonMessage;

}

void ui_start(PGconn *cn) {
  conn = cn;
  init();
  UIMessageLoop();
  free_players(pl, pl_count);
  free_matches(matches);
}
