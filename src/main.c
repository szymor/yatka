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

struct Figure
{
	enum FigureId id;
	enum FigureId colorid;
};

struct Shape
{
	int blockmap[FIG_DIM*FIG_DIM];
	int cx, cy;		// correction of position after CW rotation
};

SDL_Surface *bg = NULL;

// sprites for blocks
SDL_Surface *gray = NULL;
SDL_Surface *colors[FIGID_END];

enum FigureId *board = NULL;
struct Figure *figures[FIG_NUM];
int f_x, f_y;			// coordinates of the active figure
struct Shape f_shape;	// shape of the active figure
int statistics[FIGID_END];

bool nosound = false;
bool randomcolors = false;
bool holdoff = false;
bool grayblocks = false;
bool ghostoff = false;
bool debug = false;
bool repeattrack = false;
bool numericbars = false;
bool easyspin = false;
bool lockdelay = false;
bool smoothanim = false;

int startlevel = 0;
int nextblocks = MAX_NEXTBLOCKS;
enum Skin skin = SKIN_LEGACY;

enum GameState gamestate = GS_INGAME;
bool hold_ready = true;

int lines = 0, hiscore = 0, old_hiscore, score = 0, level = 0;
int cleared_count = 0;
int tetris_count = 0;

double drop_rate = 2.00;
const double drop_rate_ratio_per_level = 1.20;
Uint32 last_drop_time;
bool softdrop_pressed = false;
Uint32 softdrop_press_time = 0;
Uint32 next_lock_time = 0;

int draw_delta_drop = -BLOCK_SIZE;

bool left_move = false;
bool right_move = false;
Uint32 next_side_move_time;

const struct Shape shape_O =
{
	.blockmap	= { 0, 0, 0, 0,
					0, 1, 1, 0,
					0, 1, 1, 0,
					0, 0, 0, 0 },
	.cx = 0,
	.cy = 0
};

const struct Shape shape_L =
{
	.blockmap	= { 0, 0, 0, 0,
					0, 0, 1, 0,
					1, 1, 1, 0,
					0, 0, 0, 0 },
	.cx = 0,
	.cy = 1
};

const struct Shape shape_J =
{
	.blockmap	= { 0, 0, 0, 0,
					1, 0, 0, 0,
					1, 1, 1, 0,
					0, 0, 0, 0 },
	.cx = 0,
	.cy = 1
};

const struct Shape shape_S =
{
	.blockmap	= { 0, 0, 0, 0,
					0, 1, 1, 0,
					1, 1, 0, 0,
					0, 0, 0, 0 },
	.cx = 0,
	.cy = 1
};

const struct Shape shape_Z =
{
	.blockmap	= { 0, 0, 0, 0,
					1, 1, 0, 0,
					0, 1, 1, 0,
					0, 0, 0, 0 },
	.cx = 0,
	.cy = 1
};

const struct Shape shape_I =
{
	.blockmap	= { 0, 0, 0, 0,
					0, 0, 0, 0,
					1, 1, 1, 1,
					0, 0, 0, 0 },
	.cx = 0,
	.cy = 0
};

const struct Shape shape_T =
{
	.blockmap	= { 0, 0, 0, 0,
					0, 1, 0, 0,
					1, 1, 1, 0,
					0, 0, 0, 0 },
	.cx = 0,
	.cy = 1
};

void initialize(void);
void finalize(void);
SDL_Surface *initBackground(void);

void markDrop(void);
Uint32 getNextDropTime(void);
void setDropRate(int level);
void softDropTimeCounter(void);

void moveLeft(void);
void moveRight(void);

void ingame_processInputEvents(void);
void ingame_updateScreen(void);

void dropSoft(void);
void dropHard(void);
void lockFigure(void);
void holdFigure(void);
void removeFullLines(void);
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
void drawShape(SDL_Surface *target, const struct Shape *sh, int x, int y, SDL_Surface *block, Uint8 alpha, bool centerx, bool centery);
void spawnFigure(void);
struct Figure *getNextFigure(void);
const struct Shape* getShape(enum FigureId id);
enum FigureId getNextColor(enum FigureId id);
enum FigureId getRandomColor(void);
void getShapeDimensions(const struct Shape *shape, int *minx, int *maxx, int *miny, int *maxy);
bool isFigureColliding(void);
void rotateFigureCW(void);
void rotateFigureCCW(void);

int main(int argc, char *argv[])
{
	loadSettings();

	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i],"--nosound"))
			nosound = true;
		else if (!strcmp(argv[i],"--smoothanim"))
			smoothanim = true;
		else if (!strcmp(argv[i],"--randomcolors"))
			randomcolors = true;
		else if (!strcmp(argv[i],"--easyspin"))
			easyspin = true;
		else if (!strcmp(argv[i],"--lockdelay"))
			lockdelay = true;
		else if (!strcmp(argv[i],"--numericbars"))
			numericbars = true;
		else if (!strcmp(argv[i],"--repeattrack"))
			repeattrack = true;
		else if (!strcmp(argv[i],"--debug"))
			debug = true;
		else if (!strcmp(argv[i],"--scale2x"))
			screenscale = 2;
		else if (!strcmp(argv[i],"--scale3x"))
			screenscale = 3;
		else if (!strcmp(argv[i],"--scale4x"))
			screenscale = 4;
		else if (!strcmp(argv[i],"--holdoff"))
			holdoff = true;
		else if (!strcmp(argv[i],"--grayblocks"))
			grayblocks = true;
		else if (!strcmp(argv[i],"--ghostoff"))
			ghostoff = true;
		else if (!strcmp(argv[i],"--startlevel"))
		{
			++i;
			startlevel = atoi(argv[i]);
		}
		else if (!strcmp(argv[i],"--nextblocks"))
		{
			++i;
			nextblocks = atoi(argv[i]);
		}
		else if (!strcmp(argv[i],"--rng"))
		{
			++i;
			int rng;
			if (!strcmp(argv[i], "naive"))
				rng = RA_NAIVE;
			else if (!strcmp(argv[i], "7bag"))
				rng = RA_7BAG;
			else if (!strcmp(argv[i], "8bag"))
				rng = RA_8BAG;
			else
				rng = atoi(argv[i]);
			if (rng >= 0 && rng < RA_END)
				randomalgo = rng;
			else
				printf("Selected RNG ID (%d) is not valid\n", rng);
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
				case GS_INGAME:
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
								++f_y;
								if (isFigureColliding())
								{
									--f_y;
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
	for (int i = 0; i < FIG_NUM; ++i)
	{
		spawnFigure();
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
	screen_scaled = SDL_SetVideoMode(SCREEN_WIDTH * screenscale, SCREEN_HEIGHT * screenscale, SCREEN_BPP, SDL_HWSURFACE | SDL_DOUBLEBUF);
	if (screen_scaled == NULL)
		exit(ERROR_SDLVIDEO);
	screen = screenscale > 1 ? SDL_CreateRGBSurface(SDL_SWSURFACE, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, 0, 0, 0, 0) : screen_scaled;
	SDL_WM_SetCaption("Y A T K A", NULL);
	SDL_ShowCursor(SDL_DISABLE);

	arcade_font = TTF_OpenFont("arcade.ttf", FONT_SIZE);
	if (arcade_font == NULL)
		exit(ERROR_NOFONT);

	ts = IMG_Load("gfx/block.png");
	if (ts == NULL)
		exit(ERROR_NOIMGFILE);
	gray = SDL_DisplayFormat(ts);
	SDL_FreeSurface(ts);

	ts = initBackground();

	SDL_Surface *mask = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, ALT_SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);

	colors[FIGID_I] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, ALT_SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_I], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_I]->format, 215, 64, 0, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_I], NULL);
	ts = colors[FIGID_I];
	colors[FIGID_I] = SDL_DisplayFormatAlpha(ts);
	SDL_FreeSurface(ts);

	colors[FIGID_T] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, ALT_SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_T], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_T]->format, 115, 121, 0, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_T], NULL);
	ts = colors[FIGID_T];
	colors[FIGID_T] = SDL_DisplayFormatAlpha(ts);
	SDL_FreeSurface(ts);

	colors[FIGID_O] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, ALT_SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_O], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_O]->format, 59, 52, 255, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_O], NULL);
	ts = colors[FIGID_O];
	colors[FIGID_O] = SDL_DisplayFormatAlpha(ts);
	SDL_FreeSurface(ts);

	colors[FIGID_S] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, ALT_SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_S], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_S]->format, 0, 132, 96, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_S], NULL);
	ts = colors[FIGID_S];
	colors[FIGID_S] = SDL_DisplayFormatAlpha(ts);
	SDL_FreeSurface(ts);

	colors[FIGID_Z] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, ALT_SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_Z], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_Z]->format, 75, 160, 255, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_Z], NULL);
	ts = colors[FIGID_Z];
	colors[FIGID_Z] = SDL_DisplayFormatAlpha(ts);
	SDL_FreeSurface(ts);

	colors[FIGID_J] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, ALT_SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_J], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_J]->format, 255, 174, 10, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_J], NULL);
	ts = colors[FIGID_J];
	colors[FIGID_J] = SDL_DisplayFormatAlpha(ts);
	SDL_FreeSurface(ts);

	colors[FIGID_L] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, ALT_SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_L], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_L]->format, 255, 109, 247, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_L], NULL);
	ts = colors[FIGID_L];
	colors[FIGID_L] = SDL_DisplayFormatAlpha(ts);
	SDL_FreeSurface(ts);

	SDL_FreeSurface(mask);

	if (!nosound)
	{
		initSound();
	}

	board = malloc(sizeof(int)*BOARD_WIDTH*BOARD_HEIGHT);
	for (int i = 0; i < (BOARD_WIDTH*BOARD_HEIGHT); ++i)
		board[i] = FIGID_END;
	srand((unsigned)time(NULL));
	lines = 0;
	level = startlevel;
	setDropRate(level);
	initFigures();
}

SDL_Surface *initBackground(void)
{
	SDL_Surface *ts = IMG_Load("gfx/bg.png");
	if (ts == NULL)
		exit(ERROR_NOIMGFILE);
	bg = SDL_DisplayFormat(ts);
	SDL_FreeSurface(ts);
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

void markDrop(void)
{
	last_drop_time = SDL_GetTicks();
}

Uint32 getNextDropTime(void)
{
	const double maxDropRate = FPS;
	double coef = (double)(MAX_SOFTDROP_PRESS - softdrop_press_time) / MAX_SOFTDROP_PRESS;
	coef = 1 - coef;
	return last_drop_time + (Uint32)(1000 / (drop_rate * (1 - coef) + maxDropRate * coef));
}

void setDropRate(int level)
{
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
	--f_x;
	if (isFigureColliding())
		++f_x;
	screenFlagUpdate(true);

	next_side_move_time = SDL_GetTicks() + SIDE_MOVE_DELAY;
}

void moveRight(void)
{
	++f_x;
	if (isFigureColliding())
		--f_x;
	screenFlagUpdate(true);

	next_side_move_time = SDL_GetTicks() + SIDE_MOVE_DELAY;
}

void drawFigure(const struct Figure *fig, int x, int y, Uint8 alpha, bool active, bool centerx, bool centery)
{
	if (fig != NULL)
	{
		const struct Shape *shape = NULL;
		if (active)
			shape = &f_shape;
		else
			shape = getShape(fig->id);

		drawShape(screen, shape, x, y, colors[fig->colorid], alpha, centerx, centery);
	}
}

void drawShape(SDL_Surface *target, const struct Shape *sh, int x, int y, SDL_Surface *block, Uint8 alpha, bool centerx, bool centery)
{
	int offset_x = 0, offset_y = 0;
	SDL_Surface *bt = NULL;

	if (alpha < 255)
	{
		bt = SDL_CreateRGBSurface(0, BLOCK_SIZE, BLOCK_SIZE, ALT_SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0x00000000);
		SDL_BlitSurface(block, NULL, bt, NULL);
		SDL_SetAlpha(bt, SDL_SRCALPHA, alpha);
	}
	else
	{
		bt = block;
	}

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

		if (sh->blockmap[i] == 1)
			SDL_BlitSurface(bt, NULL, target, &rect);
	}

	if (alpha < 255)
	{
		SDL_FreeSurface(bt);
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
	for (int i = 0; i < FIGID_END; ++i)
		drawShape(screen, getShape(i), 6, 30 + i * 30, gray, 128, true, true);
	drawBars();
	drawStatus(0, 0);

	// display board
	for (int i = BOARD_WIDTH * INVISIBLE_ROW_COUNT; i < (BOARD_WIDTH * BOARD_HEIGHT); ++i)
	{
		rect.x = (i % BOARD_WIDTH) * BLOCK_SIZE + BOARD_X_OFFSET;
		rect.y = (i / BOARD_WIDTH - INVISIBLE_ROW_COUNT) * BLOCK_SIZE + BOARD_Y_OFFSET;
		if (board[i] < FIGID_END)
			if (grayblocks)
				SDL_BlitSurface(gray, NULL, screen, &rect);
			else
				SDL_BlitSurface(colors[board[i]], NULL, screen, &rect);
	}

	// display the active figure
	int x = BOARD_X_OFFSET + BLOCK_SIZE * f_x;
	int y = BOARD_Y_OFFSET + BLOCK_SIZE * (f_y - INVISIBLE_ROW_COUNT);
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

	// display the ghost figure
	if (figures[0] != NULL && !ghostoff)
	{
		int tfy = f_y;
		while (!isFigureColliding())
			++f_y;
		if (tfy != f_y)
			--f_y;
		if ((f_y - tfy) >= FIG_DIM)
		{
			x = BOARD_X_OFFSET + BLOCK_SIZE * f_x;
			y = BOARD_Y_OFFSET + BLOCK_SIZE * (f_y - INVISIBLE_ROW_COUNT);
			drawFigure(figures[0], x, y, 64, true, false, false);
		}
		f_y = tfy;
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

		for (int i = 0; i < FIGID_END; ++i)
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
		for (int i = 0; i < FIGID_END; ++i)
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
											ALT_SCREEN_BPP,
											0x000000ff,
											0x0000ff00,
											0x00ff0000,
											0xff000000);

	Uint8 alpha_r;
	Uint8 alpha_l;
	Uint32 col;
	SDL_Rect rect;
	int ar = (value / (BAR_WIDTH + 1)) * alpha_step + alpha_start;
	int al = ar + alpha_step + alpha_start;
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
	if (cleared_count)
		ttr = 4 * tetris_count * 100 / cleared_count;
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
	cleared_count += removed;

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

	int oldlevel = level;
	level = startlevel + lines / 30;
	if (level != oldlevel)
	{
		drop_rate *= drop_rate_ratio_per_level;
	}
}

void onGameOver(void)
{
	Mix_FadeOutMusic(MUSIC_FADE_TIME);
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
		++f_y;
		if (isFigureColliding())
		{
			--f_y;
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
			++f_y;
		--f_y;
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
		if (f_shape.blockmap[i] != 0)
		{
			int x = i % FIG_DIM + f_x;
			int y = i / FIG_DIM + f_y;
			if ((x >= 0) && (x < BOARD_WIDTH) && (y >= 0) && (y < BOARD_HEIGHT))
				board[y*BOARD_WIDTH + x] = figures[0]->colorid;
		}

	free(figures[0]);
	figures[0] = NULL;

	removeFullLines();
	if (checkGameEnd())
		onGameOver();

	softdrop_pressed = false;
	softdrop_press_time = 0;
	Mix_PlayChannel(-1, hit, 0);

	next_lock_time = 0;
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

		f_y = -FIG_DIM + 2;						// ...to gain a 'slide' effect from the top of screen
		f_x = (BOARD_WIDTH - FIG_DIM) / 2;		// ...to center a figure

		memcpy(&f_shape, getShape(figures[0]->id), sizeof(f_shape));

		hold_ready = false;

		screenFlagUpdate(true);
	}
}

void removeFullLines(void)
{
	int removed_lines = 0;

	// checking and removing full lines
	for (int y = 1; y < BOARD_HEIGHT; ++y)
	{
		bool flag = true;
		for (int x = 0; x < BOARD_WIDTH; ++x)
		{
			if (board[y*BOARD_WIDTH + x] == FIGID_END)
			{
				flag = false;
				break;
			}
		}
		// removing
		if (flag)
		{
			++lines;
			++removed_lines;
			for (int ys = y-1; ys >= 0; --ys)
				for (int x = 0; x < BOARD_WIDTH; ++x)
					board[(ys+1)*BOARD_WIDTH + x] = board[ys*BOARD_WIDTH + x];
		}
	}

	if (removed_lines > 0)
		onLineClear(removed_lines);
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
							++f_x;
							if (isFigureColliding())
							{
								f_x -= 2;
								if (isFigureColliding())
								{
									rotateFigureCCW();
									++f_x;
									success = false;
								}
							}
						}
						if (success && figures[0] && figures[0]->id != FIGID_O)
						{
							if (easyspin)
								markDrop();
							screenFlagUpdate(true);
						}
					} break;
					case KEY_ROTATE_CCW:
					{
						bool success = true;
						rotateFigureCCW();
						if (isFigureColliding())
						{
							++f_x;
							if (isFigureColliding())
							{
								f_x -= 2;
								if (isFigureColliding())
								{
									rotateFigureCW();
									++f_x;
									success = false;
								}
							}
						}
						if (success && figures[0] && figures[0]->id != FIGID_O)
						{
							if (easyspin)
								markDrop();
							screenFlagUpdate(true);
						}
					} break;
					case KEY_SOFTDROP:
						softdrop_pressed = true;
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

bool checkGameEnd(void)
{
	for (int i = 0; i < BOARD_WIDTH; ++i)
		if (board[i] != FIGID_END)
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
	f_y = -FIG_DIM + 2;						// ...to gain a 'slide' effect from the top of screen
	f_x = (BOARD_WIDTH - FIG_DIM) / 2;		// ...to center a figure

	if (figures[0] != NULL)
	{
		memcpy(&f_shape, getShape(figures[0]->id), sizeof(f_shape));
		++statistics[figures[0]->id];
	}

	hold_ready = true;
}

struct Figure *getNextFigure(void)
{
	struct Figure *f = malloc(sizeof(struct Figure));

	f->id = getNextId();
	f->colorid = getNextColor(f->id);

	return f;
}

const struct Shape* getShape(enum FigureId id)
{
	switch (id)
	{
		case FIGID_O:
			return &shape_O;
		case FIGID_L:
			return &shape_L;
		case FIGID_J:
			return &shape_J;
		case FIGID_S:
			return &shape_S;
		case FIGID_Z:
			return &shape_Z;
		case FIGID_I:
			return &shape_I;
		case FIGID_T:
			return &shape_T;
	}
}

enum FigureId getNextColor(enum FigureId id)
{
	if (randomcolors)
		return getRandomColor();
	else
		return id;
}

enum FigureId getRandomColor(void)
{
	return rand() % FIGID_END;
}

void getShapeDimensions(const struct Shape *shape, int *minx, int *maxx, int *miny, int *maxy)
{
	int min_x = FIG_DIM - 1, max_x = 0;
	int min_y = FIG_DIM - 1, max_y = 0;

	for (int i = 0; i < FIG_DIM*FIG_DIM; ++i)
	{
		int x = i % FIG_DIM;
		int y = i / FIG_DIM;
		if (shape->blockmap[i])
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
				if (f_shape.blockmap[y*FIG_DIM + x] != 0)
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
				if (f_shape.blockmap[y*FIG_DIM + x] != 0)
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
				if (f_shape.blockmap[y*FIG_DIM + x] != 0)
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
		if ((f_x < -empty_columns_left) || (f_x > BOARD_WIDTH-FIG_DIM+empty_columns_right))
			return true;
		if (f_y > (BOARD_HEIGHT-FIG_DIM+empty_rows_down))
			return true;
		for (int i = 0; i < (FIG_DIM*FIG_DIM); ++i)
			if (f_shape.blockmap[i] != 0)
			{
				int x = i % FIG_DIM + f_x;
				int y = i / FIG_DIM + f_y;
				if (((y*BOARD_WIDTH + x) < (BOARD_WIDTH*BOARD_HEIGHT)) && ((y*BOARD_WIDTH + x) >= 0))
					if ((board[y*BOARD_WIDTH + x] < FIGID_END) && (f_shape.blockmap[i] < FIGID_END))
						return true;
			}
	}
	return false;
}

void rotateFigureCW(void)
{
	int temp[FIG_DIM*FIG_DIM];

	memcpy(temp, f_shape.blockmap, sizeof(temp));
	for(int i = 0; i < FIG_DIM*FIG_DIM; ++i)
	{
		int x = i % FIG_DIM;
		int y = i / FIG_DIM;
		f_shape.blockmap[i] = temp[(FIG_DIM-1-x)*FIG_DIM + y];
	}

	f_x += f_shape.cx;
	f_y += f_shape.cy;

	int tcx = f_shape.cx;
	f_shape.cx = -f_shape.cy;
	f_shape.cy = tcx;
}

void rotateFigureCCW(void)
{
	rotateFigureCW();
	rotateFigureCW();
	rotateFigureCW();
}
