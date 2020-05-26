#ifndef _H_STATE_MAINMENU
#define _H_STATE_MAINMENU

extern int menu_speed;
extern int menu_level;
extern int menu_debris;

void mainmenu_init(void);
void mainmenu_processInputEvents(void);
void mainmenu_updateScreen(void);

#endif
