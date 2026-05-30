#ifndef _H_STATE_MAINMENU
#define _H_STATE_MAINMENU

#include "main.h"

#define MAX_SKIN_NUM		32
#define MAX_SKIN_NAME_LEN	16
#define MAX_SKIN_PATH_LEN	256

struct SkinEntry
{
	char name[MAX_SKIN_NAME_LEN];
	char path[MAX_SKIN_PATH_LEN];
};

extern struct SkinEntry menu_skinentries[MAX_SKIN_NUM];
extern int menu_skinnum;
extern int menu_skin;
extern int menu_gamemode;
extern int menu_level;
extern int menu_debris;
extern int menu_debris_chance;
extern int menu_auto_debris;

void mainmenu_init(void);
void mainmenu_processInputEvents(void);
void mainmenu_updateScreen(void);

#endif
