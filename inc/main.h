#ifndef _H_MAIN
#define _H_MAIN

#include <stdbool.h>
#include <SDL/SDL.h>

#include "skin.h"

#define xstr(s) str(s)
#define str(s) #s

#define log		/*printf*/

#ifndef COMMIT_HASH
#define COMMIT_HASH		unofficial build
#endif

// default keys
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

#define BOARD_WIDTH					10
#define BOARD_HEIGHT				21
#define INVISIBLE_ROW_COUNT			1
#define FIG_DIM						4

#define FIG_NUM				(7)
#define MAX_NEXTBLOCKS		(FIG_NUM - 1)
#define LCT_LEN				(64)
#define LCT_DEADLINE		(1500)
#define GAMETIMER_STRLEN	(16)

#define ULTRA_MS_LEN				(3*60*1000)
#define SPRINT_LINE_COUNT			(40)

enum GameMode
{
	GM_MARATHON,
	GM_SPRINT,
	GM_ULTRA,
	GM_END
};

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
	ERROR_MALLOC,
	ERROR_IO,
	ERROR_SCRIPT,
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

enum TetrominoColor
{
	TC_RANDOM,
	TC_STANDARD,
	TC_GRAY,
	TC_END
};

enum BlockOrientation
{
	// proper encoding allows to rotate blocks using shift operations
	BO_EMPTY	= 0x00,
	BO_UP		= 0x08,
	BO_DOWN		= 0x02,
	BO_LEFT		= 0x01,
	BO_RIGHT	= 0x04,
	BO_FULL		= 0x0f
};

struct Block
{
	enum FigureId color;
	enum BlockOrientation orientation;
};

struct Shape
{
	enum BlockOrientation blockmap[FIG_DIM*FIG_DIM];
	int cx, cy;		// correction of position after CW rotation
	int phase;
};

struct Figure
{
	enum FigureId id;
	enum FigureId color;
	struct Shape shape;
	int x;
	int y;
};

extern struct Figure *figures[FIG_NUM];
extern struct Figure preserved;
extern struct Skin gameskin;
extern struct Block *board;
extern enum GameState gamestate;

extern bool nosound;
extern bool repeattrack;
extern bool easyspin;
extern bool lockdelay;
extern bool smoothanim;
extern bool speechon;

extern enum TetrominoColor tetrominocolor;
extern int ghostalpha;

extern Uint32 next_lock_time;
extern Uint32 last_drop_time;
extern int draw_delta_drop;
extern int brick_size;

extern int score;
extern int lines;
extern int level;
extern int tetris_count;
extern int ttr;
extern int statistics[FIGID_GRAY];

// LC - line clear
extern char lctext_top[LCT_LEN];
extern char lctext_mid[LCT_LEN];
extern char lctext_bot[LCT_LEN];

extern char gametimer[GAMETIMER_STRLEN];

// key settings
extern int kleft;
extern int kright;
extern int ksoftdrop;
extern int kharddrop;
extern int krotatecw;
extern int krotateccw;
extern int khold;
extern int kpause;
extern int kquit;

void incMod(int *var, int limit, bool sat);
void decMod(int *var, int limit, bool sat);
void resetGame(void);

void markDrop(void);
Uint32 getNextDropTime(void);
void setDropRate(int level);
void softDropTimeCounter(void);
void convertMsToStr(Uint32 ms, char *dest);

void getShapeDimensions(const struct Shape *shape, int *minx, int *maxx, int *miny, int *maxy);
struct Shape* getShape(enum FigureId id);
bool isFigureColliding(void);

#ifdef DEV
void setBlockAtScreenXY(int x, int y, enum BlockOrientation bo);
#endif

#endif
