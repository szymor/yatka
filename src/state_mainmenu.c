
#include <stdio.h>
#include <stdbool.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include <sys/types.h>
#include <dirent.h>

#include "state_mainmenu.h"
#include "main.h"
#include "video.h"

#define MAX_SKIN_NUM		32
#define MAX_SKIN_NAME_LEN	16
#define POSITION_NUM		4

int menu_skinnum = 0;
char menu_skinnames[MAX_SKIN_NUM][MAX_SKIN_NAME_LEN];
int menu_skin = 0;
int menu_level = 0;
int menu_debris = 0;
int menu_debris_chance = 8;

static int submenu_index = 0;

static void text(int x, int y, const char *string, int alignx, int aligny);

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

void mainmenu_init(void)
{
	DIR *dp;
	struct dirent *ep;

	dp = opendir("skins/");
	if (dp != NULL)
	{
		while (ep = readdir(dp))
		{
			if (strcmp(ep->d_name, ".") && strcmp(ep->d_name, ".."))
			{
				if (!strcmp(ep->d_name, "default"))
					menu_skin = menu_skinnum;
				strcpy(menu_skinnames[menu_skinnum], ep->d_name);
				++menu_skinnum;
			}
		}
		closedir(dp);
	}
	else
		perror ("Couldn't open the directory");
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
		sprintf(buff, "< %s >", menu_skinnames[menu_skin]);
	else
		sprintf(buff, "  %s  ", menu_skinnames[menu_skin]);
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

	flipScreenScaled();
}

void mainmenu_processInputEvents(void)
{
	SDL_Event event;

	if (SDL_WaitEvent(&event))
		switch (event.type)
		{
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case SDLK_UP:
						decMod(&submenu_index, POSITION_NUM, false);
						break;
					case SDLK_RIGHT:
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
					} break;
					case SDLK_DOWN:
						incMod(&submenu_index, POSITION_NUM, false);
						break;
					case SDLK_LEFT:
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
					} break;
					case KEY_PAUSE:
					{
						char path[256];
						sprintf(path, "skins/%s/game.txt", menu_skinnames[menu_skin]);
						skin_loadSkin(&gameskin, path);
						resetGame();
						gamestate = GS_INGAME;
						submenu_index = 0;
					} break;
					case KEY_QUIT:
					{
						SDL_Event ev;
						ev.type = SDL_QUIT;
						SDL_PushEvent(&ev);
					} break;
				}
				break;
			case SDL_QUIT:
				exit(0);
				break;
		}
}
