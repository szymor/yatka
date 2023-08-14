#ifndef _H_STATE_MAINMENU
#define _H_STATE_MAINMENU

#include "main.h"

extern int menu_gamemode;
extern int menu_level;
extern int menu_debris;
extern int menu_debris_chance;
extern int menu_auto_debris;

void mainmenu_init(void);
void mainmenu_processInputEvents(void);
void mainmenu_updateScreen(void);

#endif
