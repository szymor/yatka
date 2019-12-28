#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_mixer.h>

#include "main.h"
#include "hiscore.h"
#include "video.h"
#include "sound.h"

#define BOARD_X_OFFSET		100
#define BOARD_Y_OFFSET		0
#define BOARD_WIDTH			10
#define BOARD_HEIGHT		20
#define FIG_DIM				4
#define BLOCK_SIZE			12
#define FIG_NUM				7		// including the active figure

#define BAR_WIDTH			26
#define BAR_HEIGHT			8

#define MAX_SOFTDROP_PRESS	300
#define KEY_REPEAT_RATE		130		// in ms
#define FONT_SIZE			7

#define MAX8BAGID			8

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

enum RandomAlgo
{
	RA_NAIVE,
	RA_7BAG,
	RA_8BAG,
	RA_END
};

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

SDL_Surface *screen = NULL;
SDL_Surface *screen_scaled = NULL;
SDL_Surface *bg = NULL;
SDL_Surface *fg = NULL;

// sprites for blocks
SDL_Surface *gray = NULL;
SDL_Surface *colors[FIGID_END];

TTF_Font *arcade_font = NULL;

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
bool debugfps = false;
bool repeattrack = false;
bool numericbars = false;

int screenscale = 1;
int startlevel = 0;
int nextblocks = FIG_NUM - 1;
enum RandomAlgo randomalgo = RA_8BAG;
enum GameState gamestate = GS_PLAYING;
bool hold_ready = true;

int lines = 0, hiscore = 0, old_hiscore, score = 0, level = 0;

double drop_rate = 2.00;
const double drop_rate_ratio_per_level = 1.20;
Uint32 last_drop_time;
bool softdrop_pressed = false;
Uint32 softdrop_press_time = 0;

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

void markDrop(void);
Uint32 getNextDropTime(void);
void setDropRate(int level);
void softDropTimeCounter(void);

void displayBoard(void);
void dropSoft(void);
void dropHard(void);
void lockFigure(void);
void holdFigure(void);
void removeFullLines(void);
void handleInput(void);
bool checkGameEnd(void);
void drawBars(void);
void drawBar(int x, int y, int value);
void drawStatus(int x, int y);
void onDrop(void);
void onCollide(void);
void onLineClear(int removed);
void onGameOver(void);

void initFigures(void);
void drawFigure(const struct Figure *fig, int x, int y, bool screendim, bool ghost);
void spawnFigure(void);
struct Figure *getNextFigure(void);
const struct Shape* getShape(enum FigureId id);
enum FigureId getNextId(void);
enum FigureId getRandomId(void);
enum FigureId getRandom7BagId(void);
enum FigureId getRandom8BagId(void);
enum FigureId getNextColor(enum FigureId id);
enum FigureId getRandomColor(void);
void getShapeDimensions(const struct Shape *shape, int *minx, int *maxx, int *miny, int *maxy);
bool isFigureColliding(void);
void rotateFigureCW(void);
void rotateFigureCCW(void);

int main(int argc, char *argv[])
{
	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i],"--nosound"))
			nosound = true;
		else if (!strcmp(argv[i],"--randomcolors"))
			randomcolors = true;
		else if (!strcmp(argv[i],"--numericbars"))
			numericbars = true;
		else if (!strcmp(argv[i],"--repeattrack"))
			repeattrack = true;
		else if (!strcmp(argv[i],"--debugfps"))
			debugfps = true;
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
		if (!frameLimiter() || debugfps)
		{
			handleInput();
			displayBoard();
			softDropTimeCounter();

			if (SDL_GetTicks() > getNextDropTime())
			{
				if (GS_PLAYING == gamestate)
				{
					dropSoft();
				}
			}
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

	ts = IMG_Load("gfx/bg.png");
	if (ts == NULL)
		exit(ERROR_NOIMGFILE);
	bg = SDL_DisplayFormat(ts);
	SDL_FreeSurface(ts);

	ts = IMG_Load("gfx/block.png");
	if (ts == NULL)
		exit(ERROR_NOIMGFILE);
	gray = SDL_DisplayFormat(ts);
	SDL_FreeSurface(ts);

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
		int mixflags = MIX_INIT_OGG;
		if (Mix_Init(mixflags) != mixflags)
			exit(ERROR_MIXINIT);
		if (Mix_OpenAudio(11025, MIX_DEFAULT_FORMAT, 1, 1024) < 0)
			exit(ERROR_OPENAUDIO);
		music[0] = Mix_LoadMUS("sfx/korobeyniki.ogg");
		if (!music[0])
			exit(ERROR_NOSNDFILE);
		music[1] = Mix_LoadMUS("sfx/bradinsky.ogg");
		if (!music[1])
			exit(ERROR_NOSNDFILE);
		music[2] = Mix_LoadMUS("sfx/kalinka.ogg");
		if (!music[2])
			exit(ERROR_NOSNDFILE);
		music[3] = Mix_LoadMUS("sfx/troika.ogg");
		if (!music[3])
			exit(ERROR_NOSNDFILE);
		hit = Mix_LoadWAV("sfx/hit.ogg");
		if (!hit)
			exit(ERROR_NOSNDFILE);

		current_track = 0;
		Mix_FadeInMusic(music[current_track], 1, MUSIC_FADE_TIME);
		Mix_HookMusicFinished(trackFinished);
		Mix_VolumeMusic(MIX_MAX_VOLUME / 2);
	}
	SDL_EnableKeyRepeat(1, KEY_REPEAT_RATE);

	board = malloc(sizeof(int)*BOARD_WIDTH*BOARD_HEIGHT);
	for (int i = 0; i < (BOARD_WIDTH*BOARD_HEIGHT); ++i)
		board[i] = FIGID_END;
	srand((unsigned)time(NULL));
	lines = 0;
	level = startlevel;
	setDropRate(level);
	initFigures();
}

void finalize(void)
{
	if (hiscore > old_hiscore)
		saveHiscore(hiscore);

	TTF_CloseFont(arcade_font);
	TTF_Quit();
	if (!nosound)
		Mix_Quit();
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
	coef *= coef;
	return last_drop_time + (Uint32)(1000 / (drop_rate * (1 - coef) + maxDropRate * coef));
}

void setDropRate(int level)
{
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

void drawFigure(const struct Figure *fig, int x, int y, bool screendim, bool ghost)
{
	if (fig != NULL)
	{
		const struct Shape *shape = NULL;
		int minx, maxx, miny, maxy, w, h, offset_x, offset_y;
		SDL_Surface *block = NULL;

		if (ghost)
		{
			block = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, ALT_SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
			SDL_FillRect(block, NULL, SDL_MapRGBA(block->format, 0, 0, 0, 64));
			SDL_BlitSurface(colors[fig->colorid], NULL, block, NULL);
		}
		else
		{
			block = colors[fig->colorid];
		}

		if (screendim)
		{
			shape = getShape(fig->id);
			getShapeDimensions(shape, &minx, &maxx, &miny, &maxy);
			w = maxx - minx + 1;
			h = maxy - miny + 1;
			offset_x = (4 - w) * BLOCK_SIZE / 2;
			offset_y = (2 - h) * BLOCK_SIZE / 2;
		}

		for (int i = 0; i < (FIG_DIM*FIG_DIM); ++i)
		{
			SDL_Rect rect;
			if (screendim)
			{
				rect.x = (i % FIG_DIM - minx) * BLOCK_SIZE + x + offset_x;
				rect.y = (i / FIG_DIM - miny) * BLOCK_SIZE + y + offset_y;
				if (shape->blockmap[i] == 1)
					SDL_BlitSurface(block, NULL, screen, &rect);
			}
			else
			{
				rect.x = (i % FIG_DIM + x) * BLOCK_SIZE + BOARD_X_OFFSET;
				rect.y = (i / FIG_DIM + y) * BLOCK_SIZE + BOARD_Y_OFFSET;
				if (f_shape.blockmap[i] == 1)
					SDL_BlitSurface(block, NULL, screen, &rect);
			}
		}

		if (ghost)
		{
			SDL_FreeSurface(block);
		}
	}
}

void displayBoard(void)
{
	if (!screenFlagUpdate(false) && !debugfps)
		return;

	SDL_Rect rect;
	SDL_Surface *mask;
	SDL_Surface *digit;

	// display background
	rect.x = 0;
	rect.y = 0;
	SDL_BlitSurface(bg, NULL, screen, &rect);

	// display next figures
	for (int i = 1; i < nextblocks + 1; ++i)
	{
		drawFigure(figures[i], 246, 22 + 30 * (i - 1), true, false);
	}

	// display statistics
	drawBars();
	drawStatus(0, 0);

	// display board
	for (int i = 0; i < (BOARD_WIDTH*BOARD_HEIGHT); ++i)
	{
		rect.x = (i % BOARD_WIDTH) * BLOCK_SIZE + BOARD_X_OFFSET;
		rect.y = (i / BOARD_WIDTH) * BLOCK_SIZE + BOARD_Y_OFFSET;
		if (board[i] < FIGID_END)
			if (grayblocks)
				SDL_BlitSurface(gray, NULL, screen, &rect);
			else
				SDL_BlitSurface(colors[board[i]], NULL, screen, &rect);
	}

	// display the active figure
	if (GS_GAMEOVER != gamestate)
		drawFigure(figures[0], f_x, f_y, false, false);

	// display the ghost figure
	if (figures[0] != NULL && !ghostoff)
	{
		int tfy = f_y;
		while (!isFigureColliding())
			++f_y;
		if (tfy != f_y)
			--f_y;
		if ((f_y - tfy) >= FIG_DIM)
			drawFigure(figures[0], f_x, f_y, false, true);
		f_y = tfy;
	}

	if(GS_PLAYING != gamestate)
	{
		mask = SDL_CreateRGBSurface(SDL_SRCALPHA, SCREEN_WIDTH, SCREEN_HEIGHT, ALT_SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
		SDL_FillRect(mask, NULL, SDL_MapRGBA(mask->format,0,0,0,128));
		SDL_BlitSurface(mask, NULL, screen, NULL);
		SDL_FreeSurface(mask);
	}

	if (SDL_MUSTLOCK(screen_scaled))
		SDL_LockSurface(screen_scaled);
	switch (screenscale)
	{
		case 2:
			upscale2(screen_scaled->pixels, screen->pixels);
			break;
		case 3:
			upscale3(screen_scaled->pixels, screen->pixels);
			break;
		case 4:
			upscale4(screen_scaled->pixels, screen->pixels);
			break;
		default:
			break;
	}
	if (SDL_MUSTLOCK(screen_scaled))
		SDL_UnlockSurface(screen_scaled);

	SDL_Flip(screen_scaled);
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

	if (debugfps)
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
}

void onCollide(void)
{
	lockFigure();
	free(figures[0]);
	figures[0] = NULL;

	removeFullLines();
	if (checkGameEnd())
		onGameOver();

	softdrop_pressed = false;
	softdrop_press_time = 0;
	Mix_PlayChannel(-1, hit, 0);
}

void onLineClear(int removed)
{
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

		f_y = -FIG_DIM + 1;						// ...to gain a 'slide' effect from the top of screen
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

void handleInput(void)
{
	static bool rotatecw_key_state = false;
	static bool rotateccw_key_state = false;
	static bool pause_key_state = false;
	static bool softdrop_key_state = false;
	static bool harddrop_key_state = false;
	static bool hold_key_state = false;
	static bool left_key_state = false;
	static bool right_key_state = false;
	SDL_Event event;

	if (SDL_PollEvent(&event))
		switch (event.type)
		{
			case SDL_KEYUP:
				switch (event.key.keysym.sym)
				{
					case KEY_ROTATE_CW:
						rotatecw_key_state = false;
						break;
					case KEY_ROTATE_CCW:
						rotateccw_key_state = false;
						break;
					case KEY_PAUSE:
						pause_key_state = false;
						break;
					case KEY_SOFTDROP:
						softdrop_key_state = false;
						softdrop_pressed = false;
						softdrop_press_time = 0;
						break;
					case KEY_HARDDROP:
						harddrop_key_state = false;
						break;
					case KEY_HOLD:
						hold_key_state = false;
						break;
					case KEY_LEFT:
						left_key_state = false;
						break;
					case KEY_RIGHT:
						right_key_state = false;
						break;
				}
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case KEY_ROTATE_CW:
						if (!rotatecw_key_state)
						{
							rotatecw_key_state = true;
							if (GS_PLAYING == gamestate)
							{
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
										}
									}
								}
								screenFlagUpdate(true);
							}
							if (GS_MAINMENU == gamestate)
							{
								int vol = Mix_VolumeMusic(-1);
								vol += 10;
								Mix_VolumeMusic(vol);
							}
						}
						break;
					case KEY_ROTATE_CCW:
						if (!rotateccw_key_state)
						{
							rotateccw_key_state = true;
							if (GS_PLAYING == gamestate)
							{
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
										}
									}
								}
								screenFlagUpdate(true);
							}
							if(GS_MAINMENU == gamestate)
							{
								int vol = Mix_VolumeMusic(-1);
								vol -= 10;
								if (vol < 0)
									vol = 0;
								Mix_VolumeMusic(vol);
							}
						}
						break;
					case KEY_SOFTDROP:
						if (!softdrop_key_state)
						{
							softdrop_key_state = true;
							softdrop_pressed = true;
						}
						break;
					case KEY_HARDDROP:
						if (!harddrop_key_state)
						{
							harddrop_key_state = true;
							if (GS_PLAYING == gamestate)
							{
								dropHard();
							}
						}
						break;
					case KEY_HOLD:
						if (!hold_key_state)
						{
							hold_key_state = true;
							if (GS_PLAYING == gamestate)
							{
								holdFigure();
							}
						}
						break;
					case KEY_LEFT:
						if (GS_PLAYING == gamestate)
						{
							--f_x;
							if (isFigureColliding())
								++f_x;
							screenFlagUpdate(true);
						}
						if (GS_MAINMENU == gamestate)
						{
							if (!left_key_state)
							{
								left_key_state = true;
								selectPreviousTrack();
								Mix_PlayMusic(music[current_track], 1);
							}
						}
						break;
					case KEY_RIGHT:
						if (GS_PLAYING == gamestate)
						{
							++f_x;
							if (isFigureColliding())
								--f_x;
							screenFlagUpdate(true);
						}
						if (GS_MAINMENU == gamestate)
						{
							if (!right_key_state)
							{
								right_key_state = true;
								selectNextTrack();
								Mix_PlayMusic(music[current_track], 1);
							}
						}
						break;
					case KEY_PAUSE:
						if (!pause_key_state)
						{
							pause_key_state = true;
							if (GS_PLAYING == gamestate)
								gamestate = GS_MAINMENU;
							else if (GS_MAINMENU == gamestate)
								gamestate = GS_PLAYING;
							screenFlagUpdate(true);
						}
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

enum FigureId getNextId(void)
{
	switch (randomalgo)
	{
		case RA_NAIVE:
			return getRandomId();
		case RA_7BAG:
			return getRandom7BagId();
		case RA_8BAG:
			return getRandom8BagId();
		default:
			// should never happen
			return FIGID_I;
	}
}

enum FigureId getRandomId(void)
{
	return rand() % FIGID_END;
}

enum FigureId getRandom7BagId(void)
{
	static enum FigureId tab[FIGID_END];
	static int pos = FIGID_END;

	if (pos >= FIGID_END)
	{
		pos = 0;
		for (int i = 0; i < FIGID_END; ++i)
			tab[i] = i;

		for (int i = 0; i < 1000; ++i)
		{
			int one = rand() % FIGID_END;
			int two = rand() % FIGID_END;
			int temp = tab[one];
			tab[one] = tab[two];
			tab[two] = temp;
		}
	}

	return tab[pos++];
}

enum FigureId getRandom8BagId(void)
{
	static enum FigureId tab[MAX8BAGID];
	static int pos = MAX8BAGID;

	if (pos >= MAX8BAGID)
	{
		pos = 0;
		for (int i = 0; i < FIGID_END; ++i)
			tab[i] = i;
		for (int i = FIGID_END; i < MAX8BAGID; ++i)
			tab[i] = rand() % FIGID_END;

		for (int i = 0; i < 1000; ++i)
		{
			int one = rand() % MAX8BAGID;
			int two = rand() % MAX8BAGID;
			int temp = tab[one];
			tab[one] = tab[two];
			tab[two] = temp;
		}
	}

	return tab[pos++];
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
		f_shape.blockmap[i] = temp[(FIG_DIM-1-(i % FIG_DIM))*FIG_DIM + (i / FIG_DIM)];	// check this out :)
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
