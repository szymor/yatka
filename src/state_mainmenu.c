
#include <stdio.h>
#include <stdbool.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#include "state_mainmenu.h"
#include "main.h"
#include "video.h"

#define ROUND_CORNER	(15)

static SDL_Surface *menubg = NULL;
static int submenu_index = 0;

void mainmenu_init(void)
{
	if (menubg == NULL)
	{
		menubg = IMG_Load("gfx/menubg.png");
		if (menubg == NULL)
			exit(ERROR_NOIMGFILE);
	}
}

void mainmenu_updateScreen(void)
{
	SDL_BlitSurface(menubg, NULL, screen, NULL);

	SDL_Color col = {.r = 255, .g = 255, .b = 255};
	SDL_Surface *text = NULL;
	SDL_Rect rect;
	char buff[256];

	sprintf(buff, "SPEED");
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	rect.x = 165 + ROUND_CORNER;
	rect.y = 10 + ROUND_CORNER;
	SDL_BlitSurface(text, NULL, screen, &rect);
	SDL_FreeSurface(text);

	sprintf(buff, "LEVEL");
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	rect.x = 165 + ROUND_CORNER;
	rect.y = 125 + ROUND_CORNER;
	SDL_BlitSurface(text, NULL, screen, &rect);
	SDL_FreeSurface(text);

	sprintf(buff, "DEBRIS");
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	rect.x = 10 + ROUND_CORNER;
	rect.y = 125 + ROUND_CORNER;
	SDL_BlitSurface(text, NULL, screen, &rect);
	SDL_FreeSurface(text);

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
					case SDLK_RIGHT:
					{
					} break;
					case SDLK_DOWN:
					case SDLK_LEFT:
					{
					} break;
					case KEY_PAUSE:
					{
						if (submenu_index == 2)
						{
							resetGame();
							gamestate = GS_INGAME;
							submenu_index = 0;
						}
						else
						{
							++submenu_index;
						}
					} break;
					case KEY_QUIT:
					{
						if (submenu_index == 0)
						{
							SDL_Event ev;
							ev.type = SDL_QUIT;
							SDL_PushEvent(&ev);
						}
						else
						{
							--submenu_index;
						}
					} break;
				}
				break;
			case SDL_QUIT:
				exit(0);
				break;
		}
}
