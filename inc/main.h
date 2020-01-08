#ifndef _H_MAIN
#define _H_MAIN

#include <stdbool.h>
#include <SDL/SDL.h>

#ifdef _BITTBOY
#define KEY_LEFT			SDLK_LEFT
#define KEY_RIGHT			SDLK_RIGHT
#define KEY_SOFTDROP		SDLK_DOWN
#define KEY_HARDDROP		SDLK_LALT
#define KEY_ROTATE_CW		SDLK_LCTRL
#define KEY_ROTATE_CCW		SDLK_SPACE
#define KEY_HOLD			SDLK_LSHIFT
#define KEY_PAUSE			SDLK_RETURN
#define KEY_QUIT			SDLK_ESCAPE
#else
#define KEY_LEFT			SDLK_LEFT
#define KEY_RIGHT			SDLK_RIGHT
#define KEY_SOFTDROP		SDLK_DOWN
#define KEY_HARDDROP		SDLK_SPACE
#define KEY_ROTATE_CW		SDLK_UP
#define KEY_ROTATE_CCW		SDLK_z
#define KEY_HOLD			SDLK_c
#define KEY_PAUSE			SDLK_p
#define KEY_QUIT			SDLK_ESCAPE
#endif

#define FIG_NUM				7
#define MAX_NEXTBLOCKS		(FIG_NUM - 1)

enum Error
{
	ERROR_NONE,
	ERROR_SDLINIT,
	ERROR_SDLVIDEO,
	ERROR_NOIMGFILE,
	ERROR_NOSNDFILE,
	ERROR_OPENAUDIO,
	ERROR_TTFINIT,
	ERROR_NOFONT,
	ERROR_MIXINIT,
	ERROR_END
};

enum GameState
{
	GS_INGAME,
	GS_SETTINGS,
	GS_GAMEOVER,
	GS_END
};

enum FigureId
{
	FIGID_I,
	FIGID_O,
	FIGID_T,
	FIGID_S,
	FIGID_Z,
	FIGID_J,
	FIGID_L,
	FIGID_END
};

enum Skin
{
	SKIN_LEGACY,
	SKIN_PLAIN,
	SKIN_TENGENISH,
	SKIN_END
};

SDL_Surface *initBackground(void);

extern enum GameState gamestate;

extern bool nosound;
extern bool randomcolors;
extern bool holdoff;
extern bool grayblocks;
extern bool ghostoff;
extern bool debug;
extern bool repeattrack;
extern bool numericbars;
extern bool easyspin;
extern bool lockdelay;
extern bool smoothanim;

extern int startlevel;
extern int nextblocks;
extern enum Skin skin;

#endif
