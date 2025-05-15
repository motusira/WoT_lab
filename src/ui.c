#include "../include/ui.h"
#include "../include/hangar.h"
#include "../include/matches.h"
#include "../include/players.h"
#include <libpq-fe.h>

PGconn *conn;

UIButton *button, *find_button, *clear_button, *login_button,
    *select_login_button, *make_match_button, *update_matches_button,
    *repair_button, *upgrade_button, *sell_button, *buy_button, *logout_button,
    *play_button, *register_button, *report_selection_button;
UILabel *label, *player_currency, *selected_tank_from_hangar,
    *selected_tank_to_buy, *repair_cost_label, *sell_price_label,
    *buy_price_label;
UIWindow *window;
UIPanel *login_parent, *login, *player_info, *pi_ui, *pi_result, *players_list,
    *match_making, *user_panel_parent, *user_login_panel,
    *user_panel_hangar_actions, *user_panel_buy_actions, *admin_logout,
    *register_panel, *report_chose_panel;
UITabPane *admin_pane;
UITextbox *pi_input, *register_input;
UITable *players_table, *matches_table, *player_hangar_table,
    *player_can_buy_table, *report_table;
UISplitPane *hangar, *actions, *buy, *reports;

int pl_count;
int id = 1, rating = 1, currency = 1, damage = 1, destroyed = 1;
int selected = -1, match_selected = -1;
Player *pl;

int matches_count;
Match *matches = NULL;

char *user = NULL;

int tanks_in_hangar, can_buy_tanks;
TankInfo *tanks = NULL, *buyable_tanks = NULL;
int hangar_tank_selected = -1, buy_selected = -1;
int pc;

int chosen_report_type;
int report_selected = -1;

GamesStat *gs = NULL;
int gs_count;

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
  if (pl != NULL) {
    free_players(pl, pl_count);
  }
  pl = fetch_all_players(conn, &pl_count);
}

void update_matches(PGconn *conn) {
  if (matches != NULL) {
    free(matches);
  }
  matches = fetch_all_matches(conn, &matches_count);
  matches_table->itemCount = matches_count;
  UITableResizeColumns(matches_table);
}


int WLDTableMessage(UIElement *element, UIMessage message, int di,
                        void *dp) {
  if (message == UI_MSG_TABLE_GET_ITEM) {
    UITableGetItem *m = (UITableGetItem *)dp;
    m->isSelected = report_selected == m->index;
    switch (m->column) {
    case 0:
      return snprintf(m->buffer, m->bufferBytes, "%d", gs[m->index].player_id);
    case 1:
      return snprintf(m->buffer, m->bufferBytes, "%s", gs[m->index].login);
    case 2:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      gs[m->index].wins);
    case 3:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      gs[m->index].losses);
    case 4:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      gs[m->index].draws);
    }
  } else if (message == UI_MSG_LEFT_DOWN) {
    int hit = UITableHeaderHitTest((UITable *)element, element->window->cursorX,
                                   element->window->cursorY);
    switch (hit) {
    }

    int el_hit = UITableHitTest((UITable *)element, element->window->cursorX,
                                element->window->cursorY);
    if (report_selected != el_hit) {
      report_selected = el_hit;

      if (!UITableEnsureVisible((UITable *)element, report_selected)) {
        UIElementRepaint(element, NULL);
      }
    }
  }
  return 0;
}

void ReportsMenuCallback(void *cp) {
  UIButtonSetLabel(report_selection_button, cp, -1);
  if (!strcmp(cp, "Wins/Loses/Drafts")) {
    chosen_report_type = 0;
    gs = get_player_stats(conn, &gs_count);
    UIElementDestroy(&report_table->e);
    report_table = UITableCreate(&reports->e, 0, "ID\tLogin\tWins\tLoses\tDrafts");
    report_table->e.messageUser = WLDTableMessage;
    report_table->itemCount = gs_count;
    UITableResizeColumns(report_table);
    return;
  }
  if (!strcmp(cp, "Tier 1 stats")) {
    chosen_report_type = 1;
    return;
  }
  if (!strcmp(cp, "Tier 2 stats")) {
    chosen_report_type = 2;
    return;
  }
  if (!strcmp(cp, "Tier 3 stats")) {
    chosen_report_type = 3;
    return;
  }
  if (!strcmp(cp, "Tier 4 stats")) {
    chosen_report_type = 4;
    return;
  }
  if (!strcmp(cp, "Tier 5 stats")) {
    chosen_report_type = 5;
    return;
  }
}

int ReportSelectButtonMessage(UIElement *element, UIMessage message, int di,
                             void *dp) {
  if (message == UI_MSG_CLICKED) {
    UIMenu *report_menu = UIMenuCreate(element, 0);
    UIMenuAddItem(report_menu, 0, "Wins/Loses/Drafts", -1, ReportsMenuCallback, "Wins/Loses/Drafts");
    UIMenuAddItem(report_menu, 0, "Tier 1 stats", -1, ReportsMenuCallback, "Tier 1 stats");
    UIMenuAddItem(report_menu, 0, "Tier 2 stats", -1, ReportsMenuCallback, "Tier 2 stats");
    UIMenuAddItem(report_menu, 0, "Tier 3 stats", -1, ReportsMenuCallback, "Tier 3 stats");
    UIMenuAddItem(report_menu, 0, "Tier 4 stats", -1, ReportsMenuCallback, "Tier 4 stats");
    UIMenuAddItem(report_menu, 0, "Tier 5 stats", -1, ReportsMenuCallback,"Tier 5 stats");
    UIMenuShow(report_menu);
  }
  return 0;
}


void as_admin(void) {
  admin_pane = UITabPaneCreate(
      &window->e, 0,
      "Player info\tPlayers\tMatch making\tMatches\tReports\tLogout");

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
  update_pl(conn);
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

  matches_table =
      UITableCreate(&admin_pane->e, UI_ELEMENT_H_FILL,
                    "Match ID\tStart time\tResult\tTech tier\tPlayer 1\tPlayer "
                    "2\tPlayer 3\tPlayer 4\tPlayer 5\tPlayer 6");
  matches_table->e.cp = (void *)matches;
  matches_table->e.messageUser = MatchesTableMessage;
  update_matches(conn);
  matches_table->itemCount = matches_count;
  UITableResizeColumns(matches_table);

  reports = UISplitPaneCreate(&admin_pane->e, UI_SPLIT_PANE_VERTICAL, .10f);

  report_chose_panel = UIPanelCreate(&reports->e, UI_PANEL_COLOR_1);
  report_selection_button = UIButtonCreate(&report_chose_panel->e, 0, "Select report type", -1);
  report_selection_button->e.messageUser = ReportSelectButtonMessage;
  report_table = UITableCreate(&reports->e, 0, "");

  admin_logout = UIPanelCreate(&admin_pane->e, UI_PANEL_COLOR_1);
  logout_button = UIButtonCreate(&admin_logout->e, 0, "Logout", -1);
  logout_button->e.messageUser = LogoutButtonMessage;
}

int PlayerHangarTableMessage(UIElement *element, UIMessage message, int di,
                             void *dp) {
  if (message == UI_MSG_TABLE_GET_ITEM) {
    UITableGetItem *m = (UITableGetItem *)dp;
    m->isSelected = hangar_tank_selected == m->index;
    char buff[256];
    switch (m->column) {
    case 0:
      return snprintf(m->buffer, m->bufferBytes, "%d", tanks[m->index].tank_id);
    case 1:
      return snprintf(m->buffer, m->bufferBytes, "%d", tanks[m->index].tier);
    case 2:
      return snprintf(m->buffer, m->bufferBytes, "%s", tanks[m->index].country);
    case 3:
      return snprintf(m->buffer, m->bufferBytes, "%s",
                      tanks[m->index].hangar_status);
    case 4:
      return snprintf(m->buffer, m->bufferBytes, "%s", tanks[m->index].type);
    case 5:
      return snprintf(m->buffer, m->bufferBytes, "%d", tanks[m->index].mod_id);
    case 6:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      tanks[m->index].game_points);
    case 7:
      return snprintf(m->buffer, m->bufferBytes, "%d", tanks[m->index].price);
    }
  } else if (message == UI_MSG_LEFT_DOWN) {
    int el_hit = UITableHitTest((UITable *)element, element->window->cursorX,
                                element->window->cursorY);
    if (hangar_tank_selected != el_hit) {
      hangar_tank_selected = el_hit;
      char buff[128];
      if (hangar_tank_selected >= 0) {
        snprintf(buff, 128, "Selected %d tank",
                 tanks[hangar_tank_selected].tank_id);
        UILabelSetContent(selected_tank_from_hangar, buff, -1);
      } else {
        UILabelSetContent(selected_tank_from_hangar, "No tank selected.", -1);
      }
      get_repair_cost();
      get_sell_price();
      // UIElementRefresh(&window->e);

      if (!UITableEnsureVisible((UITable *)element, hangar_tank_selected)) {
        UIElementRepaint(element, NULL);
      }
    }
  }
  return 0;
}

int PlayerCanBuyTableMessage(UIElement *element, UIMessage message, int di,
                             void *dp) {
  if (message == UI_MSG_TABLE_GET_ITEM) {
    UITableGetItem *m = (UITableGetItem *)dp;
    m->isSelected = buy_selected == m->index;
    char buff[256];
    switch (m->column) {
    case 0:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      buyable_tanks[m->index].tank_id);
    case 1:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      buyable_tanks[m->index].tier);
    case 2:
      return snprintf(m->buffer, m->bufferBytes, "%s",
                      buyable_tanks[m->index].country);
    case 3:
      return snprintf(m->buffer, m->bufferBytes, "%s",
                      buyable_tanks[m->index].type);
    case 4:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      buyable_tanks[m->index].mod_id);
    case 5:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      buyable_tanks[m->index].required_points);
    case 6:
      return snprintf(m->buffer, m->bufferBytes, "%d",
                      buyable_tanks[m->index].price);
    }
  } else if (message == UI_MSG_LEFT_DOWN) {
    int el_hit = UITableHitTest((UITable *)element, element->window->cursorX,
                                element->window->cursorY);
    if (buy_selected != el_hit) {
      buy_selected = el_hit;
      char buff[128];
      if (buy_selected >= 0) {
        snprintf(buff, 128, "Selected %d tank",
                 buyable_tanks[buy_selected].tank_id);
        UILabelSetContent(selected_tank_to_buy, buff, -1);
        snprintf(buff, 128, "Price: %d", buyable_tanks[buy_selected].price);
        UILabelSetContent(buy_price_label, buff, -1);
      } else {
        UILabelSetContent(selected_tank_to_buy, "No tank selected.", -1);
        UILabelSetContent(buy_price_label, "", -1);
      }

      if (!UITableEnsureVisible((UITable *)element, buy_selected)) {
        UIElementRepaint(element, NULL);
      }
    }
  }
  return 0;
}

void update_tanks() {
  if (tanks != NULL) {
    free(tanks);
  }
  if (buyable_tanks != NULL) {
    free(buyable_tanks);
  }
  tanks = get_player_tanks(conn, user, &tanks_in_hangar);
  buyable_tanks = get_available_tanks(conn, user, &can_buy_tanks);
}

int RepairButtonMessage(UIElement *element, UIMessage message, int di,
                        void *dp) {
  if (message == UI_MSG_CLICKED) {
    if (hangar_tank_selected >= 0 &&
        !strcmp(tanks[hangar_tank_selected].hangar_status, "needs_repair")) {
      repair_tank_by_login(conn, user, tanks[hangar_tank_selected].hangar_id,
                           tanks[hangar_tank_selected].price / 4);
      get_player_currency(user);
      update_tanks();
      UIElementRefresh(&window->e);
    }
  }
  return 0;
}

int SellButtonMessage(UIElement *element, UIMessage message, int di, void *dp) {
  if (message == UI_MSG_CLICKED) {
    if (hangar_tank_selected >= 0) {
      sell_tank_by_login(conn, user, tanks[hangar_tank_selected].hangar_id,
                         tanks[hangar_tank_selected].price / 5 * 4);
      get_player_currency(user);
      update_tanks();
      hangar_tank_selected = -1;
      UILabelSetContent(selected_tank_from_hangar, "No tank selected.", -1);
      player_hangar_table->itemCount = tanks_in_hangar;
      UITableResizeColumns(player_hangar_table);
      player_can_buy_table->itemCount = can_buy_tanks;
      UITableResizeColumns(player_can_buy_table);
      UIElementRefresh(&window->e);
      UIElementRefresh(&window->e);
    }
  }
  return 0;
}

int RegisterButtonMessage(UIElement *element, UIMessage message, int di,
                          void *dp) {
  if (message == UI_MSG_CLICKED) {
    char *b = UITextboxToCString(register_input);
    if (strcmp(b, "admin")) {
      if (create_player(conn, b)) {
        update_pl(conn);
        for (int i = 0; i < pl_count; i++) {
          if (!strcmp(b, pl[i].login)) {
            user = pl[i].login;
            break;
          }
        }
        UIElementDestroyDescendents(&window->e);
        process_login();
      }
    }
    UI_FREE(b);
    UITextboxClear(register_input, false);
  }
  return 0;
}

void init_login() {
  login_parent =
      UIPanelCreate(&window->e, UI_PANEL_COLOR_1 | UI_PANEL_MEDIUM_SPACING);
  login =
      UIPanelCreate(&login_parent->e, UI_PANEL_COLOR_1 | UI_PANEL_HORIZONTAL);
  login_button = UIButtonCreate(&login->e, 0, "Login as", -1);
  login_button->e.messageUser = LoginButtonMessage;
  select_login_button = UIButtonCreate(&login->e, 0, "Select login", -1);
  select_login_button->e.messageUser = SelectLoginButtonMessage;
  UISpacerCreate(&login_parent->e, 0, 1, 50);
  register_panel =
      UIPanelCreate(&login_parent->e, UI_PANEL_COLOR_1 | UI_PANEL_HORIZONTAL);
  register_input = UITextboxCreate(&register_panel->e, 0);
  register_button = UIButtonCreate(&register_panel->e, 0, "Create account", -1);
  register_button->e.messageUser = RegisterButtonMessage;
}

int LogoutButtonMessage(UIElement *element, UIMessage message, int di,
                        void *dp) {
  if (message == UI_MSG_CLICKED) {
    UIElementDestroyDescendents(&window->e);
    hangar_tank_selected = -1;
    user = NULL;
    init_login();
  }
  return 0;
}

int BuyButtonMessage(UIElement *element, UIMessage message, int di, void *dp) {
  if (message == UI_MSG_CLICKED) {
    if (buy_selected >= 0) {
      int error = buy_tank(conn, user, buyable_tanks[buy_selected].tank_id,
                           buyable_tanks[buy_selected].mod_id);

      if (error == NOT_ENOUGH_CURRENCY) {
        UILabelSetContent(buy_price_label, "Not enough currency amount!", -1);
      } else if (error == NOT_ENOUGH_POINTS) {
        UILabelSetContent(buy_price_label, "Not enough game points amout!", -1);
      } else if (error == SUCCESS) {
        UILabelSetContent(buy_price_label, "", -1);
      }
      UILabelSetContent(selected_tank_to_buy, "No tank selected.", -1);

      get_player_currency(user);
      update_tanks();
      buy_selected = -1;
      player_hangar_table->itemCount = tanks_in_hangar;
      UITableResizeColumns(player_hangar_table);

      player_can_buy_table->itemCount = can_buy_tanks;
      UITableResizeColumns(player_can_buy_table);
      UIElementRefresh(&window->e);
    }
  }
  return 0;
}

void get_player_currency(const char *l) {
  const char *params[1] = {l};
  PGresult *res = PQexecParams(
      conn, "SELECT p.currency_amount FROM players p WHERE p.login = $1", 1,
      NULL, params, NULL, NULL, 0);
  char buff[512];
  snprintf(buff, 512, "Currency amount: %s", PQgetvalue(res, 0, 0));
  UILabelSetContent(player_currency, buff, -1);
}

void get_repair_cost() {
  if (hangar_tank_selected >= 0) {
    char buff[512];
    snprintf(buff, 512, "Repair cost: %d",
             tanks[hangar_tank_selected].price / 4);
    UILabelSetContent(repair_cost_label, buff, -1);
  } else {
    UILabelSetContent(repair_cost_label, "", -1);
  }
}

void get_sell_price() {
  if (hangar_tank_selected >= 0) {
    char buff[512];
    snprintf(buff, 512, "Sell price: %d",
             tanks[hangar_tank_selected].price / 5 * 4);
    UILabelSetContent(sell_price_label, buff, -1);
  } else {
    UILabelSetContent(sell_price_label, "", -1);
  }
}

void as_user(void) {
  hangar = UISplitPaneCreate(&window->e, 0, 0.50f);

  update_tanks();
  player_hangar_table =
      UITableCreate(&hangar->e, 0,
                    "Tank ID\tTier\tCountry\tStatus\tType\tModification\tGame "
                    "points\tPrice (Sell)");
  player_hangar_table->e.messageUser = PlayerHangarTableMessage;
  player_hangar_table->itemCount = tanks_in_hangar;
  UITableResizeColumns(player_hangar_table);

  actions = UISplitPaneCreate(&hangar->e, UI_SPLIT_PANE_VERTICAL, 0.50f);

  char buff[512];
  user_panel_parent =
      UIPanelCreate(&actions->e, UI_PANEL_COLOR_1 | UI_PANEL_MEDIUM_SPACING);

  player_can_buy_table = UITableCreate(
      &actions->e, 0,
      "Tank ID\tTier\tCountry\tType\tModification\tRequired points\tPrice");
  player_can_buy_table->e.messageUser = PlayerCanBuyTableMessage;
  player_can_buy_table->itemCount = can_buy_tanks;
  UITableResizeColumns(player_can_buy_table);

  user_login_panel = UIPanelCreate(&user_panel_parent->e,
                                   UI_PANEL_COLOR_1 | UI_PANEL_HORIZONTAL);

  UILabelCreate(&user_login_panel->e, 0, user, -1);
  UISpacerCreate(&user_login_panel->e, 0, 30, 1);

  logout_button = UIButtonCreate(&user_login_panel->e, 0, "Logout", -1);
  logout_button->e.messageUser = LogoutButtonMessage;

  player_currency = UILabelCreate(&user_panel_parent->e, 0, "", -1);
  get_player_currency(user);

  UISpacerCreate(&user_panel_parent->e, 0, 1, 20);

  selected_tank_from_hangar =
      UILabelCreate(&user_panel_parent->e, 0, "No tank selected.", -1);

  user_panel_hangar_actions = UIPanelCreate(
      &user_panel_parent->e, UI_PANEL_COLOR_1 | UI_PANEL_HORIZONTAL);

  repair_button =
      UIButtonCreate(&user_panel_hangar_actions->e, 0, "Repair", -1);
  repair_button->e.messageUser = RepairButtonMessage;
  sell_button = UIButtonCreate(&user_panel_hangar_actions->e, 0, "Sell", -1);
  sell_button->e.messageUser = SellButtonMessage;

  repair_cost_label = UILabelCreate(&user_panel_parent->e, 0, "", -1);
  sell_price_label = UILabelCreate(&user_panel_parent->e, 0, "", -1);

  UISpacerCreate(&user_panel_parent->e, 0, 1, 20);

  selected_tank_to_buy =
      UILabelCreate(&user_panel_parent->e, 0, "No tank selected.", -1);

  user_panel_buy_actions = UIPanelCreate(
      &user_panel_parent->e, UI_PANEL_COLOR_1 | UI_PANEL_HORIZONTAL);

  buy_button =
      UIButtonCreate(&user_panel_buy_actions->e, 0, "Buy selected vehicle", -1);
  buy_button->e.messageUser = BuyButtonMessage;

  buy_price_label = UILabelCreate(&user_panel_parent->e, 0, "", -1);
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
    if (user != NULL) {
      UIElementDestroyDescendents(&window->e);
      process_login();
    }
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
      UIMenuAddItem(login_menu, 0, pl[i].login, -1, LoginMenuCallback,
                    pl[i].login);
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
    case 4:
      currency = currency == 0 ? 1 : 0;
      sort_players(pl, pl_count, BY_CURRENCY_AMOUNT, currency);
      break;
    case 5:
      damage = damage == 0 ? 1 : 0;
      sort_players(pl, pl_count, BY_DAMAGE, damage);
      break;
    case 6:
      destroyed = destroyed == 0 ? 1 : 0;
      sort_players(pl, pl_count, BY_DESTROYED_VEHICLES, destroyed);
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
  UIFontActivate(UIFontCreate("font.ttf", 18));

  pl = fetch_all_players(conn, &pl_count);

  window = UIWindowCreate(0, 0, "WoT", 640 * 2, 480 * 2);
  window->e.messageUser = WindowMessage;

  init_login();
}

void ui_start(PGconn *cn) {
  conn = cn;
  init();
  UIMessageLoop();
  free_players(pl, pl_count);
  free_matches(matches);
  free(tanks);
}
