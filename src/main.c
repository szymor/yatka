#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_mixer.h>

#include "main.h"
#include "data_persistence.h"
#include "video.h"
#include "sound.h"
#include "state_mainmenu.h"
#include "state_gameover.h"
#include "state_settings.h"
#include "randomizer.h"

#define BOARD_X_OFFSET				100
#define BOARD_Y_OFFSET				0
#define BOARD_WIDTH					10
#define BOARD_HEIGHT				21
#define INVISIBLE_ROW_COUNT			1
#define FIG_DIM						4
#define BLOCK_SIZE					12

#define BAR_WIDTH					26
#define BAR_HEIGHT					8

#define MAX_SOFTDROP_PRESS			300
#define FONT_SIZE					7

#define SIDE_MOVE_DELAY				130
#define FIXED_LOCK_DELAY			500
#define EASY_SPIN_DELAY				500
#define EASY_SPIN_MAX_COUNT			16

#define START_DROP_RATE				2.00

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
};

struct ShapeTemplate
{
	int blockmap[FIG_DIM*FIG_DIM];
	int cx, cy;
};

struct Figure
{
	enum FigureId id;
	enum FigureId color;
	struct Shape shape;
	int x;
	int y;
};

SDL_Surface *bg = NULL;

// sprites for blocks
SDL_Surface *legacy_colors[FIGID_END];
SDL_Surface *plain_colors[FIGID_END];

struct Block *board = NULL;
struct Figure *figures[FIG_NUM];
int statistics[FIGID_GRAY];

bool nosound = false;
bool holdoff = false;
bool grayblocks = false;
bool debug = false;
bool repeattrack = false;
bool numericbars = false;
bool easyspin = false;
bool lockdelay = false;
bool smoothanim = false;

int nextblocks = MAX_NEXTBLOCKS;
int ghostalpha = 64;
enum TetrominoColor tetrominocolor = TC_PIECEWISE;
enum TetrominoStyle tetrominostyle = TS_LEGACY;

enum GameState gamestate = GS_MAINMENU;
static bool hold_ready = true;

static int lines = 0, lines_level_up = 0, level = 0;
static int hiscore = 0, old_hiscore = 0, score = 0;
static int tetris_count = 0;

static double drop_rate = START_DROP_RATE;
static const double drop_rate_ratio_per_level = 1.20;
static Uint32 last_drop_time;
static bool easyspin_pressed = false;
static int easyspin_counter = 0;
static bool softdrop_pressed = false;
static Uint32 softdrop_press_time = 0;
static Uint32 next_lock_time = 0;

static int draw_delta_drop = -BLOCK_SIZE;

static bool left_move = false;
static bool right_move = false;
static Uint32 next_side_move_time;

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
SDL_Surface *initBackground(void);
struct Shape *generateFromTemplate(const struct ShapeTemplate *template);

void markDrop(void);
Uint32 getNextDropTime(void);
void setDropRate(int level);
void softDropTimeCounter(void);

void moveLeft(void);
void moveRight(void);

void ingame_processInputEvents(void);
void ingame_updateScreen(void);

void resetGame(void);
void dropSoft(void);
void dropHard(void);
void lockFigure(void);
void holdFigure(void);
int removeFullLines(void);
bool checkGameEnd(void);
void drawBars(void);
void drawBar(int x, int y, int value);
void drawStatus(int x, int y);
void onDrop(void);
void onCollide(void);
void onLineClear(int removed);
void onGameOver(void);

void initFigures(void);
void drawFigure(const struct Figure *fig, int x, int y, Uint8 alpha, bool active, bool centerx, bool centery);
void drawShape(SDL_Surface *target, const struct Shape *sh, int x, int y, enum FigureId color, Uint8 alpha, bool centerx, bool centery);
void spawnFigure(void);
struct Figure *getNextFigure(void);
struct Shape* getShape(enum FigureId id);
enum FigureId getNextColor(enum FigureId id);
enum FigureId getRandomColor(void);
void getShapeDimensions(const struct Shape *shape, int *minx, int *maxx, int *miny, int *maxy);
bool isFigureColliding(void);
void rotateFigureCW(void);
void rotateFigureCCW(void);
SDL_Surface *getBlock(enum FigureId color, enum BlockOrientation orient, SDL_Rect *srcrect);

int main(int argc, char *argv[])
{
	loadSettings();

	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i],"--nosound"))
			nosound = true;
		else if (!strcmp(argv[i],"--sound"))
			nosound = false;
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
					if (!frameLimiter() || debug)
					{
						ingame_processInputEvents();
						ingame_updateScreen();
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
								next_lock_time = 0;
						}
						if (ct > next_side_move_time)
						{
							if (left_move && !right_move)
							{
								moveLeft();
							}
							else if (right_move && !left_move)
							{
								moveRight();
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

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0)
		exit(ERROR_SDLINIT);
	if (TTF_Init() < 0)
		exit(ERROR_TTFINIT);
	atexit(finalize);
	screen_scaled = SDL_SetVideoMode(SCREEN_WIDTH * screenscale, SCREEN_HEIGHT * screenscale, SCREEN_BPP, VIDEO_MODE_FLAGS);
	if (screen_scaled == NULL)
		exit(ERROR_SDLVIDEO);
	screen = screenscale > 1 ? SDL_CreateRGBSurface(SDL_SWSURFACE, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, 0, 0, 0, 0) : screen_scaled;
	SDL_WM_SetCaption("Y A T K A", NULL);
	SDL_ShowCursor(SDL_DISABLE);

	arcade_font = TTF_OpenFont("arcade.ttf", FONT_SIZE);
	if (arcade_font == NULL)
		exit(ERROR_NOFONT);

	ts = IMG_Load("gfx/legacy.png");
	if (ts == NULL)
		exit(ERROR_NOIMGFILE);
	legacy_colors[FIGID_GRAY] = SDL_DisplayFormat(ts);
	SDL_FreeSurface(ts);

	ts = IMG_Load("gfx/plain.png");
	if (ts == NULL)
		exit(ERROR_NOIMGFILE);
	plain_colors[FIGID_GRAY] = SDL_DisplayFormat(ts);
	SDL_FreeSurface(ts);

	SDL_PixelFormat *f = screen->format;
	SDL_SetAlpha(legacy_colors[FIGID_GRAY], SDL_SRCALPHA, 128);
	SDL_SetAlpha(plain_colors[FIGID_GRAY], SDL_SRCALPHA, 128);

	Uint32 rgb[] = {
		SDL_MapRGB(f, 215, 64, 0),
		SDL_MapRGB(f, 59, 52, 255),
		SDL_MapRGB(f, 115, 121, 0),
		SDL_MapRGB(f, 0, 132, 96),
		SDL_MapRGB(f, 75, 160, 255),
		SDL_MapRGB(f, 255, 174, 10),
		SDL_MapRGB(f, 255, 109, 247)
	};

	// tetromino dyeing
	for (int i = 0; i < FIGID_GRAY; ++i)
	{
		legacy_colors[i] = SDL_CreateRGBSurface(0, legacy_colors[FIGID_GRAY]->w, legacy_colors[FIGID_GRAY]->h, f->BitsPerPixel, f->Rmask, f->Gmask, f->Bmask, 0);
		SDL_FillRect(legacy_colors[i], NULL, rgb[i]);
		SDL_BlitSurface(legacy_colors[FIGID_GRAY], NULL, legacy_colors[i], NULL);

		plain_colors[i] = SDL_CreateRGBSurface(0, plain_colors[FIGID_GRAY]->w, plain_colors[FIGID_GRAY]->h, f->BitsPerPixel, f->Rmask, f->Gmask, f->Bmask, 0);
		SDL_FillRect(plain_colors[i], NULL, rgb[i]);
		SDL_BlitSurface(plain_colors[FIGID_GRAY], NULL, plain_colors[i], NULL);
	}

	if (!nosound)
	{
		initSound();
	}

	// shape init
	for (int i = 0; i < FIGID_GRAY; ++i)
	{
		shapes[i] = generateFromTemplate(&templates[i]);
	}

	board = malloc(sizeof(struct Block)*BOARD_WIDTH*BOARD_HEIGHT);

	srand((unsigned)time(NULL));

	initFigures();
	initBackground();
	mainmenu_init();

	resetGame();
}

SDL_Surface *initBackground(void)
{
	if (bg != NULL)
	{
		SDL_FreeSurface(bg);
	}

	SDL_Surface *ts = IMG_Load("gfx/bg.png");
	if (ts == NULL)
		exit(ERROR_NOIMGFILE);
	bg = SDL_DisplayFormat(ts);
	SDL_FreeSurface(ts);

	// figures for statistics
	for (int i = 0; i < FIGID_GRAY; ++i)
		drawShape(bg, getShape(i), 6, 30 + i * 30, FIGID_GRAY, 160, true, true);

	// placeholders for next tetrominoes
	SDL_Surface *ph = SDL_CreateRGBSurface(0, 48, 24, bg->format->BitsPerPixel, bg->format->Rmask, bg->format->Gmask, bg->format->Bmask, 0);
	SDL_FillRect(ph, NULL, SDL_MapRGB(ph->format, 0xff, 0xff, 0xff));
	SDL_SetAlpha(ph, SDL_SRCALPHA, 48);
	for (int i = 0; i < nextblocks; ++i)
	{
		SDL_Rect r = { .x = 246, .y = 22 + 30 * i };
		SDL_BlitSurface(ph, NULL, bg, &r);
	}
	SDL_FreeSurface(ph);

	// placeholder for the board
	int w = BOARD_WIDTH * BLOCK_SIZE;
	int h = (BOARD_HEIGHT - INVISIBLE_ROW_COUNT) * BLOCK_SIZE;
	ph = SDL_CreateRGBSurface(0, w, h, bg->format->BitsPerPixel, bg->format->Rmask, bg->format->Gmask, bg->format->Bmask, 0);
	SDL_FillRect(ph, NULL, SDL_MapRGB(ph->format, 0xff, 0xff, 0xff));
	SDL_SetAlpha(ph, SDL_SRCALPHA, 48);
	SDL_Rect r = { .x = BOARD_X_OFFSET, .y = BOARD_Y_OFFSET };
	SDL_BlitSurface(ph, NULL, bg, &r);
	SDL_FreeSurface(ph);

	return bg;
}

void finalize(void)
{
	if (hiscore > old_hiscore)
		saveHiscore(hiscore);
	if (settings_changed)
		saveSettings();

	TTF_CloseFont(arcade_font);
	TTF_Quit();
	if (!nosound)
		deinitSound();
	SDL_Quit();
	if(board != NULL)
		free(board);
	for (int i = 0; i < FIG_NUM; ++i)
		free(figures[i]);
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
	if (easyspin_pressed && easyspin_counter <= EASY_SPIN_MAX_COUNT)
		return last_drop_time + EASY_SPIN_DELAY;

	const double maxDropRate = FPS;
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

void moveLeft(void)
{
	if (figures[0] != NULL)
	{
		--figures[0]->x;
		if (isFigureColliding())
			++figures[0]->x;
		screenFlagUpdate(true);

		next_side_move_time = SDL_GetTicks() + SIDE_MOVE_DELAY;
	}
}

void moveRight(void)
{
	if (figures[0] != NULL)
	{
		++figures[0]->x;
		if (isFigureColliding())
			--figures[0]->x;
		screenFlagUpdate(true);

		next_side_move_time = SDL_GetTicks() + SIDE_MOVE_DELAY;
	}
}

void drawFigure(const struct Figure *fig, int x, int y, Uint8 alpha, bool active, bool centerx, bool centery)
{
	if (fig != NULL)
	{
		const struct Shape *shape = NULL;
		if (active)
			shape = &figures[0]->shape;
		else
			shape = getShape(fig->id);

		drawShape(screen, shape, x, y, fig->color, alpha, centerx, centery);
	}
}

void drawShape(SDL_Surface *target, const struct Shape *sh, int x, int y, enum FigureId color, Uint8 alpha, bool centerx, bool centery)
{
	int offset_x = 0, offset_y = 0;
	SDL_Rect srcrect;

	if (centerx || centery)
	{
		int minx, maxx, miny, maxy, w, h;
		getShapeDimensions(sh, &minx, &maxx, &miny, &maxy);
		if (centerx)
		{
			w = maxx - minx + 1;
			offset_x = (4 - w) * BLOCK_SIZE / 2 - minx * BLOCK_SIZE;
		}
		if (centery)
		{
			h = maxy - miny + 1;
			offset_y = (2 - h) * BLOCK_SIZE / 2 - miny * BLOCK_SIZE;
		}
	}

	for (int i = 0; i < (FIG_DIM * FIG_DIM); ++i)
	{
		SDL_Rect rect;
		rect.x = (i % FIG_DIM) * BLOCK_SIZE + x + offset_x;
		rect.y = (i / FIG_DIM) * BLOCK_SIZE + y + offset_y;

		SDL_Surface *block = getBlock(color, sh->blockmap[i], &srcrect);
		SDL_SetAlpha(block, SDL_SRCALPHA, alpha);

		if (sh->blockmap[i])
			SDL_BlitSurface(block, &srcrect, target, &rect);
	}
}

void ingame_updateScreen(void)
{
	if (!screenFlagUpdate(false) && !debug && !smoothanim)
		return;

	SDL_Rect rect;
	SDL_Surface *mask;
	SDL_Surface *digit;

	// display background
	rect.x = 0;
	rect.y = 0;
	SDL_BlitSurface(bg, NULL, screen, &rect);

	// display next figures
	for (int i = 1; i <= nextblocks; ++i)
	{
		drawFigure(figures[i], 246, 22 + 30 * (i - 1), 255, false, true, true);
	}

	// display statistics
	drawBars();
	drawStatus(0, 0);

	// display board
	for (int i = BOARD_WIDTH * INVISIBLE_ROW_COUNT; i < (BOARD_WIDTH * BOARD_HEIGHT); ++i)
	{
		rect.x = (i % BOARD_WIDTH) * BLOCK_SIZE + BOARD_X_OFFSET;
		rect.y = (i / BOARD_WIDTH - INVISIBLE_ROW_COUNT) * BLOCK_SIZE + BOARD_Y_OFFSET;
		if (board[i].orientation != BO_EMPTY)
		{
			SDL_Rect srcrect;
			SDL_Surface *block = grayblocks ? getBlock(FIGID_GRAY, board[i].orientation, &srcrect) : getBlock(board[i].color, board[i].orientation, &srcrect);
			SDL_SetAlpha(block, SDL_SRCALPHA, 255);
			SDL_BlitSurface(block, &srcrect, screen, &rect);
		}
	}

	// display the active figure
	if (figures[0] != NULL)
	{
		int x = BOARD_X_OFFSET + BLOCK_SIZE * figures[0]->x;
		int y = BOARD_Y_OFFSET + BLOCK_SIZE * (figures[0]->y - INVISIBLE_ROW_COUNT);
		if (smoothanim)
		{
			Uint32 ct = SDL_GetTicks();
			double fraction = (double)(ct - last_drop_time) / (getNextDropTime() - last_drop_time);
			int new_delta = BLOCK_SIZE * fraction - BLOCK_SIZE;
			if (new_delta > draw_delta_drop)
			{
				draw_delta_drop = new_delta;
				if (draw_delta_drop > 0)
					draw_delta_drop = 0;
			}
			y += draw_delta_drop;
		}
		drawFigure(figures[0], x, y, 255, true, false, false);
	}

	// display the ghost figure
	if (figures[0] != NULL && ghostalpha > 0)
	{
		int tfy = figures[0]->y;
		while (!isFigureColliding())
			++figures[0]->y;
		if (tfy != figures[0]->y)
			--figures[0]->y;
		if ((figures[0]->y - tfy) >= FIG_DIM)
		{
			int x = BOARD_X_OFFSET + BLOCK_SIZE * figures[0]->x;
			int y = BOARD_Y_OFFSET + BLOCK_SIZE * (figures[0]->y - INVISIBLE_ROW_COUNT);
			drawFigure(figures[0], x, y, ghostalpha, true, false, false);
		}
		figures[0]->y = tfy;
	}

	flipScreenScaled();
	frameCounter();
}

void drawBars(void)
{
	if (numericbars)
	{
		const SDL_Color white = {.r = 255, .g = 255, .b = 255};
		SDL_Surface *text = NULL;
		SDL_Rect rect;
		char buff[256];

		for (int i = 0; i < FIGID_GRAY; ++i)
		{
			sprintf(buff, "%d", statistics[i]);
			text = TTF_RenderUTF8_Blended(arcade_font, buff, white);
			rect.x = 64 + BAR_WIDTH - text->w;
			rect.y = 38 + i * 30;
			SDL_BlitSurface(text, NULL, screen, &rect);
			SDL_FreeSurface(text);
		}
	}
	else
	{
		for (int i = 0; i < FIGID_GRAY; ++i)
		{
			drawBar(64, 38 + i * 30, statistics[i]);
		}
	}
}

void drawBar(int x, int y, int value)
{
	const int alpha_step = 48;
	const int alpha_start = 32;

	SDL_Surface *bar = SDL_CreateRGBSurface(SDL_SRCALPHA,
											BAR_WIDTH,
											BAR_HEIGHT,
											32,
											0x000000ff,
											0x0000ff00,
											0x00ff0000,
											0xff000000);

	Uint8 alpha_r;
	Uint8 alpha_l;
	Uint32 col;
	SDL_Rect rect;
	int ar = (value / (BAR_WIDTH + 1)) * alpha_step + alpha_start;
	int al = ar + alpha_step;
	ar = ar > 255 ? 255 : ar;
	al = al > 255 ? 255 : al;
	alpha_l = (Uint8)al;
	alpha_r = (Uint8)ar;

	rect.x = 0;
	rect.y = 0;
	rect.w = value % (BAR_WIDTH + 1);
	rect.h = BAR_HEIGHT;
	col = SDL_MapRGBA(bar->format, 255, 255, 255, alpha_l);
	SDL_FillRect(bar, &rect, col);

	rect.x = rect.w;
	rect.y = 0;
	rect.w = BAR_WIDTH - rect.w;
	rect.h = BAR_HEIGHT;
	col = SDL_MapRGBA(bar->format, 255, 255, 255, alpha_r);
	SDL_FillRect(bar, &rect, col);

	rect.x = x;
	rect.y = y;
	rect.w = BAR_WIDTH;
	rect.h = BAR_HEIGHT;
	SDL_BlitSurface(bar, NULL, screen, &rect);
	SDL_FreeSurface(bar);
}

void drawStatus(int x, int y)
{
	SDL_Color col = {.r = 255, .g = 255, .b = 255};
	SDL_Surface *text = NULL;
	SDL_Rect rect = {.x = x, .y = y};
	char buff[256];

	sprintf(buff, "Hiscore: %d", hiscore);
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	SDL_BlitSurface(text, NULL, screen, &rect);
	SDL_FreeSurface(text);

	rect.y += FONT_SIZE;
	sprintf(buff, "Score: %d", score);
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	SDL_BlitSurface(text, NULL, screen, &rect);
	SDL_FreeSurface(text);

	rect.y += FONT_SIZE;
	sprintf(buff, "Level: %d", level);
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	SDL_BlitSurface(text, NULL, screen, &rect);
	SDL_FreeSurface(text);

	rect.y += FONT_SIZE;
	sprintf(buff, "Lines: %d", lines);
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	SDL_BlitSurface(text, NULL, screen, &rect);
	SDL_FreeSurface(text);

	rect.y += FONT_SIZE;
	int ttr;
	if (lines != 0)
		ttr = 4 * tetris_count * 100 / lines;
	else
		ttr = 0;
	sprintf(buff, "Tetris: %d%%", ttr);
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	SDL_BlitSurface(text, NULL, screen, &rect);
	SDL_FreeSurface(text);

	if (debug)
	{
		rect.y += FONT_SIZE;
		sprintf(buff, "FPS: %d", fps);
		text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
		SDL_BlitSurface(text, NULL, screen, &rect);
		SDL_FreeSurface(text);
	}
}

void onDrop(void)
{
	markDrop();
	screenFlagUpdate(true);
	if (!next_lock_time && smoothanim)
		draw_delta_drop = -BLOCK_SIZE;
	easyspin_pressed = false;
}

void onCollide(void)
{
	if (!next_lock_time)
		if (lockdelay)
			next_lock_time = last_drop_time + FIXED_LOCK_DELAY;
		else
			next_lock_time = getNextDropTime();
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
			onCollide();
		}
		else
			next_lock_time = 0;
		onDrop();
	}
}

void dropHard(void)
{
	if (figures[0] != NULL)
	{
		while (!isFigureColliding())
			++figures[0]->y;
		--figures[0]->y;
		onCollide();
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

		screenFlagUpdate(true);
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

void ingame_processInputEvents(void)
{
	SDL_Event event;

	if (SDL_PollEvent(&event))
		switch (event.type)
		{
			case SDL_KEYUP:
				switch (event.key.keysym.sym)
				{
					case KEY_SOFTDROP:
						softdrop_pressed = false;
						softdrop_press_time = 0;
						break;
					case KEY_LEFT:
						left_move = false;
						break;
					case KEY_RIGHT:
						right_move = false;
						break;
				}
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case KEY_ROTATE_CW:
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
							if (easyspin)
							{
								markDrop();
								easyspin_pressed = true;
								++easyspin_counter;
								next_lock_time = 0;
							}
							screenFlagUpdate(true);
						}
					} break;
					case KEY_ROTATE_CCW:
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
							if (easyspin)
							{
								markDrop();
								easyspin_pressed = true;
								++easyspin_counter;
								next_lock_time = 0;
							}
							screenFlagUpdate(true);
						}
					} break;
					case KEY_SOFTDROP:
						softdrop_pressed = true;
						easyspin_pressed = false;
						break;
					case KEY_HARDDROP:
						dropHard();
						break;
					case KEY_HOLD:
						holdFigure();
						break;
					case KEY_LEFT:
						left_move = true;
						moveLeft();
						break;
					case KEY_RIGHT:
						right_move = true;
						moveRight();
						break;
					case KEY_PAUSE:
						gamestate = GS_SETTINGS;
						break;
					case KEY_QUIT:
						gamestate = GS_MAINMENU;
						if (hiscore > old_hiscore)
						{
							saveHiscore(hiscore);
							old_hiscore = hiscore;
						}
						break;
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
		case TC_PIECEWISE:
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

SDL_Surface *getBlock(enum FigureId color, enum BlockOrientation orient, SDL_Rect *srcrect)
{
	if (srcrect)
	{
		srcrect->x = 0;
		srcrect->y = 0;
		srcrect->w = BLOCK_SIZE;
		srcrect->h = BLOCK_SIZE;
	}

	SDL_Surface *s = NULL;
	switch (tetrominostyle)
	{
		case TS_LEGACY:
			s = legacy_colors[color];
			break;
		case TS_PLAIN:
			if (srcrect)
				srcrect->x = BO_FULL * BLOCK_SIZE - BLOCK_SIZE;
			s = plain_colors[color];
			break;
		case TS_TENGENISH:
			if (srcrect)
				srcrect->x = orient * BLOCK_SIZE - BLOCK_SIZE;
			s = plain_colors[color];
			break;
		default:
			s = NULL;
	}

	return s;
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
	left_move = false;
	right_move = false;

	for (int i = 0; i < FIG_NUM; ++i)
	{
		spawnFigure();
	}

	for (int i = 0; i < FIGID_GRAY; ++i)
	{
		statistics[i] = 0;
	}

	// debris
	for (int i = 0; i < (BOARD_WIDTH * BOARD_HEIGHT); ++i)
	{
		int y = i / BOARD_WIDTH;
		y = BOARD_HEIGHT - INVISIBLE_ROW_COUNT - y;
		if ((y < menu_debris) && ((rand() % 128) < (menu_debris_chance * 128 / 10)))
		{
			board[i].color = FIGID_GRAY;
			board[i].orientation = BO_FULL;
		}
	}

	// to do - fix drop rate
}
