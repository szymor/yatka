#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_mixer.h>

#include "joystick.h"
#include "main.h"
#include "data_persistence.h"
#include "video.h"
#include "sound.h"
#include "state_mainmenu.h"
#include "state_gameover.h"
#include "state_settings.h"
#include "randomizer.h"
#include "skin.h"

#define MAX_SOFTDROP_PRESS			300
#define FONT_SIZE					7

#define SIDE_MOVE_DELAY				60
#define FIXED_LOCK_DELAY			500
#define EASY_SPIN_DELAY				500
#define EASY_SPIN_MAX_COUNT			16

#define START_DROP_RATE				2.00

struct ShapeTemplate
{
	int blockmap[FIG_DIM*FIG_DIM];
	int cx, cy;
};


struct Skin gameskin;

SDL_Surface *bg = NULL;

struct Block *board = NULL;
struct Figure *figures[FIG_NUM];
int statistics[FIGID_GRAY];

bool nosound = false;
bool holdoff = false;
bool repeattrack = false;
bool easyspin = false;
bool lockdelay = false;
bool smoothanim = false;

enum TetrominoColor tetrominocolor = TC_STANDARD;

enum GameState gamestate = GS_MAINMENU;
static bool hold_ready = true;

int hiscore = 0;
int score = 0;
int lines = 0;
int level = 0;
int tetris_count = 0;
int ttr = 0;
static int lines_level_up = 0;
static int old_hiscore = 0;

int kleft = KEY_LEFT;
int kright = KEY_RIGHT;
int ksoftdrop = KEY_SOFTDROP;
int kharddrop = KEY_HARDDROP;
int krotatecw = KEY_ROTATE_CW;
int krotateccw = KEY_ROTATE_CCW;
int khold = KEY_HOLD;
int kpause = KEY_PAUSE;
int kquit = KEY_QUIT;

static double drop_rate = START_DROP_RATE;
static const double drop_rate_ratio_per_level = 1.20;
Uint32 last_drop_time;
static int easyspin_counter = 0;
static bool softdrop_pressed = false;
static Uint32 softdrop_press_time = 0;
static Uint32 next_lock_time = 0;

int brick_size;
int draw_delta_drop;

static bool left_move = false;
static bool right_move = false;
static Uint32 next_side_move_time;

static SDL_Joystick *joystick = NULL;

static struct Shape *shapes[FIGID_GRAY];
static const struct ShapeTemplate templates[] = {
	{	// I
		.blockmap	= { 0, 0, 0, 0,
						0, 0, 0, 0,
						1, 1, 1, 1,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 0
	},
	{	// O
		.blockmap	= { 0, 0, 0, 0,
						0, 1, 1, 0,
						0, 1, 1, 0,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 0
	},
	{	// T
		.blockmap	= { 0, 0, 0, 0,
						0, 1, 0, 0,
						1, 1, 1, 0,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 1
	},
	{	// S
		.blockmap	= { 0, 0, 0, 0,
						0, 1, 1, 0,
						1, 1, 0, 0,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 1
	},
	{	// Z
		.blockmap	= { 0, 0, 0, 0,
						1, 1, 0, 0,
						0, 1, 1, 0,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 1
	},
	{	// J
		.blockmap	= { 0, 0, 0, 0,
						1, 0, 0, 0,
						1, 1, 1, 0,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 1
	},
	{	// L
		.blockmap	= { 0, 0, 0, 0,
						0, 0, 1, 0,
						1, 1, 1, 0,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 1
	}
};

void initialize(void);
void finalize(void);
struct Shape *generateFromTemplate(const struct ShapeTemplate *template);

void moveLeft(int delay);
void moveRight(int delay);

void ingame_processInputEvents(void);

void dropSoft(void);
void dropHard(void);
void lockFigure(void);
void holdFigure(void);
void updateLockTime(void);
void updateEasySpin(void);
int removeFullLines(void);
bool checkGameEnd(void);
void onDrop(void);
void onLineClear(int removed);
void onGameOver(void);
void checkForPrelocking(void);

void initFigures(void);
void spawnFigure(void);
struct Figure *getNextFigure(void);
enum FigureId getNextColor(enum FigureId id);
enum FigureId getRandomColor(void);
void rotateFigureCW(void);
void rotateFigureCCW(void);

static void softdrop_off(void);
static void softdrop_on(void);
static void left_off(void);
static void left_on(void);
static void right_off(void);
static void right_on(void);
static void rotate_cw(void);
static void rotate_ccw(void);
static void pause(void);
static void quit(void);

int main(int argc, char *argv[])
{
	initPaths();
	loadSettings();

	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i],"--nosound"))
			nosound = true;
		else if (!strcmp(argv[i],"--sound"))
			nosound = false;
		else if (!strcmp(argv[i],"--fullscreen"))
			screenscale = 0;
		else if (!strcmp(argv[i],"--scale1x"))
			screenscale = 1;
		else if (!strcmp(argv[i],"--scale2x"))
			screenscale = 2;
		else if (!strcmp(argv[i],"--scale3x"))
			screenscale = 3;
		else if (!strcmp(argv[i],"--scale4x"))
			screenscale = 4;
		else if (!strcmp(argv[i],"--startlevel"))
		{
			++i;
			menu_level = atoi(argv[i]);
		}
		else
			printf("Unrecognized parameter: %s\n", argv[i]);
	}

	initialize();
	markDrop();
	while (1)
	{
		switch (gamestate)
		{
			case GS_MAINMENU:
				SDL_EnableKeyRepeat(500, 100);
				letMusicFinish();
				while (GS_MAINMENU == gamestate)
				{
					mainmenu_updateScreen();
					mainmenu_processInputEvents();
				}
				break;
			case GS_INGAME:
				SDL_EnableKeyRepeat(0, 0);
				playMusic();
				while (GS_INGAME == gamestate)
				{
					if (!frameLimiter())
					{
						ingame_processInputEvents();
						skin_updateScreen(&gameskin, screen);
						softDropTimeCounter();

						Uint32 ct = SDL_GetTicks();
						if (ct > getNextDropTime())
						{
							dropSoft();
						}
						if (next_lock_time && ct > next_lock_time)
						{
							++figures[0]->y;
							if (isFigureColliding())
							{
								--figures[0]->y;
								lockFigure();
							}
							else
							{
								next_lock_time = 0;
							}
						}
						if (ct > next_side_move_time)
						{
							if (left_move && !right_move)
							{
								moveLeft(SIDE_MOVE_DELAY);
							}
							else if (right_move && !left_move)
							{
								moveRight(SIDE_MOVE_DELAY);
							}
						}
					}
				}
				saveLastGameScreen();
				break;
			case GS_SETTINGS:
				SDL_EnableKeyRepeat(500, 100);
				while (GS_SETTINGS == gamestate)
				{
					settings_updateScreen();
					settings_processInputEvents();
				}
				break;
			case GS_GAMEOVER:
				gameover_updateScreen();
				while (GS_GAMEOVER == gamestate)
				{
					gameover_processInputEvents();
				}
				break;
		}
	}
	return 0;
}

void initFigures(void)
{
	for (int i = 0; i < FIG_NUM; ++i)
	{
		figures[i] = NULL;
	}
}

void initialize(void)
{
	SDL_Surface *ts = NULL;

	hiscore = loadHiscore();
	old_hiscore = hiscore;

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK) < 0)
	{
		printf("SDL_Init failed.\n");
		exit(ERROR_SDLINIT);
	}
	if (TTF_Init() < 0)
	{
		printf("TTF_Init failed.\n");
		exit(ERROR_TTFINIT);
	}
	atexit(finalize);
	int video_flags = VIDEO_MODE_FLAGS;
	int scale = screenscale;
	if (0 == screenscale)
	{
		scale = 1;
		video_flags |= SDL_FULLSCREEN;
	}
	screen_scaled = SDL_SetVideoMode(SCREEN_WIDTH * scale, SCREEN_HEIGHT * scale, SCREEN_BPP, video_flags);
	if (screen_scaled == NULL)
	{
		printf("SDL_SetVideoMode failed.\n");
		exit(ERROR_SDLVIDEO);
	}
	screen = scale > 1 ? SDL_CreateRGBSurface(SDL_SWSURFACE, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, 0, 0, 0, 0) : screen_scaled;
	SDL_WM_SetCaption("Y A T K A", NULL);
	SDL_ShowCursor(SDL_DISABLE);

	arcade_font = TTF_OpenFont("skins/default/arcade.ttf", FONT_SIZE);
	if (arcade_font == NULL)
	{
		printf("TTF_OpenFont failed.\n");
		exit(ERROR_NOFONT);
	}

	if (!nosound)
	{
		initSound();
	}

	// joystick support
	if (SDL_NumJoysticks() > 0)
	{
		SDL_JoystickEventState(SDL_ENABLE);
		joystick = SDL_JoystickOpen(0);
	}

	// shape init
	for (int i = 0; i < FIGID_GRAY; ++i)
	{
		shapes[i] = generateFromTemplate(&templates[i]);
	}

	board = malloc(sizeof(struct Block)*BOARD_WIDTH*BOARD_HEIGHT);

	srand((unsigned)time(NULL));

	initFigures();
	mainmenu_init();

	resetGame();

	skin_initSkin(&gameskin);
}

void finalize(void)
{
	if (hiscore > old_hiscore)
		saveHiscore(hiscore);
	if (settings_changed)
		saveSettings();

	skin_destroySkin(&gameskin);

	TTF_CloseFont(arcade_font);
	TTF_Quit();

	if (joystick != NULL)
		SDL_JoystickClose(joystick);

	if (!nosound)
		deinitSound();
	SDL_Quit();
	if(board != NULL)
		free(board);
	for (int i = 0; i < FIG_NUM; ++i)
		free(figures[i]);
	printf("Quit successfully.\n");
}

struct Shape *generateFromTemplate(const struct ShapeTemplate *template)
{
	struct Shape *sh = malloc(sizeof(struct Shape));
	if (!sh)
		return NULL;

	sh->cx = template->cx;
	sh->cy = template->cy;
	for (int i = 0; i < FIG_DIM * FIG_DIM; ++i)
	{
		if (template->blockmap[i])
		{
			int x = i % FIG_DIM;
			int y = i / FIG_DIM;
			int p;
			sh->blockmap[i] = BO_EMPTY;

			--y;
			p = x + y * FIG_DIM;
			if (p >= 0 && (p < FIG_DIM * FIG_DIM) && template->blockmap[p])
				sh->blockmap[i] |= BO_UP;
			++y;

			++y;
			p = x + y * FIG_DIM;
			if (p >= 0 && (p < FIG_DIM * FIG_DIM) && template->blockmap[p])
				sh->blockmap[i] |= BO_DOWN;
			--y;

			--x;
			p = x + y * FIG_DIM;
			if (p >= 0 && (p < FIG_DIM * FIG_DIM) && template->blockmap[p])
				sh->blockmap[i] |= BO_LEFT;
			++x;

			++x;
			p = x + y * FIG_DIM;
			if (p >= 0 && (p < FIG_DIM * FIG_DIM) && template->blockmap[p])
				sh->blockmap[i] |= BO_RIGHT;
			--x;

			if (sh->blockmap[i] == BO_EMPTY)
				sh->blockmap[i] = BO_FULL;
		}
		else
			sh->blockmap[i] = BO_EMPTY;
	}

	return sh;
}

void markDrop(void)
{
	last_drop_time = SDL_GetTicks();
}

Uint32 getNextDropTime(void)
{
	const double maxDropRate = 60.0;
	double coef = (double)(MAX_SOFTDROP_PRESS - softdrop_press_time) / MAX_SOFTDROP_PRESS;
	coef = 1 - coef;
	return last_drop_time + (Uint32)(1000 / (drop_rate * (1 - coef) + maxDropRate * coef));
}

void setDropRate(int level)
{
	drop_rate = START_DROP_RATE;
	if (level < 0)
		return;
	while (level--)
		drop_rate *= drop_rate_ratio_per_level;
}

void softDropTimeCounter(void)
{
	static Uint32 curTicks;
	static Uint32 lastTicks;
	Uint32 delta;

	lastTicks = curTicks;
	curTicks = SDL_GetTicks();
	delta = curTicks - lastTicks;

	if (softdrop_pressed)
	{
		softdrop_press_time += delta;
		if (softdrop_press_time > MAX_SOFTDROP_PRESS)
			softdrop_press_time = MAX_SOFTDROP_PRESS;
	}
}

void moveLeft(int delay)
{
	Uint32 ct = SDL_GetTicks();
	if (figures[0] != NULL)
	{
		--figures[0]->x;
		if (isFigureColliding())
			++figures[0]->x;
		else
		{
			Mix_PlayChannel(-1, click, 0);
			updateEasySpin();
			updateLockTime();
			if (lockdelay)
			{
				checkForPrelocking();
			}
		}

		next_side_move_time = ct + delay;
	}
}

void moveRight(int delay)
{
	Uint32 ct = SDL_GetTicks();
	if (figures[0] != NULL)
	{
		++figures[0]->x;
		if (isFigureColliding())
			--figures[0]->x;
		else
		{
			Mix_PlayChannel(-1, click, 0);
			updateEasySpin();
			updateLockTime();
			if (lockdelay)
			{
				checkForPrelocking();
			}
		}

		next_side_move_time = ct + delay;
	}
}

void onDrop(void)
{
	markDrop();
	if (!next_lock_time && smoothanim)
		draw_delta_drop = -brick_size;
}

void onLineClear(int removed)
{
	lines += removed;
	lines_level_up += removed;

	switch (removed)
	{
		case 1:
			score += 100;
			break;
		case 2:
			score += 300;
			break;
		case 3:
			score += 500;
			break;
		case 4:
			score += 800;
			++tetris_count;
			break;
	}
	if (score > hiscore)
		hiscore = score;

	int levelup = lines_level_up / 30;
	level += levelup;
	if (levelup)
	{
		drop_rate *= drop_rate_ratio_per_level;
	}
	lines_level_up %= 30;

	if (lines != 0)
		ttr = 4 * tetris_count * 100 / lines;
	else
		ttr = 0;

	Mix_PlayChannel(-1, clr, 0);
}

void onGameOver(void)
{
	stopMusic();
	gamestate = GS_GAMEOVER;
}

void dropSoft(void)
{
	if (figures[0] == NULL)
	{
		spawnFigure();
	}
	else
	{
		++figures[0]->y;
		if (isFigureColliding())
		{
			--figures[0]->y;
			if (!lockdelay)
			{
				next_lock_time = getNextDropTime();
			}
		}
		else
		{
			if (lockdelay)
			{
				checkForPrelocking();
			}
		}
		onDrop();
	}
}

void checkForPrelocking(void)
{
	if (figures[0] == NULL)
		return;
	// check for prelocking state
	// next_lock_time is both a time marker and a state flag
	++figures[0]->y;
	if (isFigureColliding())
	{
		if (!next_lock_time)
			next_lock_time = SDL_GetTicks() + FIXED_LOCK_DELAY;
	}
	else
	{
		next_lock_time = 0;
	}
	--figures[0]->y;
}

void dropHard(void)
{
	if (figures[0] != NULL)
	{
		while (!isFigureColliding())
			++figures[0]->y;
		--figures[0]->y;
		lockFigure();
		onDrop();
	}
}

void lockFigure(void)
{
	if (figures[0] == NULL)
		return;

	for (int i = 0; i < (FIG_DIM*FIG_DIM); ++i)
		if (figures[0]->shape.blockmap[i] != BO_EMPTY)
		{
			int x = i % FIG_DIM + figures[0]->x;
			int y = i / FIG_DIM + figures[0]->y;
			if ((x >= 0) && (x < BOARD_WIDTH) && (y >= 0) && (y < BOARD_HEIGHT))
			{
				board[y*BOARD_WIDTH + x].color = figures[0]->color;
				board[y*BOARD_WIDTH + x].orientation = figures[0]->shape.blockmap[i];
			}
		}

	free(figures[0]);
	figures[0] = NULL;

	int removed = removeFullLines();
	if (checkGameEnd())
		onGameOver();

	softdrop_pressed = false;
	softdrop_press_time = 0;
	if (!removed)
		Mix_PlayChannel(-1, hit, 0);

	next_lock_time = 0;
	easyspin_counter = 0;
}

void holdFigure(void)
{
	if (hold_ready && !holdoff && figures[0])
	{
		--statistics[figures[0]->id];
		struct Figure temp = *figures[0];
		*figures[0] = *figures[1];
		*figures[1] = temp;
		++statistics[figures[0]->id];

		figures[0]->y = -FIG_DIM + 2;						// ...to gain a 'slide' effect from the top of screen
		figures[0]->x = (BOARD_WIDTH - FIG_DIM) / 2;		// ...to center a figure

		memcpy(&figures[0]->shape, getShape(figures[0]->id), sizeof(figures[0]->shape));

		hold_ready = false;
	}
}

int removeFullLines(void)
{
	int removed_lines = 0;

	// checking and removing full lines
	for (int y = 1; y < BOARD_HEIGHT; ++y)
	{
		bool flag = true;
		for (int x = 0; x < BOARD_WIDTH; ++x)
		{
			if (board[y*BOARD_WIDTH + x].orientation == BO_EMPTY)
			{
				flag = false;
				break;
			}
		}
		if (flag)
		{
			// removing
			++removed_lines;
			for (int ys = y-1; ys >= 0; --ys)
				for (int x = 0; x < BOARD_WIDTH; ++x)
					board[(ys + 1) * BOARD_WIDTH + x] = board[ys * BOARD_WIDTH + x];

			// adjusting orientation
			for (int x = 0; x < BOARD_WIDTH; ++x)
			{
				enum BlockOrientation *bo = NULL;
				bo = &board[y * BOARD_WIDTH + x].orientation;
				if (*bo != BO_EMPTY && *bo != BO_FULL)
				{
					*bo &= ~BO_DOWN;
					if (*bo == BO_EMPTY)
						*bo = BO_FULL;
				}
				if (y + 1 < BOARD_HEIGHT)
				{
					bo = &board[(y + 1) * BOARD_WIDTH + x].orientation;
					if (*bo != BO_EMPTY && *bo != BO_FULL)
					{
						*bo &= ~BO_UP;
						if (*bo == BO_EMPTY)
							*bo = BO_FULL;
					}
				}
			}
		}
	}

	if (removed_lines > 0)
		onLineClear(removed_lines);

	return removed_lines;
}

static void softdrop_off(void)
{
	softdrop_pressed = false;
	softdrop_press_time = 0;
}

static void softdrop_on(void)
{
	softdrop_pressed = true;
}

static void left_off(void)
{
	left_move = false;
}

static void left_on(void)
{
	left_move = true;
	moveLeft(SIDE_MOVE_DELAY * 3);
}

static void right_off(void)
{
	right_move = false;
}

static void right_on(void)
{
	right_move = true;
	moveRight(SIDE_MOVE_DELAY * 3);
}

static void rotate_cw(void)
{
	bool success = true;
	rotateFigureCW();
	if (isFigureColliding())
	{
		++figures[0]->x;
		if (isFigureColliding())
		{
			figures[0]->x -= 2;
			if (isFigureColliding())
			{
				rotateFigureCCW();
				++figures[0]->x;
				success = false;
			}
		}
	}
	if (success && figures[0] && figures[0]->id != FIGID_O)
	{
		updateEasySpin();
		updateLockTime();
		if (lockdelay)
		{
			checkForPrelocking();
		}
	}
}

static void rotate_ccw(void)
{
	bool success = true;
	rotateFigureCCW();
	if (isFigureColliding())
	{
		++figures[0]->x;
		if (isFigureColliding())
		{
			figures[0]->x -= 2;
			if (isFigureColliding())
			{
				rotateFigureCW();
				++figures[0]->x;
				success = false;
			}
		}
	}
	if (success && figures[0] && figures[0]->id != FIGID_O)
	{
		updateEasySpin();
		updateLockTime();
		if (lockdelay)
		{
			checkForPrelocking();
		}
	}
}

static void pause(void)
{
	gamestate = GS_SETTINGS;
}

static void quit(void)
{
	gamestate = GS_MAINMENU;
	if (hiscore > old_hiscore)
	{
		saveHiscore(hiscore);
		old_hiscore = hiscore;
	}
}

void ingame_processInputEvents(void)
{
	SDL_Event event;
	bool joy_harddrop = false;

	if (SDL_PollEvent(&event))
		switch (event.type)
		{
			case SDL_JOYAXISMOTION:
				if ((event.jaxis.value < -JOY_THRESHOLD) || (event.jaxis.value > JOY_THRESHOLD))
				{
					if(event.jaxis.axis == 0)
					{
						if (event.jaxis.value < 0)
						{
							if (!left_move)
								left_on();
						}
						else
						{
							if (!right_move)
								right_on();
						}
					}

					if(event.jaxis.axis == 1)
					{
						if (event.jaxis.value < 0)
						{
							if (!joy_harddrop)
							{
								joy_harddrop = true;
								dropHard();
							}
						}
						else
						{
							if (!softdrop_pressed)
								softdrop_on();
						}
					}
				}
				else
				{
					left_off();
					right_off();
					softdrop_off();
					joy_harddrop = false;
				}
				break;
			case SDL_JOYBUTTONDOWN:
				if (event.jbutton.button == JOY_ROTATE_CW)
				{
					rotate_cw();
				}
				else if (event.jbutton.button == JOY_ROTATE_CCW)
				{
					rotate_ccw();
				}
				else if (event.jbutton.button == JOY_HOLD)
				{
					holdFigure();
				}
				else if (event.jbutton.button == JOY_PAUSE)
				{
					pause();
				}
				else if (event.jbutton.button == JOY_QUIT)
				{
					quit();
				}
				break;
			case SDL_KEYUP:
				if (event.key.keysym.sym == ksoftdrop)
				{
					softdrop_off();
				}
				else if (event.key.keysym.sym == kleft)
				{
					left_off();
				}
				else if (event.key.keysym.sym == kright)
				{
					right_off();
				}
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == krotatecw)
				{
					rotate_cw();
				}
				else if (event.key.keysym.sym == krotateccw)
				{
					rotate_ccw();
				}
				else if (event.key.keysym.sym == ksoftdrop)
				{
					softdrop_on();
				}
				else if (event.key.keysym.sym == kharddrop)
				{
					dropHard();
				}
				else if (event.key.keysym.sym == khold)
				{
					holdFigure();
				}
				else if (event.key.keysym.sym == kleft)
				{
					left_on();
				}
				else if (event.key.keysym.sym == kright)
				{
					right_on();
				}
				else if (event.key.keysym.sym == kpause)
				{
					pause();
				}
				else if (event.key.keysym.sym == kquit)
				{
					quit();
				}
				break;
			case SDL_QUIT:
				exit(0);
				break;
		}
}

bool checkGameEnd(void)
{
	for (int i = 0; i < BOARD_WIDTH; ++i)
		if (board[i].orientation != BO_EMPTY)
			return true;
	return false;
}

void spawnFigure(void)
{
	if (figures[0] != NULL)
		free(figures[0]);
	for (int i = 0; i < FIG_NUM - 1; ++i)
		figures[i] = figures[i+1];
	figures[FIG_NUM - 1] = getNextFigure();

	if (figures[0] != NULL)
	{
		figures[0]->y = -FIG_DIM + 2;						// ...to gain a 'slide' effect from the top of screen
		figures[0]->x = (BOARD_WIDTH - FIG_DIM) / 2;		// ...to center a figure
		memcpy(&figures[0]->shape, getShape(figures[0]->id), sizeof(figures[0]->shape));
		++statistics[figures[0]->id];
	}

	hold_ready = true;
}

struct Figure *getNextFigure(void)
{
	struct Figure *f = malloc(sizeof(struct Figure));
	f->id = getNextId();
	f->color = getNextColor(f->id);
	return f;
}

struct Shape* getShape(enum FigureId id)
{
	return shapes[id];
}

enum FigureId getNextColor(enum FigureId id)
{
	switch (tetrominocolor)
	{
		case TC_RANDOM:
			return getRandomColor();
		case TC_STANDARD:
			return id;
		case TC_GRAY:
		default:
			return FIGID_GRAY;
	}
}

enum FigureId getRandomColor(void)
{
	return rand() % FIGID_GRAY;
}

void getShapeDimensions(const struct Shape *shape, int *minx, int *maxx, int *miny, int *maxy)
{
	int min_x = FIG_DIM - 1, max_x = 0;
	int min_y = FIG_DIM - 1, max_y = 0;

	for (int i = 0; i < FIG_DIM*FIG_DIM; ++i)
	{
		int x = i % FIG_DIM;
		int y = i / FIG_DIM;
		if (shape->blockmap[i] != BO_EMPTY)
		{
			if (x < min_x)
				min_x = x;
			if (x > max_x)
				max_x = x;
			if (y < min_y)
				min_y = y;
			if (y > max_y)
				max_y = y;
		}
	}

	*minx = min_x;
	*maxx = max_x;
	*miny = min_y;
	*maxy = max_y;
}

bool isFigureColliding(void)
{
	int i, empty_columns_right, empty_columns_left, empty_rows_down;
	if (figures[0] != NULL)
	{
		// counting empty columns on the righthand side of figure
		// 'x' and 'y' used as counters
		empty_columns_right = 0;
		for (int x = FIG_DIM-1; x >= 0; --x)
		{
			i = 1;		// empty by default
			for (int y = 0; y < FIG_DIM; ++y)
				if (figures[0]->shape.blockmap[y*FIG_DIM + x] != BO_EMPTY)
				{
					i = 0;
					break;
				}
			if (i)
				++empty_columns_right;
			else
				break;
		}

		// the same as above but for the lefthand side
		empty_columns_left = 0;
		for (int x = 0; x < FIG_DIM; ++x)
		{
			i = 1;		// empty by default
			for (int y = 0; y < FIG_DIM; ++y )
				if (figures[0]->shape.blockmap[y*FIG_DIM + x] != BO_EMPTY)
				{
					i = 0;
					break;
				}
			if (i)
				++empty_columns_left;
			else
				break;
		}

		// ...and for the bottom side
		empty_rows_down = 0;
		for (int y = FIG_DIM-1; y >= 0; --y)
		{
			i = 1;		// empty by default
			for (int x = 0; x < FIG_DIM; ++x)
				if (figures[0]->shape.blockmap[y*FIG_DIM + x] != BO_EMPTY)
				{
					i = 0;
					break;
				}
			if (i)
				++empty_rows_down;
			else
				break;
		}

		// proper collision checking
		if ((figures[0]->x < -empty_columns_left) || (figures[0]->x > BOARD_WIDTH-FIG_DIM+empty_columns_right))
			return true;
		if (figures[0]->y > (BOARD_HEIGHT-FIG_DIM+empty_rows_down))
			return true;
		for (int i = 0; i < (FIG_DIM*FIG_DIM); ++i)
			if (figures[0]->shape.blockmap[i] != BO_EMPTY)
			{
				int x = i % FIG_DIM + figures[0]->x;
				int y = i / FIG_DIM + figures[0]->y;
				if (((y*BOARD_WIDTH + x) < (BOARD_WIDTH*BOARD_HEIGHT)) && ((y*BOARD_WIDTH + x) >= 0))
					if (board[y*BOARD_WIDTH + x].orientation != BO_EMPTY)
						return true;
			}
	}
	return false;
}

void rotateFigureCW(void)
{
	int temp[FIG_DIM*FIG_DIM];

	if (figures[0] != NULL)
	{
		memcpy(temp, figures[0]->shape.blockmap, sizeof(temp));
		for(int i = 0; i < FIG_DIM*FIG_DIM; ++i)
		{
			int x = i % FIG_DIM;
			int y = i / FIG_DIM;
			figures[0]->shape.blockmap[i] = temp[(FIG_DIM-1-x)*FIG_DIM + y];
			// block rotation
			figures[0]->shape.blockmap[i] = ( ((figures[0]->shape.blockmap[i]) >> 1) | (((figures[0]->shape.blockmap[i]) & 1) << 3) );
		}

		figures[0]->x += figures[0]->shape.cx;
		figures[0]->y += figures[0]->shape.cy;

		int tcx = figures[0]->shape.cx;
		figures[0]->shape.cx = -figures[0]->shape.cy;
		figures[0]->shape.cy = tcx;
	}
}

void rotateFigureCCW(void)
{
	rotateFigureCW();
	rotateFigureCW();
	rotateFigureCW();
}

void incMod(int *var, int limit, bool sat)
{
	if (sat && *var == limit - 1)
		return;
	*var = (*var + 1) % limit;
}

void decMod(int *var, int limit, bool sat)
{
	if (sat && *var == 0)
		return;
	*var = (*var + limit - 1) % limit;
}

void resetGame(void)
{
	for (int i = 0; i < (BOARD_WIDTH*BOARD_HEIGHT); ++i)
	{
		board[i].color = FIGID_END;
		board[i].orientation = BO_EMPTY;
	}

	lines = 0;
	lines_level_up = 0;
	level = menu_level;
	setDropRate(level);
	score = 0;
	tetris_count = 0;
	ttr = 0;
	left_move = false;
	right_move = false;

	randomizer_reset();

	for (int i = 0; i < FIG_NUM - 1; ++i)
	{
		spawnFigure();
	}

	for (int i = 0; i < FIGID_GRAY; ++i)
	{
		statistics[i] = 0;
	}

	spawnFigure();

	// debris
	int filled = 0;
	for (int i = 0; i < (BOARD_WIDTH * BOARD_HEIGHT); ++i)
	{
		int y = i / BOARD_WIDTH;
		int x = i % BOARD_WIDTH;

		y = BOARD_HEIGHT - INVISIBLE_ROW_COUNT - y;
		if ((y < menu_debris) && ((rand() % 128) < (menu_debris_chance * 128 / 10)))
		{
			board[i].color = FIGID_GRAY;
			board[i].orientation = BO_FULL;
			++filled;
		}

		if ((BOARD_WIDTH - 1) == x)
		{
			if (BOARD_WIDTH == filled)
			{
				int ind = rand() % BOARD_WIDTH;
				ind = i - ind;
				board[ind].color = FIGID_END;
				board[ind].orientation = BO_EMPTY;
			}
			filled = 0;
		}
	}
}

void updateLockTime(void)
{
	if (lockdelay && easyspin && next_lock_time && (easyspin_counter < EASY_SPIN_MAX_COUNT))
	{
		next_lock_time = SDL_GetTicks() + FIXED_LOCK_DELAY;
	}
}

void updateEasySpin(void)
{
	if (next_lock_time)
	{
		++easyspin_counter;
	}
}
