#include "../include/ui.h"
#include "players.h"

#define UI_LINUX
#define UI_IMPLEMENTATION

#include "../luigi/luigi.h"

UIButton *button;
UILabel *label;

UIWindow *window;
UIPanel *login;
UIButton *login_button;

UITabPane *pane;
UIPanel *panel;

int LoginButtonMessage(UIElement *element, UIMessage message, int di,
                       void *dp) {
  if (message == UI_MSG_CLICKED) {
    UIElementDestroy(element);
    UIElementDestroy(element->parent);
  }
  return 0;
}

int WindowMessage(UIElement *element, UIMessage message, int di, void *dp) {
  if (message == UI_MSG_PRESSED_DESCENDENT || message == UI_MSG_RIGHT_UP) {
    UIElementRelayout(element);
    UIElementRefresh(element);
  }
  return 0;
}

void draw_info(PGconn *conn, const char *l) {
  const char *query =
      "SELECT p.player_id, h.tank_id, ti.type, m.mod_id, h.game_points "
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
    UILabelCreate(&panel->e, 0, buff, -1);
    return;
  }

  snprintf(buff, 1024, "Player login: %s | Player ID: %s", l,
           PQgetvalue(res, 0, 0));
  UILabelCreate(&panel->e, 0, buff, -1);
  snprintf(buff, 1024, "| %-8s | %-12s | %-15s | %-10s |", "Tank ID", "Type",
           "Modification", "Points");
  UILabelCreate(&panel->e, 0, buff, -1);

  for (int i = 0; i < rows; i++) {
    snprintf(buff, 1024, "| %-8s | %-12s | %-15s | %-10s |",
             PQgetvalue(res, i, 1), PQgetvalue(res, i, 2),
             PQgetvalue(res, i, 3), PQgetvalue(res, i, 4));
    UILabelCreate(&panel->e, 0, buff, -1);
  }

  PQclear(res);
}

UITextbox *textbox;

int TextBoxMessage(UIElement *element, UIMessage message, int di, void *dp) {
  if (message == UI_MSG_KEY_TYPED) {
    UIKeyTyped *k = (UIKeyTyped *) dp;
    if(k->code == UI_KEYCODE_ENTER) {
      draw_info(textbox->e.cp, UITextboxToCString(textbox));
    }
  }
  return 0;
}

void ui_start(PGconn *conn) {
  UIInitialise();
  ui.theme = uiThemeClassic;
  window = UIWindowCreate(0, UI_ELEMENT_PARENT_PUSH, "WoT", 640, 480);
  window->e.messageUser = WindowMessage;
  login = UIPanelCreate(0, UI_PANEL_COLOR_1 | UI_PANEL_MEDIUM_SPACING);
  login_button = UIButtonCreate(&login->e, 0, "Login", -1);
  login_button->e.messageUser = LoginButtonMessage;
  pane = UITabPaneCreate(&window->e, 0, "tab1");
  panel = UIPanelCreate(&pane->e, UI_PANEL_COLOR_1 | UI_PANEL_MEDIUM_SPACING | UI_PANEL_SCROLL);
  textbox = UITextboxCreate(&panel->e, 0);
  textbox->e.cp = (void *) conn;
  textbox->e.messageUser = TextBoxMessage;
  UIMessageLoop();
}
