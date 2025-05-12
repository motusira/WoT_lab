#ifndef UI_H
#define UI_H

#include <libpq-fe.h>

#define UI_LINUX
#define UI_IMPLEMENTATION
#include "../luigi/luigi.h"


// buttons
int UpdateMatchesButtonMessage(UIElement *element, UIMessage message, int di,
                               void *dp);
int MakeMatchButtonMessage(UIElement *element, UIMessage message, int di,
                           void *dp);
int ClearButtonMessage(UIElement *element, UIMessage message, int di, void *dp);
int FindButtonMessage(UIElement *element, UIMessage message, int di, void *dp);
int LoginButtonMessage(UIElement *element, UIMessage message, int di, void *dp);
int SelectLoginButtonMessage(UIElement *element, UIMessage message, int di,
                             void *dp);
int LogoutButtonMessage(UIElement *element, UIMessage message, int di,
                        void *dp);
int RepairButtonMessage(UIElement *element, UIMessage message, int di,
                        void *dp);
int SellButtonMessage(UIElement *element, UIMessage message, int di, void *dp);

// tables
int PlayerHangarTableMessage(UIElement *element, UIMessage message, int di,
                             void *dp);
int MatchesTableMessage(UIElement *element, UIMessage message, int di,
                        void *dp);
int PlayersTableMessage(UIElement *element, UIMessage message, int di,
                        void *dp);

// textboxes
int PIInputMessage(UIElement *element, UIMessage message, int di, void *dp);

// menu
void LoginMenuCallback(void *cp);

// window
int WindowMessage(UIElement *element, UIMessage message, int di, void *dp);

// utils and other
void draw_info(PGconn *conn, const char *l);
void update_pl(PGconn *conn);
void update_matches(PGconn *conn);
void update_tanks(void);
void as_admin(void);
void as_user(void);
void get_player_currency(const char *l);
void get_repair_cost(void);
void get_sell_price(void);
void init_login(void);
void process_login(void);


#endif
