#ifndef _H_SKIN
#define _H_SKIN

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>

#define FONT_NUM	8

enum FigureId
{
	FIGID_I,
	FIGID_O,
	FIGID_T,
	FIGID_S,
	FIGID_Z,
	FIGID_J,
	FIGID_L,
	FIGID_GRAY,
	FIGID_I_CYAN,
	FIGID_O_YELLOW,
	FIGID_T_PURPLE,
	FIGID_S_GREEN,
	FIGID_Z_RED,
	FIGID_J_BLUE,
	FIGID_L_ORANGE,
	FIGID_END
};

enum BrickStyle
{
	BS_SIMPLE,
	BS_ORIENTATION_BASED,
	BS_END
};

struct Skin
{
	SDL_Surface *screen;
	SDL_Surface *bg;
	SDL_Surface *fg;
	SDL_Surface *bricksprite[FIGID_END];
	enum BrickStyle brickstyle;
	enum FigureId debriscolor;
	char *script;
	char *path;
	int boardx;
	int boardy;
	int bricksize;
	TTF_Font *fonts[FONT_NUM];
};

void skin_initSkin(struct Skin *skin);
void skin_destroySkin(struct Skin *skin);
void skin_loadSkin(struct Skin *skin, const char *path);
void skin_updateScreen(struct Skin *skin, SDL_Surface *screen);
void skin_updateBackground(struct Skin *skin);

#endif
