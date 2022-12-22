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

#define TEST_NUM					(5)
#define PHASE_NUM					(4)
#define TST_PTS_NUM					(4)

struct ShapeTemplate
{
	int blockmap[FIG_DIM*FIG_DIM];
	int cx, cy;
	int phase;
};

enum TSpinType
{
	TST_NONE,
	TST_REGULAR,
	TST_MINI,
	TST_END
};

enum GameOverType
{
	GOT_PLAYING,
	GOT_FULLWELL,
	GOT_LINECLEAR,
	GOT_TIMEUP,
	GOT_END
};

struct Skin gameskin;

SDL_Surface *bg = NULL;

struct Block *board = NULL;
struct Figure *figures[FIG_NUM];
struct Figure preserved = { .id = FIGID_END };
int statistics[FIGID_GRAY];

/* all settings need to be false in order to
 * be properly read from settings file */
bool nosound = false;
bool repeattrack = false;
bool easyspin = false;
bool lockdelay = false;
bool smoothanim = false;
bool speechon = false;

enum TetrominoColor tetrominocolor = TC_STANDARD;

enum GameState gamestate = GS_MAINMENU;
static bool hold_ready = true;

int score = 0;
int lines = 0;
int level = 0;
int tetris_count = 0;
int ttr = 0;
int b2b = 0;	// back2back bonus
int combo = 0;	// combo bonus
static int lines_level_up = 0;

char lctext_top[LCT_LEN];
char lctext_mid[LCT_LEN];
char lctext_bot[LCT_LEN];
Uint32 lct_deadline = 0;

Uint32 game_starttime = 0;
Uint32 game_totaltime = 0;
char gametimer[GAMETIMER_STRLEN];

// test variables for T-Spin detection
bool tst_tetromino_t = false;
bool tst_rotation_last = false;
bool tst_2front_12back = false;
bool tst_1front_2back = false;
bool tst_rotation_12 = false;

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
Uint32 next_lock_time = 0;

int brick_size;
int draw_delta_drop;

static bool left_move = false;
static bool right_move = false;
static Uint32 next_side_move_time;

#ifdef JOY_SUPPORT
static SDL_Joystick *joystick = NULL;
#endif

static struct Shape *shapes[FIGID_GRAY];
static const struct ShapeTemplate templates[] = {
	{	// I
		.blockmap	= { 0, 0, 0, 0,
						0, 0, 0, 0,
						1, 1, 1, 1,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 0,
		.phase = 2
	},
	{	// O
		.blockmap	= { 0, 0, 0, 0,
						0, 1, 1, 0,
						0, 1, 1, 0,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 0,
		.phase = 0
	},
	{	// T
		.blockmap	= { 0, 0, 0, 0,
						0, 1, 0, 0,
						1, 1, 1, 0,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 1,
		.phase = 0
	},
	{	// S
		.blockmap	= { 0, 0, 0, 0,
						0, 1, 1, 0,
						1, 1, 0, 0,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 1,
		.phase = 0
	},
	{	// Z
		.blockmap	= { 0, 0, 0, 0,
						1, 1, 0, 0,
						0, 1, 1, 0,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 1,
		.phase = 0
	},
	{	// J
		.blockmap	= { 0, 0, 0, 0,
						1, 0, 0, 0,
						1, 1, 1, 0,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 1,
		.phase = 0
	},
	{	// L
		.blockmap	= { 0, 0, 0, 0,
						0, 0, 1, 0,
						1, 1, 1, 0,
						0, 0, 0, 0 },
		.cx = 0,
		.cy = 1,
		.phase = 0
	}
};

// source: https://tetris.fandom.com/wiki/SRS
// phase, testid, x, y
static const int wallkick_data[PHASE_NUM][TEST_NUM][2] = {
	{ { 0, 0}, {-1, 0}, {-1,-1}, { 0, 2}, {-1, 2} },
	{ { 0, 0}, { 1, 0}, { 1, 1}, { 0,-2}, { 1,-2} },
	{ { 0, 0}, { 1, 0}, { 1,-1}, { 0, 2}, { 1, 2} },
	{ { 0, 0}, {-1, 0}, {-1, 1}, { 0,-2}, {-1,-2} }
};
static const int wallkick_data_figI[PHASE_NUM][TEST_NUM][2] = {
	{ { 0, 0}, {-2, 0}, { 1, 0}, {-2, 1}, { 1,-2} },
	{ { 0, 0}, {-1, 0}, { 2, 0}, {-1,-2}, { 2, 1} },
	{ { 0, 0}, { 2, 0}, {-1, 0}, { 2,-1}, {-1, 2} },
	{ { 0, 0}, { 1, 0}, {-2, 0}, { 1, 2}, {-2,-1} }
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
enum TSpinType getTSpinType(void);
enum GameOverType checkGameEnd(void);
void onDrop(void);
void onLineClear(int removed);
void onGameOver(enum GameOverType reason);
void checkForPrelocking(void);
void updateLCT(char *top, char *mid, char *bot, Uint32 ms);
void updateGTimer(Uint32 ms);
void updateHiscores(enum GameMode gm, enum GameOverType got);

void initFigures(void);
void spawnFigure(void);
struct Figure *getNextFigure(void);
enum FigureId getNextColor(enum FigureId id);
enum FigureId getRandomColor(void);
void rotateFigureCW(void);
void rotateFigureCCW(void);
bool rotateTest(int x, int y);

static void generateDebris(void);
static void checkForSmoothAnimationCollisions(void);

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
						if (ct > getNextDropTime() && !next_lock_time)
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
						if (ct > lct_deadline)
						{
							updateLCT("", "", "", 0);
						}
						updateGTimer(ct - game_starttime);
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

	loadRecords();

	Uint32 sdl_init_flags = SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO;
#ifdef JOY_SUPPORT
	sdl_init_flags |= SDL_INIT_JOYSTICK;
#endif

	if (SDL_Init(sdl_init_flags) < 0)
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

	Uint32 video_flags = VIDEO_MODE_FLAGS;
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
#ifdef DEV
	SDL_ShowCursor(SDL_ENABLE);
#else
	SDL_ShowCursor(SDL_DISABLE);
#endif

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

#ifdef JOY_SUPPORT
	if (SDL_NumJoysticks() > 0)
	{
		SDL_JoystickEventState(SDL_ENABLE);
		joystick = SDL_JoystickOpen(0);
	}
#endif

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
	if (settings_changed)
		saveSettings();
	saveRecords();

	skin_destroySkin(&gameskin);

	TTF_CloseFont(arcade_font);
	TTF_Quit();

#ifdef JOY_SUPPORT
	if (joystick != NULL)
		SDL_JoystickClose(joystick);
#endif

	if (!nosound)
		deinitSound();
	SDL_Quit();
	if(board != NULL)
		free(board);
	for (int i = 0; i < FIG_NUM; ++i)
		free(figures[i]);
	log("Quit successfully.\n");
}

struct Shape *generateFromTemplate(const struct ShapeTemplate *template)
{
	struct Shape *sh = malloc(sizeof(struct Shape));
	if (!sh)
		return NULL;

	sh->cx = template->cx;
	sh->cy = template->cy;
	sh->phase = template->phase;
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
			tst_rotation_last = false;
			playEffect(SE_CLICK);
			updateEasySpin();
			updateLockTime();
			if (lockdelay)
			{
				checkForPrelocking();
			}
			checkForSmoothAnimationCollisions();
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
			tst_rotation_last = false;
			playEffect(SE_CLICK);
			updateEasySpin();
			updateLockTime();
			if (lockdelay)
			{
				checkForPrelocking();
			}
			checkForSmoothAnimationCollisions();
		}

		next_side_move_time = ct + delay;
	}
}

void onDrop(void)
{
	markDrop();
	if (smoothanim)
		draw_delta_drop = -brick_size;
}

void onLineClear(int removed)
{
	enum TSpinType tst = getTSpinType();
	lines += removed;
	lines_level_up += removed;

	// remove b2b flag on easy clear
	if (removed != 4 && TST_NONE == tst)
	{
		b2b = 0;
	}

	// calculate score
	int extra = 0;
	switch (removed)
	{
		case 1:
			switch (tst)
			{
				case TST_REGULAR:
					extra = 800 * (level + 1);
					break;
				case TST_MINI:
					extra = 200 * (level + 1);
					break;
				case TST_NONE:
				default:
					extra = 100 * (level + 1);
			}
			break;
		case 2:
			switch (tst)
			{
				case TST_REGULAR:
					extra = 1200 * (level + 1);
					break;
				case TST_MINI:
					extra = 400 * (level + 1);
					break;
				case TST_NONE:
				default:
					extra = 300 * (level + 1);
			}
			break;
		case 3:
			switch (tst)
			{
				case TST_REGULAR:
					extra = 1600 * (level + 1);
					break;
				case TST_MINI:
					// not possible
					break;
				case TST_NONE:
				default:
					extra = 500 * (level + 1);
			}
			break;
		case 4:
			extra = 800 * (level + 1);
			break;
	}
	if (b2b)
	{
		extra += extra / 2;
	}
	if (combo)
	{
		extra += 50 * (level + 1) * combo;
	}
	score += extra;

	// notify a player about a special event
	static char clear_text[5][16] = {
		"Single", "Double", "Triple", "Tetris", "Cheatris"
	};
	static char tspin_text[TST_END][16] = {
		"", "T-Spin ", "Mini T-Spin "
	};
	int rmvd = removed - 1;
	if (rmvd > 4)
		rmvd = 4;
	char lctmid[LCT_LEN];
	char lctbot[LCT_LEN];
	sprintf(lctmid, "%s%s", tspin_text[tst], clear_text[rmvd]);
	if (combo)
	{
		sprintf(lctbot, "combo %dx", combo);
	}
	else
	{
		lctbot[0] = '\0';
	}
	updateLCT(b2b ? "Back-2-Back" : "", lctmid, lctbot, LCT_DEADLINE);

	if (speechon)
	{
		enum SfxSpeech ssflags = b2b ? SS_B2B : 0;
		switch (tst)
		{
			case TST_REGULAR:
				ssflags |= SS_TSPIN;
				break;
			case TST_MINI:
				ssflags |= SS_MINITSPIN;
		}
		switch (removed)
		{
			case 1:
				ssflags |= SS_SINGLE;
				break;
			case 2:
				ssflags |= SS_DOUBLE;
				break;
			case 3:
				ssflags |= SS_TRIPLE;
				break;
			case 4:
				ssflags |= SS_TETRIS;
				break;
		}
		playSpeech(ssflags);
	}

	// update flags for future usage
	++combo;
	if (4 == removed)
	{
			++b2b;
			++tetris_count;
	}
	if (TST_NONE != tst)
	{
		++b2b;
	}

	// adjust level if needed (Marathon only)
	if (GM_MARATHON == menu_gamemode)
	{
		int levelup = lines_level_up / 30;
		level += levelup;
		if (levelup)
		{
			drop_rate *= drop_rate_ratio_per_level;
		}
		lines_level_up %= 30;
	}

	// calculate tetris percentage
	if (lines != 0)
		ttr = 4 * tetris_count * 100 / lines;
	else
		ttr = 0;

	// play a clearing sound
	playEffect(SE_CLEAR);
}

void onGameOver(enum GameOverType reason)
{
	stopMusic();
	game_totaltime = SDL_GetTicks() - game_starttime;
	updateHiscores(menu_gamemode, reason);
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
		if (softdrop_pressed)
			++score;
		if (isFigureColliding())
		{
			--figures[0]->y;
			if (softdrop_pressed)
				--score;
			if (!lockdelay)
			{
				next_lock_time = getNextDropTime();
			}
		}
		else
		{
			tst_rotation_last = false;
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

static void checkForSmoothAnimationCollisions(void)
{
	if (figures[0] != NULL)
	{
		bool collide = false;
		int fx = figures[0]->x;
		int fy = figures[0]->y - 1;
		for (int y = 0; y < FIG_DIM; ++y)
			for (int x = 0; x < FIG_DIM; ++x)
			{
				if ((fy + y) < 0)
					continue;
				if (figures[0]->shape.blockmap[y * FIG_DIM + x] != BO_EMPTY &&
					board[BOARD_WIDTH * (fy + y) + (fx + x)].orientation != BO_EMPTY)
				{
					collide = true;
					break;
				}
			}
		if (collide)
		{
			draw_delta_drop = 0;
		}
	}
}

void dropHard(void)
{
	if (figures[0] != NULL)
	{
		tst_rotation_last = false;
		while (!isFigureColliding())
		{
			++figures[0]->y;
			score += 2;
		}
		--figures[0]->y;
		score -= 2;
		lockFigure();
		onDrop();
	}
}

void lockFigure(void)
{
	if (figures[0] == NULL)
		return;
	tst_tetromino_t = figures[0]->id == FIGID_T;
	if (tst_tetromino_t)
	{
		// calculation of front and back bricks
		// first two are front bricks, last two are back ones
		// it could be refactored to 4x 4x 2x const array instead
		int tst_coords_x[TST_PTS_NUM] = { 0, 2, 0, 2 };
		int tst_coords_y[TST_PTS_NUM] = { 1, 1, 3, 3 };
		int tst_phase = 0;
		while (tst_phase != figures[0]->shape.phase)
		{
			for (int i = 0; i < TST_PTS_NUM; ++i)
			{
				int old_x = tst_coords_x[i];
				tst_coords_x[i] = FIG_DIM - 1 - tst_coords_y[i];
				tst_coords_y[i] = old_x;
			}
			tst_phase = (tst_phase + 1) % PHASE_NUM;
		}

		// calculation of tst_*front_*back conditions for T-Spin detection
		int front_num = 0, back_num = 0;
		// translation to the board coordinate system and counting
		for (int i = 0; i < TST_PTS_NUM; ++i)
		{
			tst_coords_x[i] += figures[0]->x;
			tst_coords_y[i] += figures[0]->y;
			int x = tst_coords_x[i];
			int y = tst_coords_y[i];
			bool x_out_of_board = (x < 0) || (x >= BOARD_WIDTH);
			bool y_out_of_board = (y < 0) || (y >= BOARD_HEIGHT);
			bool xy_not_empty = false;
			if (!x_out_of_board && !y_out_of_board)
			{
				xy_not_empty = BO_EMPTY != board[y*BOARD_WIDTH + x].orientation;
			}
			if (x_out_of_board || y_out_of_board || xy_not_empty)
			{
				if (i < 2)
				{
					++front_num;
				}
				else
				{
					++back_num;
				}
			}
		}
		tst_2front_12back = (2 == front_num) && (back_num >= 1);
		tst_1front_2back = (1 == front_num) && (2 == back_num);
	}

	// storing the figure in the board
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
	enum GameOverType goreason = checkGameEnd();
	if (goreason != GOT_PLAYING)
		onGameOver(goreason);

	softdrop_pressed = false;
	softdrop_press_time = 0;
	if (!removed)
	{
		combo = 0;
		playEffect(SE_HIT);
	}

	next_lock_time = 0;
	easyspin_counter = 0;
}

void holdFigure(void)
{
	switch (gameskin.holdmode)
	{
		case HM_OFF:
			break;	// do nothing
		case HM_EXCHANGE:
		{
			if (hold_ready && figures[0])
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
		} break;
		case HM_PRESERVE:
		{
			if (hold_ready && figures[0])
			{
				if (preserved.id != FIGID_END)
				{
					struct Figure temp = *figures[0];
					// restore the preserved figure
					--statistics[figures[0]->id];
					*figures[0] = preserved;
					figures[0]->y = -FIG_DIM + 2;
					figures[0]->x = (BOARD_WIDTH - FIG_DIM) / 2;	// center a figure
					memcpy(&figures[0]->shape, getShape(figures[0]->id), sizeof(figures[0]->shape));
					++statistics[figures[0]->id];
					// preserve the current figure
					preserved = temp;
				}
				else
				{
					// preserve the current figure
					--statistics[figures[0]->id];
					preserved = *figures[0];
					// take the next figure
					spawnFigure();
				}
				hold_ready = false;
			}
		} break;
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
	{
		onLineClear(removed_lines);
	}
	else
	{
		// check for T-Spins clearing no lines
		switch (getTSpinType())
		{
			case TST_REGULAR:
				score += 400 * (level + 1);
				updateLCT("", "T-Spin", "", LCT_DEADLINE);
				break;
			case TST_MINI:
				score += 100 * (level + 1);
				updateLCT("", "Mini T-Spin", "", LCT_DEADLINE);
				break;
			case TST_NONE:
			default:
				; // no action
		}
	}

	return removed_lines;
}

enum TSpinType getTSpinType(void)
{
	/*
	 * T-spins are detected and rewarded upon the locking of
	 * a T tetrimino. The mechanism can be described as follows:
	 * 1. The last maneuver of the T tetrimino must be a rotation.
	 * 2. If there are two minoes in the front corners of the 3 by 3
	 * square occupied by the T (the front corners are ones next to
	 * the sticking out mino of the T) and at least one mino in the
	 * two other corners (to the back), it is a "proper" T-spin.
	 * 3. Otherwise, if there is only one mino in two front corners
	 * and two minoes to the back corners, it is a Mini T-spin.
	 * However, if the last rotation that kicked the T moves its
	 * center 1 by 2 blocks (the last rotation offset of SRS), it is
	 * still a proper T-spin.
	 *
	 * source: https://tetris.wiki/T-Spin
	 *
	 */
	if (!tst_tetromino_t || !tst_rotation_last)
		return TST_NONE;
	if (tst_2front_12back || (tst_1front_2back && tst_rotation_12))
		return TST_REGULAR;
	if (tst_1front_2back)
		return TST_MINI;
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
	if (NULL == figures[0])
		return;

	bool success = false;

	int phase = figures[0]->shape.phase;
	const int (*tests)[2] = (FIGID_I == figures[0]->id) ?
				wallkick_data_figI[phase] :
				wallkick_data[phase];
	rotateFigureCW();
	int whichTestSucceeded = -1;
	for (int i = 0; i < TEST_NUM; ++i)
	{
		if (rotateTest(tests[i][0], tests[i][1]))
		{
			figures[0]->x += tests[i][0];
			figures[0]->y += tests[i][1];
			success = true;
			whichTestSucceeded = i;
			break;
		}
	}
	if (!success)
	{
		rotateFigureCCW();
	}

	if (success && figures[0] && figures[0]->id != FIGID_O)
	{
		updateEasySpin();
		updateLockTime();
		if (lockdelay)
		{
			checkForPrelocking();
		}
		checkForSmoothAnimationCollisions();
		tst_rotation_last = true;
		tst_rotation_12 = 4 == whichTestSucceeded;
	}
}

static void rotate_ccw(void)
{
	if (NULL == figures[0])
		return;

	bool success = false;

	rotateFigureCCW();
	int phase = figures[0]->shape.phase;
	const int (*tests)[2] = (FIGID_I == figures[0]->id) ?
				wallkick_data_figI[phase] :
				wallkick_data[phase];
	int whichTestSucceeded = -1;
	for (int i = 0; i < TEST_NUM; ++i)
	{
		if (rotateTest(-tests[i][0], -tests[i][1]))
		{
			figures[0]->x -= tests[i][0];
			figures[0]->y -= tests[i][1];
			success = true;
			whichTestSucceeded = i;
			break;
		}
	}
	if (!success)
	{
		rotateFigureCW();
	}

	if (success && figures[0] && figures[0]->id != FIGID_O)
	{
		updateEasySpin();
		updateLockTime();
		if (lockdelay)
		{
			checkForPrelocking();
		}
		checkForSmoothAnimationCollisions();
		tst_rotation_last = true;
		tst_rotation_12 = 4 == whichTestSucceeded;
	}
}

static void pause(void)
{
	gamestate = GS_SETTINGS;
}

static void quit(void)
{
	gamestate = GS_MAINMENU;
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
#ifdef DEV
			case SDL_MOUSEBUTTONDOWN:
				if (SDL_BUTTON_LEFT == event.button.button)
				{
					setBlockAtScreenXY(event.button.x, event.button.y, BO_FULL);
				}
				else if (SDL_BUTTON_RIGHT == event.button.button)
				{
					setBlockAtScreenXY(event.button.x, event.button.y, BO_EMPTY);
				}
				break;
			case SDL_MOUSEMOTION:
				// workaround for an apparent SDL bug
				event.button.state = SDL_GetMouseState(NULL, NULL);
				if (SDL_BUTTON_LMASK & event.button.state)
				{
					setBlockAtScreenXY(event.button.x, event.button.y, BO_FULL);
				}
				else if (SDL_BUTTON_RMASK & event.button.state)
				{
					setBlockAtScreenXY(event.button.x, event.button.y, BO_EMPTY);
				}
				break;
#endif
			case SDL_QUIT:
				exit(0);
				break;
		}
}

enum GameOverType checkGameEnd(void)
{
	enum GameOverType ret = GOT_PLAYING;
	// filling up the well
	for (int i = 0; i < BOARD_WIDTH; ++i)
		if (board[i].orientation != BO_EMPTY)
		{
			ret = GOT_FULLWELL;
			break;
		}
	// target number of lines cleared (Sprint only)
	if (GM_SPRINT == menu_gamemode)
	{
		if (lines >= SPRINT_LINE_COUNT)
		{
			ret = GOT_LINECLEAR;
		}
	}
	// time is up (Ultra only)
	if (GM_ULTRA == menu_gamemode)
	{
		Uint32 now = SDL_GetTicks();
		if ((now - game_starttime) > ULTRA_MS_LEN)
		{
			ret = GOT_TIMEUP;
		}
	}
	return ret;
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
		figures[0]->y = -FIG_DIM + 2;
		figures[0]->x = (BOARD_WIDTH - FIG_DIM) / 2;	// center a figure
		memcpy(&figures[0]->shape, getShape(figures[0]->id), sizeof(figures[0]->shape));
		++statistics[figures[0]->id];
	}

	hold_ready = true;
	if (lockdelay)
	{
		checkForPrelocking();
	}
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

		figures[0]->shape.phase = (figures[0]->shape.phase + 1) % PHASE_NUM;
	}
}

void rotateFigureCCW(void)
{
	rotateFigureCW();
	rotateFigureCW();
	rotateFigureCW();
}

bool rotateTest(int x, int y)
{
	figures[0]->x += x;
	figures[0]->y += y;
	bool result = isFigureColliding();
	figures[0]->x -= x;
	figures[0]->y -= y;
	return !result;
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

static void generateDebris(void)
{
	for (int yy = 0; yy < menu_debris; ++yy)
	{
		int bricks_per_row = 0;
		while (bricks_per_row != menu_debris_chance)
		{
			int x = rand() % BOARD_WIDTH;
			int y = BOARD_HEIGHT - INVISIBLE_ROW_COUNT - yy;
			int i = y * BOARD_WIDTH + x;
			if (BO_FULL == board[i].orientation)
				continue;
			board[i].orientation = BO_FULL;
			board[i].color = FIGID_GRAY;
			++bricks_per_row;
		}
	}
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
	b2b = 0;
	combo = 0;
	left_move = false;
	right_move = false;
	lct_deadline = 0;

	updateLCT("", "", "", 0);
	randomizer_reset();

	for (int i = 0; i < FIG_NUM - 1; ++i)
	{
		spawnFigure();
	}

	for (int i = 0; i < FIGID_GRAY; ++i)
	{
		statistics[i] = 0;
	}

	preserved.id = FIGID_END;
	spawnFigure();
	generateDebris();
	game_starttime = SDL_GetTicks();
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

void updateLCT(char *top, char *mid, char *bot, Uint32 ms)
{
	strcpy(lctext_top, top);
	strcpy(lctext_mid, mid);
	strcpy(lctext_bot, bot);
	if (0 != ms)
		lct_deadline = SDL_GetTicks() + ms;
}

void convertMsToStr(Uint32 ms, char *dest)
{
	int hh, mm, ss, cs;
	hh = ms / (1000 * 60 * 60);
	ms = ms % (1000 * 60 * 60);
	mm = ms / (1000 * 60);
	ms = ms % (1000 * 60);
	ss = ms / 1000;
	ms = ms % 1000;
	cs = ms / 10;
	if (hh)
	{
		sprintf(dest, "%02d:%02d:%02d.%02d", hh, mm, ss, cs);
	}
	else
	{
		sprintf(dest, "%02d:%02d.%02d", mm, ss, cs);
	}
}

void updateGTimer(Uint32 ms)
{
	if (GM_ULTRA == menu_gamemode)
	{
		if (ms < ULTRA_MS_LEN)
		{
			ms = ULTRA_MS_LEN - ms;
		}
		else
		{
			ms = 0;
		}
	}
	convertMsToStr(ms, gametimer);
}

void updateHiscores(enum GameMode gm, enum GameOverType got)
{
	switch (gm)
	{
		case GM_MARATHON:
			if (score > getRecord(RT_MARATHON_SCORE))
			{
				setRecord(RT_MARATHON_SCORE, score);
			}
			if (lines > getRecord(RT_MARATHON_LINES))
			{
				setRecord(RT_MARATHON_LINES, lines);
			}
			break;
		case GM_SPRINT:
			if (getRecord(RT_SPRINT_TIME) == 0 ||
				(game_totaltime < getRecord(RT_SPRINT_TIME) &&
				GOT_LINECLEAR == got))
			{
				setRecord(RT_SPRINT_TIME, game_totaltime);
			}
			break;
		case GM_ULTRA:
			if (score > getRecord(RT_ULTRA_SCORE))
			{
				setRecord(RT_ULTRA_SCORE, score);
			}
			if (lines > getRecord(RT_ULTRA_LINES))
			{
				setRecord(RT_ULTRA_LINES, lines);
			}
			break;
	}
	saveRecords();
}

#ifdef DEV
void setBlockAtScreenXY(int x, int y, enum BlockOrientation bo)
{
	// adjust coordinates to the screenscale
	int scale = screenscale == 0 ? 1 : screenscale;
	int px = x / scale;
	int py = y / scale;
	// discard the click if out of bounds
	if (px < gameskin.boardx || py < gameskin.boardy ||
		px >= gameskin.boardx + BOARD_WIDTH * gameskin.bricksize ||
		py >= gameskin.boardy + (BOARD_HEIGHT - INVISIBLE_ROW_COUNT) * gameskin.bricksize)
		return;
	// convert screen coords to board coords
	int bx = (px - gameskin.boardx) / gameskin.bricksize;
	int by = (py - gameskin.boardy) / gameskin.bricksize + INVISIBLE_ROW_COUNT;
	board[by * BOARD_WIDTH + bx].orientation = bo;
	board[by * BOARD_WIDTH + bx].color = FIGID_GRAY;
}
#endif
