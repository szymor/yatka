#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>
#include <SDL/SDL_ttf.h>
#include <SDL/SDL_mixer.h>

#define SCREEN_WIDTH		320
#define SCREEN_HEIGHT		240
#define SCREEN_BPP			32
#define FPS					60.0

#ifdef _BITTBOY
#define KEY_LEFT			SDLK_LEFT
#define KEY_RIGHT			SDLK_RIGHT
#define KEY_SOFTDROP		SDLK_DOWN
#define KEY_HARDDROP		SDLK_LCTRL
#define KEY_ROTATE_CW		SDLK_SPACE
#define KEY_ROTATE_CCW		SDLK_LALT
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

#define BOARD_X				100
#define BOARD_Y				0
#define BOARD_WIDTH			10
#define BOARD_HEIGHT		20
#define FIG_DIM				4
#define BLOCK_SIZE			12
#define FIG_NUM				7		// including the active figure

#define KEY_REPEAT_RATE		80		// in ms
#define FONT_SIZE			7
#define MUSIC_TRACK_NUM		4
#define MUSIC_FADE_TIME		3000

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

enum FigureId
{
	FIGID_I,
	FIGID_O,
	FIGID_T,
	FIGID_S,
	FIGID_Z,
	FIGID_J,
	FIGID_L,
	FIGID_NUM
};

enum RandomAlgo
{
	RA_NAIVE,
	RA_7BAG,
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
SDL_Surface *colors[FIGID_NUM];

TTF_Font *arcade_font = NULL;

Mix_Music *music[MUSIC_TRACK_NUM];
int current_track = 0;

enum FigureId *board = NULL;
struct Figure *figures[FIG_NUM];
int f_x, f_y;			// coordinates of the active figure
struct Shape f_shape;	// shape of the active figure
int statistics[FIG_NUM];

bool nosound = false, randomcolors = false, holdoff = false, grayblocks = false;
int screenscale = 1;
int startlevel = 0;
int nextblocks = FIG_NUM - 1;
enum RandomAlgo randomalgo = RA_7BAG;
bool pause = false, gameover = false, hold_ready = true, fast_drop = false;

const int fast_drop_rate = KEY_REPEAT_RATE;
const int drop_delay_per_level[] = {500, 425, 360, 307, 261, 222, 189, 160, 136, 116};
int fps, lines = 0, score = 0, level = 0;
int next_time;

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

void fillAudio(void *userdata, Uint8 *stream, int len);

void displayBoard(void);
void dropSoft(void);
void dropHard(void);
void lockFigure(void);
void holdFigure(void);
void checkFullLines(void);
void handleInput(void);
bool checkGameEnd(void);
void drawBar(int x, int y, int value);
void drawStatus(int x, int y);

void initFigures(void);
void drawFigure(const struct Figure *fig, int x, int y, bool screendim);
void spawnFigure(void);
struct Figure *getNextFigure(void);
const struct Shape* getShape(enum FigureId id);
enum FigureId getNextId(void);
enum FigureId getRandomId(void);
enum FigureId getRandom7BagId(void);
enum FigureId getNextColor(enum FigureId id);
enum FigureId getRandomColor(void);
void getShapeDimensions(const struct Shape *shape, int *minx, int *maxx, int *miny, int *maxy);
bool isFigureColliding(void);
void rotateFigureCW(void);
void rotateFigureCCW(void);

bool screenFlagUpdate(bool v);
void upscale2(uint32_t *to, uint32_t *from);
void upscale3(uint32_t *to, uint32_t *from);
void upscale4(uint32_t *to, uint32_t *from);
void frameCounter(void);
int frameLimiter(void);

int main(int argc, char *argv[])
{
	for (int i = 1; i < argc; ++i)
	{
		if (!strcmp(argv[i],"--nosound"))
			nosound = true;
		else if (!strcmp(argv[i],"--randomcolors"))
			randomcolors = true;
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
		else if (!strcmp(argv[i],"--naiverng"))
			randomalgo = RA_NAIVE;
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
		else
			printf("Unrecognized parameter: %s\n", argv[i]);
	}

	initialize();
	next_time = SDL_GetTicks() + drop_delay_per_level[level];
	while (1)
	{
		handleInput();
		displayBoard();
		frameLimiter();

		if (SDL_GetTicks() > next_time)
		{
			if (!(pause || gameover))
				dropSoft();
		}
	}
	return 0;
}

bool screenFlagUpdate(bool v)
{
	static bool value = true;
	bool old = value;
	value = v;
	return old;
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

	bg = IMG_Load("gfx/bg.png");
	if (bg == NULL)
		exit(ERROR_NOIMGFILE);
	gray = IMG_Load("gfx/block.png");
	if (gray == NULL)
		exit(ERROR_NOIMGFILE);

	SDL_Surface *mask = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);

	colors[FIGID_I] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_I], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_I]->format, 215, 64, 0, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_I], NULL);

	colors[FIGID_T] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_T], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_T]->format, 115, 121, 0, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_T], NULL);

	colors[FIGID_O] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_O], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_O]->format, 59, 52, 255, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_O], NULL);

	colors[FIGID_S] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_S], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_S]->format, 0, 132, 96, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_S], NULL);

	colors[FIGID_Z] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_Z], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_Z]->format, 75, 160, 255, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_Z], NULL);

	colors[FIGID_J] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_J], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_J]->format, 255, 174, 10, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_J], NULL);

	colors[FIGID_L] = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, colors[FIGID_L], NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(colors[FIGID_L]->format, 255, 109, 247, 128));
	SDL_BlitSurface(mask, NULL, colors[FIGID_L], NULL);

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
		current_track = 0;
		Mix_FadeInMusic(music[current_track], -1, MUSIC_FADE_TIME);
	}
	SDL_EnableKeyRepeat(1, KEY_REPEAT_RATE);

	board = malloc(sizeof(int)*BOARD_WIDTH*BOARD_HEIGHT);
	for (int i = 0; i < (BOARD_WIDTH*BOARD_HEIGHT); ++i)
		board[i] = FIGID_NUM;
	srand((unsigned)time(NULL));
	pause = false;
	gameover = false;
	lines = 0;
	level = startlevel;
	initFigures();
}

void finalize(void)
{
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

void drawFigure(const struct Figure *fig, int x, int y, bool screendim)
{
	if (fig != NULL)
	{
		const struct Shape *shape = NULL;
		int minx, maxx, miny, maxy, w, h, offset_x, offset_y;

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
					SDL_BlitSurface(colors[fig->colorid], NULL, screen, &rect);
			}
			else
			{
				rect.x = (i % FIG_DIM + x) * BLOCK_SIZE + BOARD_X;
				rect.y = (i / FIG_DIM + y) * BLOCK_SIZE + BOARD_Y;
				if (f_shape.blockmap[i] == 1)
					SDL_BlitSurface(colors[figures[0]->colorid], NULL, screen, &rect);
			}
		}
	}
}

void displayBoard(void)
{
	if (!screenFlagUpdate(false))
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
		drawFigure(figures[i], 246, 22 + 30 * (i - 1), true);
	}

	// display statistics
	for (int i = 0; i < FIG_NUM; ++i)
	{
		drawBar(64, 38 + i * 30, statistics[i]);
	}
	
	drawStatus(0, 0);

	// display board
	for (int i = 0; i < (BOARD_WIDTH*BOARD_HEIGHT); ++i)
	{
		rect.x = (i % BOARD_WIDTH) * BLOCK_SIZE + BOARD_X;
		rect.y = (i / BOARD_WIDTH) * BLOCK_SIZE + BOARD_Y;
		if (board[i] < FIGID_NUM)
			if (grayblocks)
				SDL_BlitSurface(gray, NULL, screen, &rect);
			else
				SDL_BlitSurface(colors[board[i]], NULL, screen, &rect);
	}
	drawFigure(figures[0], f_x, f_y, false);

	if(pause || gameover)
	{
		mask = SDL_CreateRGBSurface(SDL_SRCALPHA, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
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

void drawBar(int x, int y, int value)
{
	const int maxw = 26;
	const int maxh = 8;
	const int alpha_step = 64;

	SDL_Surface *bar = SDL_CreateRGBSurface(SDL_SRCALPHA,
											SCREEN_WIDTH,
											SCREEN_HEIGHT,
											SCREEN_BPP,
											0x000000ff,
											0x0000ff00,
											0x00ff0000,
											0xff000000);

	Uint8 alpha_r;
	Uint8 alpha_l;
	Uint32 col;
	SDL_Rect rect;
	int ar = (value / (maxw + 1)) * alpha_step;
	int al = ar + alpha_step;
	ar = ar > 255 ? 255 : ar;
	al = al > 255 ? 255 : al;
	alpha_l = (Uint8)al;
	alpha_r = (Uint8)ar;

	rect.x = x;
	rect.y = y;
	rect.w = value % (maxw + 1);
	rect.h = maxh;
	col = SDL_MapRGBA(bar->format, 255, 255, 255, alpha_l);
	SDL_FillRect(bar, &rect, col);

	rect.x = x + rect.w;
	rect.y = y;
	rect.w = maxw - rect.w;
	rect.h = maxh;
	col = SDL_MapRGBA(bar->format, 255, 255, 255, alpha_r);
	SDL_FillRect(bar, &rect, col);

	SDL_BlitSurface(bar, NULL, screen, NULL);
	SDL_FreeSurface(bar);
}

void drawFps(int x, int y)
{
	SDL_Color col = {.r = 255, .g = 255, .b = 255};
	SDL_Surface *text = TTF_RenderUTF8_Blended(arcade_font, "fps", col);
	SDL_BlitSurface(text, NULL, screen, NULL);
	SDL_FreeSurface(text);
}

void drawStatus(int x, int y)
{
	SDL_Color col = {.r = 255, .g = 255, .b = 255};
	SDL_Surface *text = NULL;
	SDL_Rect rect = {.x = x, .y = y};
	char buff[256];

	sprintf(buff, "Score: %d", score);
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	SDL_BlitSurface(text, NULL, screen, &rect);

	rect.y += FONT_SIZE;
	sprintf(buff, "Level: %d", level);
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	SDL_BlitSurface(text, NULL, screen, &rect);

	rect.y += FONT_SIZE;
	sprintf(buff, "Lines: %d", lines);
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	SDL_BlitSurface(text, NULL, screen, &rect);

	rect.y += FONT_SIZE;
	sprintf(buff, "FPS: %d", fps);
	text = TTF_RenderUTF8_Blended(arcade_font, buff, col);
	SDL_BlitSurface(text, NULL, screen, &rect);

	SDL_FreeSurface(text);
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
			lockFigure();
			free(figures[0]);
			figures[0] = NULL;
			fast_drop = false;
		}
	}

	checkFullLines();
	gameover = checkGameEnd();
	if (gameover)
		Mix_FadeOutMusic(MUSIC_FADE_TIME);

	if (fast_drop)
		next_time = SDL_GetTicks() + fast_drop_rate;
	else
		next_time = SDL_GetTicks() + drop_delay_per_level[level];
	screenFlagUpdate(true);
}

void dropHard(void)
{
	if (figures[0] != NULL)
	{
		while (!isFigureColliding())
			++f_y;
		--f_y;
		lockFigure();
		free(figures[0]);
		figures[0] = NULL;
		
		screenFlagUpdate(true);
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

void checkFullLines(void)
{
	int lines_once = 0;
	
	// checking and removing full lines
	for (int y = 1; y < BOARD_HEIGHT; ++y)
	{
		bool flag = true;
		for (int x = 0; x < BOARD_WIDTH; ++x)
		{
			if (board[y*BOARD_WIDTH + x] == FIGID_NUM)
			{
				flag = false;
				break;
			}
		}
		// removing
		if (flag)
		{
			++lines;
			++lines_once;
			for (int ys = y-1; ys >= 0; --ys)
				for (int x = 0; x < BOARD_WIDTH; ++x)
					board[(ys+1)*BOARD_WIDTH + x] = board[ys*BOARD_WIDTH + x];
		}
	}
	
	switch (lines_once)
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
	
	level = startlevel + lines / 30;
	if (level >= sizeof(drop_delay_per_level) / sizeof(int))
		level = sizeof(drop_delay_per_level) / sizeof(int) - 1;
}

void handleInput(void)
{
	static bool rotatecw_key_state = false;
	static bool rotateccw_key_state = false;
	static bool pause_key_state = false;
	static bool harddrop_key_state = false;
	static bool softdrop_key_state = false;
	static bool hold_key_state = false;
	static bool left_key_state = false;
	static bool right_key_state = false;
	SDL_Event event;

	if(SDL_PollEvent(&event))
		switch(event.type)
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
					case KEY_HARDDROP:
						harddrop_key_state = false;
						break;
					case KEY_SOFTDROP:
						fast_drop = false;
						softdrop_key_state = false;
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
							if(!pause && !gameover)
							{
								rotateFigureCW();
								if(isFigureColliding())
								{
									++f_x;
									if(isFigureColliding())
									{
										f_x -= 2;
										if(isFigureColliding())
										{
											rotateFigureCCW();
											++f_x;
										}
									}
								}
								screenFlagUpdate(true);
							}
							if(pause && !gameover)
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
							if (!pause && !gameover)
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
							if(pause && !gameover)
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
							if (!pause && !gameover)
							{
								fast_drop = true;
								dropSoft();
							}
						}
						break;
					case KEY_HARDDROP:
						if (!harddrop_key_state)
						{
							harddrop_key_state = true;
							if (!pause && !gameover)
							{
								dropHard();
							}
						}
						break;
					case KEY_HOLD:
						if (!hold_key_state)
						{
							hold_key_state = true;
							if (!pause && !gameover)
							{
								holdFigure();
							}
						}
						break;
					case KEY_LEFT:
						if (!pause && !gameover)
						{
							--f_x;
							if (isFigureColliding())
								++f_x;
							screenFlagUpdate(true);
						}
						if (pause && !gameover)
						{
							if (!left_key_state)
							{
								left_key_state = true;
								current_track = (current_track + MUSIC_TRACK_NUM - 1) % MUSIC_TRACK_NUM;
								Mix_PlayMusic(music[current_track], -1);
							}
						}
						break;
					case KEY_RIGHT:
						if (!pause && !gameover)
						{
							++f_x;
							if (isFigureColliding())
								--f_x;
							screenFlagUpdate(true);
						}
						if (pause && !gameover)
						{
							if (!right_key_state)
							{
								right_key_state = true;
								current_track = (current_track + 1) % MUSIC_TRACK_NUM;
								Mix_PlayMusic(music[current_track], -1);
							}
						}
						break;
					case KEY_PAUSE:
						if (!pause_key_state)
						{
							pause_key_state = true;
							pause = !pause;
							if (!gameover)
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
		if (board[i] != FIGID_NUM)
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
		default:
			// should never happen
			return FIGID_I;
	}
}

enum FigureId getRandomId(void)
{
	return rand() % FIGID_NUM;
}

enum FigureId getRandom7BagId(void)
{
	static enum FigureId tab[FIGID_NUM];
	static int pos = FIGID_NUM;
	
	if (pos >= FIGID_NUM)
	{
		pos = 0;
		for (int i = 0; i < FIGID_NUM; ++i)
			tab[i] = i;
		
		for (int i = 0; i < 1000; ++i)
		{
			int one = rand() % FIGID_NUM;
			int two = rand() % FIGID_NUM;
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
	return rand() % FIGID_NUM;
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
					if ((board[y*BOARD_WIDTH + x] != FIGID_NUM) && (f_shape.blockmap[i] != FIGID_NUM))
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

void upscale2(uint32_t *to, uint32_t *from)
{
	switch (SCREEN_BPP)
	{
		case 16:
		{
			uint16_t *from16 = (uint16_t *)from;
			int j;

			for (j = 0; j < SCREEN_HEIGHT; ++j)
			{
				int i;
				int fromWidth = SCREEN_WIDTH/4;

				for (i = 0; i < fromWidth; ++i)
				{
					/* Unroll the loop for instruction pipelining. */
					uint32_t tmp = *from16++;
					uint32_t tmp2 = *from16++;
					uint32_t tmp3 = *from16++;
					uint32_t tmp4 = *from16++;

					tmp = (tmp << 16) | tmp;
					*(to + SCREEN_WIDTH) = tmp;
					*to++ = tmp;

					tmp2 = (tmp2 << 16) | tmp2;
					*(to + SCREEN_WIDTH) = tmp2;
					*to++ = tmp2;

					tmp3 = (tmp3 << 16) | tmp3;
					*(to + SCREEN_WIDTH) = tmp3;
					*to++ = tmp3;

					tmp4 = (tmp4 << 16) | tmp4;
					*(to + SCREEN_WIDTH) = tmp4;
					*to++ = tmp4;
				}

				to += SCREEN_WIDTH;
			}
		}
		break;
		case 32:
		{
			int j;
			int fromWidth = SCREEN_WIDTH/4;
			int toWidth = SCREEN_WIDTH*2;

			for (j = 0; j < SCREEN_HEIGHT; ++j)
			{
				int i;

				for (i = 0; i < fromWidth; ++i)
				{
					/* Unroll the loop for instruction pipelining. */
					uint32_t tmp = *from++;
					uint32_t tmp2 = *from++;
					uint32_t tmp3 = *from++;
					uint32_t tmp4 = *from++;

					*to = tmp;
					*(to++ + toWidth) = tmp;
					*to = tmp;
					*(to++ + toWidth) = tmp;

					*to = tmp2;
					*(to++ + toWidth) = tmp2;
					*to = tmp2;
					*(to++ + toWidth) = tmp2;

					*to = tmp3;
					*(to++ + toWidth) = tmp3;
					*to = tmp3;
					*(to++ + toWidth) = tmp3;

					*to = tmp4;
					*(to++ + toWidth) = tmp4;
					*to = tmp4;
					*(to++ + toWidth) = tmp4;
				}

				to += toWidth;
			}
		}
		break;

		default:
		break;
	}
}

void upscale3(uint32_t *to, uint32_t *from)
{
	switch (SCREEN_BPP)
	{
		case 16:
		{
			uint16_t *from16 = (uint16_t *)from;
			int j;

			for (j = 0; j < SCREEN_HEIGHT; ++j)
			{
				int i;
				int fromWidth = SCREEN_WIDTH/4;
				int toWidth = SCREEN_WIDTH + SCREEN_WIDTH/2;

				for (i = 0; i < fromWidth; ++i)
				{
					/* Unroll the loop for instruction pipelining. */
					uint32_t tmp = *from16++;
					uint32_t tmp2 = *from16++;
					uint32_t tmp3 = *from16++;
					uint32_t tmp4 = *from16++;

					tmp = (tmp << 16) | tmp;
					*(to + toWidth + toWidth) = tmp;
					*(to + toWidth) = tmp;
					*to++ = tmp;

#if defined(PLATFORM_BIG_ENDIAN)
					tmp = tmp2 | (tmp & 0xffff0000);
#else
					tmp = (tmp2 << 16) | (tmp & 0xffff);
#endif
					*(to + toWidth + toWidth) = tmp;
					*(to + toWidth) = tmp;
					*to++ = tmp;

					tmp2 = (tmp2 << 16) | tmp2;
					*(to + toWidth + toWidth) = tmp2;
					*(to + toWidth) = tmp2;
					*to++ = tmp2;

					tmp3 = (tmp3 << 16) | tmp3;
					*(to + toWidth + toWidth) = tmp3;
					*(to + toWidth) = tmp3;
					*to++ = tmp3;

#if defined(PLATFORM_BIG_ENDIAN)
					tmp3 = tmp4 | (tmp3 & 0xffff0000);
#else
					tmp3 = (tmp4 << 16) | (tmp3 & 0xffff);
#endif
					*(to + toWidth + toWidth) = tmp3;
					*(to + toWidth) = tmp3;
					*to++ = tmp3;

					tmp4 = (tmp4 << 16) | tmp4;
					*(to + toWidth + toWidth) = tmp4;
					*(to + toWidth) = tmp4;
					*to++ = tmp4;
				}

				to += toWidth * 2;
			}
		}
		break;
		case 32:
		{
			int j;

			for (j = 0; j < SCREEN_HEIGHT; ++j)
			{
				int i;
				int fromWidth = SCREEN_WIDTH/4;
				int toWidth = SCREEN_WIDTH*3;

				for (i = 0; i < fromWidth; ++i)
				{
					/* Unroll the loop for instruction pipelining. */
					uint32_t tmp = *from++;
					uint32_t tmp2 = *from++;
					uint32_t tmp3 = *from++;
					uint32_t tmp4 = *from++;

					*(to + toWidth + toWidth) = tmp;
					*(to + toWidth) = tmp;
					*to++ = tmp;
					*(to + toWidth + toWidth) = tmp;
					*(to + toWidth) = tmp;
					*to++ = tmp;
					*(to + toWidth + toWidth) = tmp;
					*(to + toWidth) = tmp;
					*to++ = tmp;

					*(to + toWidth + toWidth) = tmp2;
					*(to + toWidth) = tmp2;
					*to++ = tmp2;
					*(to + toWidth + toWidth) = tmp2;
					*(to + toWidth) = tmp2;
					*to++ = tmp2;
					*(to + toWidth + toWidth) = tmp2;
					*(to + toWidth) = tmp2;
					*to++ = tmp2;

					*(to + toWidth + toWidth) = tmp3;
					*(to + toWidth) = tmp3;
					*to++ = tmp3;
					*(to + toWidth + toWidth) = tmp3;
					*(to + toWidth) = tmp3;
					*to++ = tmp3;
					*(to + toWidth + toWidth) = tmp3;
					*(to + toWidth) = tmp3;
					*to++ = tmp3;

					*(to + toWidth + toWidth) = tmp4;
					*(to + toWidth) = tmp4;
					*to++ = tmp4;
					*(to + toWidth + toWidth) = tmp4;
					*(to + toWidth) = tmp4;
					*to++ = tmp4;
					*(to + toWidth + toWidth) = tmp4;
					*(to + toWidth) = tmp4;
					*to++ = tmp4;
				}

				to += toWidth * 2;
			}
		}
		break;

		default:
		break;
	}
}

void upscale4(uint32_t *to, uint32_t *from)
{
	switch (SCREEN_BPP)
	{
		case 16:
		{
			uint16_t *from16 = (uint16_t *)from;
			int j;

			for (j = 0; j < SCREEN_HEIGHT; ++j)
			{
				int i;
				int fromWidth = SCREEN_WIDTH/4;
				int toWidth = SCREEN_WIDTH*2;

				for (i = 0; i < fromWidth; ++i)
				{
					/* Unroll the loop for instruction pipelining. */
					uint32_t tmp = *from16++;
					uint32_t tmp2 = *from16++;
					uint32_t tmp3 = *from16++;
					uint32_t tmp4 = *from16++;

					tmp = (tmp << 16) | tmp;
					*(to + toWidth + toWidth + toWidth) = tmp;
					*(to + toWidth + toWidth) = tmp;
					*(to + toWidth) = tmp;
					*to++ = tmp;
					*(to + toWidth + toWidth + toWidth) = tmp;
					*(to + toWidth + toWidth) = tmp;
					*(to + toWidth) = tmp;
					*to++ = tmp;

					tmp2 = (tmp2 << 16) | tmp2;
					*(to + toWidth + toWidth + toWidth) = tmp2;
					*(to + toWidth + toWidth) = tmp2;
					*(to + toWidth) = tmp2;
					*to++ = tmp2;
					*(to + toWidth + toWidth + toWidth) = tmp2;
					*(to + toWidth + toWidth) = tmp2;
					*(to + toWidth) = tmp2;
					*to++ = tmp2;

					tmp3 = (tmp3 << 16) | tmp3;
					*(to + toWidth + toWidth + toWidth) = tmp3;
					*(to + toWidth + toWidth) = tmp3;
					*(to + toWidth) = tmp3;
					*to++ = tmp3;
					*(to + toWidth + toWidth + toWidth) = tmp3;
					*(to + toWidth + toWidth) = tmp3;
					*(to + toWidth) = tmp3;
					*to++ = tmp3;

					tmp4 = (tmp4 << 16) | tmp4;
					*(to + toWidth + toWidth + toWidth) = tmp4;
					*(to + toWidth + toWidth) = tmp4;
					*(to + toWidth) = tmp4;
					*to++ = tmp4;
					*(to + toWidth + toWidth + toWidth) = tmp4;
					*(to + toWidth + toWidth) = tmp4;
					*(to + toWidth) = tmp4;
					*to++ = tmp4;
				}

				to += toWidth * 3;
			}
		}
		break;
		case 32:
		{
			int j;

			for (j = 0; j < SCREEN_HEIGHT; ++j)
			{
				int i;
				int fromWidth = SCREEN_WIDTH/4;
				int toWidth = SCREEN_WIDTH*4;

				for (i = 0; i < fromWidth; ++i)
				{
					/* Unroll the loop for instruction pipelining. */
					uint32_t tmp = *from++;
					uint32_t tmp2 = *from++;
					uint32_t tmp3 = *from++;
					uint32_t tmp4 = *from++;

					*(to + toWidth + toWidth + toWidth) = tmp;
					*(to + toWidth + toWidth) = tmp;
					*(to + toWidth) = tmp;
					*to++ = tmp;
					*(to + toWidth + toWidth + toWidth) = tmp;
					*(to + toWidth + toWidth) = tmp;
					*(to + toWidth) = tmp;
					*to++ = tmp;
					*(to + toWidth + toWidth + toWidth) = tmp;
					*(to + toWidth + toWidth) = tmp;
					*(to + toWidth) = tmp;
					*to++ = tmp;
					*(to + toWidth + toWidth + toWidth) = tmp;
					*(to + toWidth + toWidth) = tmp;
					*(to + toWidth) = tmp;
					*to++ = tmp;

					*(to + toWidth + toWidth + toWidth) = tmp2;
					*(to + toWidth + toWidth) = tmp2;
					*(to + toWidth) = tmp2;
					*to++ = tmp2;
					*(to + toWidth + toWidth + toWidth) = tmp2;
					*(to + toWidth + toWidth) = tmp2;
					*(to + toWidth) = tmp2;
					*to++ = tmp2;
					*(to + toWidth + toWidth + toWidth) = tmp2;
					*(to + toWidth + toWidth) = tmp2;
					*(to + toWidth) = tmp2;
					*to++ = tmp2;
					*(to + toWidth + toWidth + toWidth) = tmp2;
					*(to + toWidth + toWidth) = tmp2;
					*(to + toWidth) = tmp2;
					*to++ = tmp2;

					*(to + toWidth + toWidth + toWidth) = tmp3;
					*(to + toWidth + toWidth) = tmp3;
					*(to + toWidth) = tmp3;
					*to++ = tmp3;
					*(to + toWidth + toWidth + toWidth) = tmp3;
					*(to + toWidth + toWidth) = tmp3;
					*(to + toWidth) = tmp3;
					*to++ = tmp3;
					*(to + toWidth + toWidth + toWidth) = tmp3;
					*(to + toWidth + toWidth) = tmp3;
					*(to + toWidth) = tmp3;
					*to++ = tmp3;
					*(to + toWidth + toWidth + toWidth) = tmp3;
					*(to + toWidth + toWidth) = tmp3;
					*(to + toWidth) = tmp3;
					*to++ = tmp3;

					*(to + toWidth + toWidth + toWidth) = tmp4;
					*(to + toWidth + toWidth) = tmp4;
					*(to + toWidth) = tmp4;
					*to++ = tmp4;
					*(to + toWidth + toWidth + toWidth) = tmp4;
					*(to + toWidth + toWidth) = tmp4;
					*(to + toWidth) = tmp4;
					*to++ = tmp4;
					*(to + toWidth + toWidth + toWidth) = tmp4;
					*(to + toWidth + toWidth) = tmp4;
					*(to + toWidth) = tmp4;
					*to++ = tmp4;
					*(to + toWidth + toWidth + toWidth) = tmp4;
					*(to + toWidth + toWidth) = tmp4;
					*(to + toWidth) = tmp4;
					*to++ = tmp4;
				}

				to += toWidth * 3;
			}
		}
		break;

		default:
		break;
	}
}

void frameCounter(void)
{
	static unsigned int frames;
	static Uint32 curTicks;
	static Uint32 lastTicks;
	Uint32 t;

	curTicks = SDL_GetTicks();
	t = curTicks - lastTicks;

	if (t >= 1000)
	{
		lastTicks = curTicks;
		fps = frames;
		frames = 0;
	}

	++frames;
}

int frameLimiter(void)
{
	static Uint32 curTicks;
	static Uint32 lastTicks;
	float t;

#if NO_FRAMELIMIT
	return 0;
#endif

	curTicks = SDL_GetTicks();
	t = curTicks - lastTicks;

	if (t >= 1000.0/FPS)
	{
		lastTicks = curTicks;
		return 0;
	}


	SDL_Delay(1);

	return 1;
}
