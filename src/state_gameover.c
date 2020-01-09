
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include "state_gameover.h"
#include "main.h"
#include "video.h"

void gameover_updateScreen(void)
{
	SDL_Surface *mask = NULL;

	SDL_PixelFormat *f = screen->format;
	SDL_BlitSurface(last_game_screen, NULL, screen, NULL);
	mask = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, f->BitsPerPixel, f->Rmask, f->Gmask, f->Bmask, 0);
	SDL_FillRect(mask, NULL, SDL_MapRGB(mask->format, 0, 0, 0));
	SDL_SetAlpha(mask, SDL_SRCALPHA, 128);
	SDL_BlitSurface(mask, NULL, screen, NULL);
	SDL_FreeSurface(mask);

	SDL_Color col = {.r = 255, .g = 255, .b = 255};
	SDL_Surface *text = NULL;
	SDL_Rect rect;
	char buff[256];

	sprintf(buff, "GAME OVER");
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	rect.x = (screen->w - text->w) / 2;
	rect.y = (screen->h - text->h) / 2;
	SDL_BlitSurface(text, NULL, screen, &rect);
	SDL_FreeSurface(text);

	flipScreenScaled();
}

void gameover_processInputEvents(void)
{
	SDL_Event event;

	if (SDL_WaitEvent(&event))
		switch (event.type)
		{
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case KEY_QUIT:
						{
							SDL_Event ev;
							ev.type = SDL_QUIT;
							SDL_PushEvent(&ev);
						}
						break;
				}
				break;
			case SDL_QUIT:
				exit(0);
				break;
		}
}
