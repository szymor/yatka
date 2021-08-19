
#include <stdio.h>
#include <stdbool.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include <sys/types.h>
#include <dirent.h>

#include "state_mainmenu.h"
#include "main.h"
#include "joystick.h"
#include "video.h"
#include "data_persistence.h"

#define MAX_SKIN_NUM		32
#define MAX_SKIN_NAME_LEN	16
#define MAX_SKIN_PATH_LEN	256
#define POSITION_NUM		5

enum KeyId
{
	KI_LEFT,
	KI_RIGHT,
	KI_SOFTDROP,
	KI_HARDDROP,
	KI_ROTATE_CW,
	KI_ROTATE_CCW,
	KI_HOLD,
	KI_PAUSE,
	KI_QUIT,
	KI_END
};

struct SkinEntry
{
	char name[MAX_SKIN_NAME_LEN];
	char path[MAX_SKIN_PATH_LEN];
};

int menu_skinnum = 0;	// number of all detected skins
struct SkinEntry menu_skinentries[MAX_SKIN_NUM];
int menu_skin = 0;		// currently selected skin
int menu_level = 0;
int menu_debris = 0;
int menu_debris_chance = 8;

static char custom_skin_dir[256] = "";
static int submenu_index = 0;

static void up(void);
static void down(void);
static void left(void);
static void right(void);
static void action(void);
static void quit(void);
static void text(int x, int y, const char *string, int alignx, int aligny);
static SDLKey getKey(void);

static SDLKey getKey(void)
{
	SDL_Event event;
	while (SDL_WaitEvent(&event))
	{
		if (SDL_KEYDOWN == event.type)
		{
			return event.key.keysym.sym;
		}
	}
	return SDLK_ESCAPE;
}

static void text(int x, int y, const char *string, int alignx, int aligny)
{
	SDL_Color col = {.r = 255, .g = 255, .b = 255};
	SDL_Surface *ts = NULL;
	SDL_Rect rect;
	ts = TTF_RenderUTF8_Blended(arcade_font, string, col);
	rect.x = x;
	rect.y = y;
	if (1 == alignx)
		rect.x -= ts->w / 2;
	else if (2 == alignx)
		rect.x -= ts->w;
	if (1 == aligny)
		rect.y -= ts->h / 2;
	else if (2 == aligny)
		rect.y -= ts->h;
	SDL_BlitSurface(ts, NULL, screen, &rect);
	SDL_FreeSurface(ts);
}

static void mainmenu_init_skinload_helper(DIR *dp, const char *maindir)
{
	struct dirent *ep;
	while (ep = readdir(dp))
	{
		if (strcmp(ep->d_name, ".") && strcmp(ep->d_name, ".."))
		{
			if (!strcmp(ep->d_name, "default"))
				menu_skin = menu_skinnum;
			strcpy(menu_skinentries[menu_skinnum].name, ep->d_name);
			sprintf(menu_skinentries[menu_skinnum].path, "%s%s/game.txt", maindir, ep->d_name);
			++menu_skinnum;
		}
	}
}

void mainmenu_init(void)
{
	DIR *dp;

	menu_skinnum = 0;
	sprintf(custom_skin_dir, "%s/skins/", dirpath);

	dp = opendir(custom_skin_dir);
	if (dp != NULL)
	{
		mainmenu_init_skinload_helper(dp, custom_skin_dir);
		closedir(dp);
	}

	dp = opendir("skins/");
	if (dp != NULL)
	{
		mainmenu_init_skinload_helper(dp, "skins/");
		closedir(dp);
	}
	else
		perror("Couldn't open the main skin directory.");
}

void mainmenu_updateScreen(void)
{
	char buff[256];

	SDL_FillRect(screen, NULL, 0);

	text(16, 16, "YATKA", 0, 0);
	text(320, 240, "build " __DATE__ " " __TIME__, 2, 2);

	text(16, 48, "SKIN", 0, 0);
	text(16, 64, "LEVEL", 0, 0);
	text(16, 80, "DEBRIS", 0, 0);
	text(16, 96, "DEBRIS CHANCE", 0, 0);

	if (0 == submenu_index)
		sprintf(buff, "< %s >", menu_skinentries[menu_skin].name);
	else
		sprintf(buff, "  %s  ", menu_skinentries[menu_skin].name);
	text(120, 48, buff, 0, 0);

	if (1 == submenu_index)
		sprintf(buff, "< %d >", menu_level);
	else
		sprintf(buff, "  %d  ", menu_level);
	text(120, 64, buff, 0, 0);

	if (2 == submenu_index)
		sprintf(buff, "< %d >", menu_debris);
	else
		sprintf(buff, "  %d  ", menu_debris);
	text(120, 80, buff, 0, 0);

	if (3 == submenu_index)
		sprintf(buff, "< %d >", menu_debris_chance);
	else
		sprintf(buff, "  %d  ", menu_debris_chance);
	text(120, 96, buff, 0, 0);

	if (4 == submenu_index)
		sprintf(buff, "> KEY CONFIGURATION <", menu_debris_chance);
	else
		sprintf(buff, "  KEY CONFIGURATION  ", menu_debris_chance);
	text(16, 128, buff, 0, 0);

	flipScreenScaled();
}

static void up(void)
{
	decMod(&submenu_index, POSITION_NUM, false);
}

static void down(void)
{
	incMod(&submenu_index, POSITION_NUM, false);
}

static void left(void)
{
	int *option = NULL;
	int limit = 10;
	switch (submenu_index)
	{
		case 0:
			option = &menu_skin;
			limit = menu_skinnum;
			decMod(option, limit, false);
			break;
		case 1:
			option = &menu_level;
			decMod(option, limit, true);
			break;
		case 2:
			option = &menu_debris;
			limit = 16;
			decMod(option, limit, true);
			break;
		case 3:
			option = &menu_debris_chance;
			decMod(option, limit, true);
			break;
	}
}

static void right(void)
{
	int *option = NULL;
	int limit = 10;
	switch (submenu_index)
	{
		case 0:
			option = &menu_skin;
			limit = menu_skinnum;
			incMod(option, limit, false);
			break;
		case 1:
			option = &menu_level;
			incMod(option, limit, true);
			break;
		case 2:
			option = &menu_debris;
			limit = 16;
			incMod(option, limit, true);
			break;
		case 3:
			option = &menu_debris_chance;
			incMod(option, limit, true);
			break;
	}
}

static void action(void)
{
	// key configuration
	if (4 == submenu_index)
	{
		const int xpos = 16;
		const int ypos = 144;
		const int ydelta = 8;
		SDLKey keys[KI_END];
		char keyfunc[KI_END][32] = {
			"  LEFT",
			"  RIGHT",
			"  SOFT DROP",
			"  HARD DROP",
			"  ROTATE CLOCKWISE",
			"  ROTATE COUNTERCLOCKWISE",
			"  HOLD",
			"  PAUSE",
			"  QUIT"
		};

		for (int i = 0; i < KI_END; ++i)
		{
			// fiddling around with flip fixes display issues on some devices
			flipScreenScaled();
			text(xpos, ypos + ydelta * i, keyfunc[i], 0, 0);
			flipScreenScaled();
			keys[i] = getKey();
			flipScreenScaled();
			text(xpos + 200, ypos + ydelta * i, SDL_GetKeyName(keys[i]), 0, 0);
			flipScreenScaled();
		}
		flipScreenScaled();
		text(xpos, ypos + ydelta * (KI_END + 1), "IS IT OK? (Y = UP, N = DOWN)", 0, 0);
		flipScreenScaled();
		SDLKey answer;
		do
		{
			answer = getKey();
		} while (SDLK_UP != answer && SDLK_DOWN != answer);
		if (SDLK_UP == answer)
		{
			kleft = keys[KI_LEFT];
			kright = keys[KI_RIGHT];
			ksoftdrop = keys[KI_SOFTDROP];
			kharddrop = keys[KI_HARDDROP];
			krotatecw = keys[KI_ROTATE_CW];
			krotateccw = keys[KI_ROTATE_CCW];
			khold = keys[KI_HOLD];
			kpause = keys[KI_PAUSE];
			kquit = keys[KI_QUIT];
			saveSettings();
		}
	}
	else
	{
		skin_destroySkin(&gameskin);
		skin_initSkin(&gameskin);
		skin_loadSkin(&gameskin, menu_skinentries[menu_skin].path);
		resetGame();
		gamestate = GS_INGAME;
		submenu_index = 0;
	}
}

static void quit(void)
{
	SDL_Event ev;
	ev.type = SDL_QUIT;
	SDL_PushEvent(&ev);
}

void mainmenu_processInputEvents(void)
{
	SDL_Event event;

	if (SDL_WaitEvent(&event))
		switch (event.type)
		{
			case SDL_JOYAXISMOTION:
				if ((event.jaxis.value < -JOY_THRESHOLD) || (event.jaxis.value > JOY_THRESHOLD))
				{
					if(event.jaxis.axis == 0)
					{
						if (event.jaxis.value < 0)
							left();
						else
							right();
					}

					if(event.jaxis.axis == 1)
					{
						if (event.jaxis.value < 0)
							up();
						else
							down();
					}
				}
				break;
			case SDL_JOYBUTTONDOWN:
				if (event.jbutton.button == JOY_PAUSE)
				{
					action();
				}
				if (event.jbutton.button == JOY_QUIT)
				{
					quit();
				}
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case SDLK_UP:
						up();
						break;
					case SDLK_RIGHT:
						right();
						break;
					case SDLK_DOWN:
						down();
						break;
					case SDLK_LEFT:
						left();
						break;
					default:
					{
						if (event.key.keysym.sym == kquit)
						{
							quit();
						}
						else if (event.key.keysym.sym == kpause)
						{
							action();
						}
					}
				}
				break;
			case SDL_QUIT:
				exit(0);
				break;
		}
}
