#include "../include/ui.h"
#include "../include/players.h"
#include <libpq-fe.h>

#define UI_LINUX
#define UI_IMPLEMENTATION

#include "../luigi/luigi.h"

UIButton *button, *find_button, *clear_button, *login_button;
UILabel *label;
UIWindow *window;
UIPanel *login, *player_info, *pi_ui, *pi_result, *players_list;
UITabPane *admin_pane;
UITextbox *pi_input;
UITable *players_table;

int pl_count;
int id = 1, rating;
int selected = -1;
Player *pl;

void draw_info(PGconn *conn, const char *l) {
  const char *query =
      "SELECT p.player_id, p.status, p.currency_amount, p.total_damage, "
      "p.destroyed_vehicles, h.tank_id, ti.type, m.mod_id, h.game_points "
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
  snprintf(buff, 1024, "Status: %s", PQgetvalue(res, 0, 1));
  UILabelCreate(&pi_result->e, 0, buff, -1);
  snprintf(buff, 1024, "Currency amount: %s", PQgetvalue(res, 0, 2));
  UILabelCreate(&pi_result->e, 0, buff, -1);
  snprintf(buff, 1024, "Total damage: %s", PQgetvalue(res, 0, 3));
  UILabelCreate(&pi_result->e, 0, buff, -1);
  snprintf(buff, 1024, "Destroyed vehicles: %s", PQgetvalue(res, 0, 4));
  UILabelCreate(&pi_result->e, 0, buff, -1);
  snprintf(buff, 1024, "Currency amount: %s", PQgetvalue(res, 0, 2));
  UILabelCreate(&pi_result->e, 0, buff, -1);
  snprintf(buff, 1024, "| %-8s | %-12s | %-15s | %-10s |", "Tank ID", "Type",
           "Modification", "Points");
  UILabelCreate(&pi_result->e, 0, buff, -1);

  for (int i = 0; i < rows; i++) {
    snprintf(buff, 1024, "| %-8s | %-12s | %-15s | %-10s |",
             PQgetvalue(res, i, 5), PQgetvalue(res, i, 6),
             PQgetvalue(res, i, 7), PQgetvalue(res, i, 8));
    UILabelCreate(&pi_result->e, 0, buff, -1);
  }

  PQclear(res);
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

void init(PGconn *conn) {
  UIInitialise();
  ui.theme = uiThemeClassic;
  UIFontActivate(UIFontCreate("font.ttf", 18));

  window = UIWindowCreate(0, 0, "WoT", 640, 480);
  window->e.messageUser = WindowMessage;

  login = UIPanelCreate(&window->e, UI_PANEL_COLOR_1 | UI_PANEL_MEDIUM_SPACING);
  login_button = UIButtonCreate(&login->e, 0, "Login", -1);
  login_button->e.messageUser = LoginButtonMessage;

  admin_pane = UITabPaneCreate(&window->e, 0, "Player info\tPlayers");

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

  pl = fetch_all_players(conn, &pl_count);
  players_table = UITableCreate(&admin_pane->e, UI_ELEMENT_H_FILL,
                                "ID\tRating\tLogin\tStatus\tCurrency "
                                "amount\tTotal damage\tDestroyed vehicles");
  players_table->e.cp = (void *)pl;
  players_table->e.messageUser = PlayersTableMessage;
  players_table->itemCount = pl_count;
  UITableResizeColumns(players_table);
}

void ui_start(PGconn *conn) {
  init(conn);
  UIMessageLoop();
  free_players(pl, pl_count);
}
