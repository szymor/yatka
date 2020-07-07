#ifndef _H_MAIN
#define _H_MAIN

#include <stdbool.h>
#include <SDL/SDL.h>

#if defined _BITTBOY
#define KEY_LEFT			SDLK_LEFT
#define KEY_RIGHT			SDLK_RIGHT
#define KEY_SOFTDROP		SDLK_DOWN
#define KEY_HARDDROP		SDLK_LALT
#define KEY_ROTATE_CW		SDLK_LCTRL
#define KEY_ROTATE_CCW		SDLK_SPACE
#define KEY_HOLD			SDLK_LSHIFT
#define KEY_PAUSE			SDLK_RETURN
#define KEY_QUIT			SDLK_ESCAPE
#elif defined _RETROFW
#define KEY_LEFT			SDLK_LEFT
#define KEY_RIGHT			SDLK_RIGHT
#define KEY_SOFTDROP		SDLK_DOWN
#define KEY_HARDDROP		SDLK_LCTRL
#define KEY_ROTATE_CW		SDLK_LALT
#define KEY_ROTATE_CCW		SDLK_LSHIFT
#define KEY_HOLD			SDLK_SPACE
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
#define KEY_PAUSE			SDLK_RETURN
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
	GS_MAINMENU,
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

enum TetrominoColor
{
	TC_RANDOM,
	TC_TENGEN,
	TC_STANDARD,
	TC_GRAY,
	TC_END
};

enum TetrominoStyle
{
	TS_LEGACY,
	TS_PLAIN,
	TS_TENGENISH,
	TS_END
};

SDL_Surface *initBackground(void);
void incMod(int *var, int limit, bool sat);
void decMod(int *var, int limit, bool sat);

extern enum GameState gamestate;

extern bool nosound;
extern bool holdoff;
extern bool grayblocks;
extern bool debug;
extern bool repeattrack;
extern bool numericbars;
extern bool easyspin;
extern bool lockdelay;
extern bool smoothanim;

extern enum TetrominoColor tetrominocolor;
extern int ghostalpha;
extern int nextblocks;
extern enum TetrominoStyle tetrominostyle;

void resetGame(void);

#endif
