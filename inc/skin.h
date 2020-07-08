#ifndef _H_SKIN
#define _H_SKIN

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#define FONT_NUM	8

struct Skin
{
	SDL_Surface *screen;
	SDL_Surface *bg;
	SDL_Surface *fg;
	char *script;
	char *path;
	int boardx;
	int boardy;
	TTF_Font *fonts[FONT_NUM];
};

void skin_initSkin(struct Skin *skin);
void skin_destroySkin(struct Skin *skin);
void skin_loadSkin(struct Skin *skin, const char *path);
void skin_updateScreen(struct Skin *skin, SDL_Surface *screen);
void skin_updateBackground(struct Skin *skin);

#endif
