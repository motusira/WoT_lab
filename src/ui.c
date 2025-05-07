#include "../include/ui.h"

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

int ButtonMessage(UIElement *element, UIMessage message, int di, void *dp) {
  if (message == UI_MSG_CLICKED)
    printf("Clicked!\n");
  return 0;
}

int LoginButtonMessage(UIElement *element, UIMessage message, int di,
                       void *dp) {
  if (message == UI_MSG_CLICKED) {
    UIElementDestroy(element);
    UIElementDestroy(element->parent);
    puts("update login");
  }
  return 0;
}

int WindowMessage(UIElement *element, UIMessage message, int di, void *dp) {
  if (message == UI_MSG_PRESSED_DESCENDENT || message == UI_MSG_RIGHT_UP) {
    UIElementRelayout(element);
    UIElementRefresh(element);
    puts("redrawed");
  }
  return 0;
}

int PaneMessage(UIElement *element, UIMessage message, int di, void *dp) {
  if (message == UI_MSG_CLICKED) {
    UIElementRefresh(element->parent);
    UIElementRepaint(element->parent, NULL);
    puts("update pane");
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
  pane = UITabPaneCreate(&window->e, 0, "tab1\ttab2");
  UIPanel *panel =
      UIPanelCreate(&pane->e, UI_PANEL_COLOR_1 | UI_PANEL_MEDIUM_SPACING);
  button = UIButtonCreate(&panel->e, 0, "Push", -1);
  button->e.messageUser = ButtonMessage;
  label = UILabelCreate(&panel->e, 0, "Some text...", -1);
  UIPanel *panel2 =
      UIPanelCreate(&pane->e, UI_PANEL_COLOR_1 | UI_PANEL_SMALL_SPACING);
  UILabel *label2 = UILabelCreate(&panel2->e, 0, "Some text...", -1);
  UILabel *label3 = UILabelCreate(&panel2->e, 0, "Some text...", -1);
  UILabel *label4 = UILabelCreate(&panel2->e, 0, "Some text...", -1);
  UIMessageLoop();
}
