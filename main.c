#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#define SCREEN_WIDTH		320
#define SCREEN_HEIGHT		240
#define SCREEN_BPP			32

#define KEY_LEFT			SDLK_LEFT
#define KEY_RIGHT			SDLK_RIGHT
#define KEY_SOFTDROP		SDLK_DOWN
#define KEY_HARDDROP		SDLK_SPACE
#define KEY_ROTATE			SDLK_UP
#define KEY_PAUSE			SDLK_ESCAPE

#define BOARD_X				100
#define BOARD_Y				0
#define BOARD_WIDTH			10
#define BOARD_HEIGHT		20
#define FIG_DIM				4
#define BLOCK_SIZE			12
#define FIG_NUM				7		// including the active figure

#define KEY_REPEAT_RATE		80		// in ms

enum Error
{
	ERROR_NONE,
	ERROR_SDLINIT,
	ERROR_SDLVIDEO,
	ERROR_NOIMGFILE,
	ERROR_NOSNDFILE,
	ERROR_OPENAUDIO,
	ERROR_END
};

enum FigureId
{
	FIGID_O,
	FIGID_L,
	FIGID_J,
	FIGID_S,
	FIGID_Z,
	FIGID_I,
	FIGID_T,
	FIGID_NUM
};

struct Figure
{
	enum FigureId id;
	SDL_Surface *color;
};

SDL_Surface *screen = NULL;
SDL_Surface *screen_scaled = NULL;
SDL_Surface *bg = NULL;
SDL_Surface *fg = NULL;

// sprites for blocks
SDL_Surface *gray = NULL;
SDL_Surface *cyan = NULL;
SDL_Surface *purple = NULL;
SDL_Surface *yellow = NULL;
SDL_Surface *green = NULL;
SDL_Surface *red = NULL;
SDL_Surface *blue = NULL;
SDL_Surface *orange = NULL;

SDL_AudioSpec audiospec;
Uint8 *bgmusic = NULL;
Uint8 *bgmusic_sample = NULL;
Uint32 bgmusic_length = 0;
Uint32 bgmusic_tmplen = 0;
Uint8 *gosound = NULL;
Uint8 *gosound_sample = NULL;
Uint32 gosound_length = 0;
Uint32 gosound_tmplen = 0;

int *board = NULL;
struct Figure *figures[FIG_NUM];
SDL_Surface *fixed_colors[FIGID_NUM];
int f_x, f_y;	// coordinates of the active figure
int rotation;	// ...of the active figure

bool pause, gameover, nosound, randomcolor;
int screenscale;
int lines;
int next_time, delay = 500;

// shapes - 4x4 figure size
const int shape_O[]		= { 0, 0, 0, 0,
							0, 1, 1, 0,
							0, 1, 1, 0,
							0, 0, 0, 0 };
const int shape_L[]		= { 0, 0, 0, 0,
							0, 0, 1, 0,
							1, 1, 1, 0,
							0, 0, 0, 0 };
const int shape_J[]		= { 0, 0, 0, 0,
							1, 0, 0, 0,
							1, 1, 1, 0,
							0, 0, 0, 0 };
const int shape_S[]		= { 0, 0, 0, 0,
							0, 1, 1, 0,
							1, 1, 0, 0,
							0, 0, 0, 0 };
const int shape_Z[]		= { 0, 0, 0, 0,
							1, 1, 0, 0,
							0, 1, 1, 0,
							0, 0, 0, 0 };
const int shape_I[]		= { 0, 0, 0, 0,
							0, 0, 0, 0,
							1, 1, 1, 1,
							0, 0, 0, 0 };
const int shape_T[]		= { 0, 0, 0, 0,
							0, 1, 0, 0,
							1, 1, 1, 0,
							0, 0, 0, 0 };

void initialize(void);
void finalize(void);

void fillAudio(void *userdata, Uint8 *stream, int len);

void displayBoard(void);
void dropSoft(void);
void dropHard(void);
void checkFullLines(void);
void handleInput(void);
bool checkGameEnd(void);

void initFigures(void);
void drawFigure(void);
void spawnFigure(void);
struct Figure *getNextFigure(void);
const int* getShape(enum FigureId id);
enum FigureId getNextId(void);
enum FigureId getRandomId(void);
SDL_Surface* getNextColor(enum FigureId id);
SDL_Surface* getRandomColor(void);
bool isFigureColliding(void);
void rotateFigureClockwise(void);

bool screenFlagUpdate(bool v);
void upscale2(uint32_t *to, uint32_t *from);
void upscale3(uint32_t *to, uint32_t *from);
void upscale4(uint32_t *to, uint32_t *from);

int main(int argc, char *argv[])
{
	nosound = ((argc == 2) && (!strcmp(argv[1],"--nosound")));
	randomcolor = ((argc == 2) && (!strcmp(argv[1],"--randomcolor")));
	screenscale = ((argc == 2) && (!strcmp(argv[1],"--scale2x"))) ? 2 : 1;
	if (screenscale == 1)
		screenscale = ((argc == 2) && (!strcmp(argv[1],"--scale3x"))) ? 3 : 1;
	if (screenscale == 1)
		screenscale = ((argc == 2) && (!strcmp(argv[1],"--scale4x"))) ? 4 : 1;

	initialize();
	next_time = SDL_GetTicks() + delay;
	while(1)
	{
		handleInput();
		displayBoard();

		if(SDL_GetTicks() > next_time)
		{
			if(!(pause || gameover))
				dropSoft();
		}

		// workaround for high CPU usage
		SDL_Delay(10);
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
	atexit(finalize);
	screen_scaled = SDL_SetVideoMode(SCREEN_WIDTH * screenscale, SCREEN_HEIGHT * screenscale, SCREEN_BPP, SDL_HWSURFACE | SDL_DOUBLEBUF);
	if (screen_scaled == NULL)
		exit(ERROR_SDLVIDEO);
	screen = screenscale > 1 ? SDL_CreateRGBSurface(SDL_SWSURFACE, SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, 0, 0, 0, 0) : screen_scaled;
	SDL_WM_SetCaption("Y A T K A", NULL);
	
	bg = IMG_Load("gfx/bg.png");
	if (bg == NULL)
		exit(ERROR_NOIMGFILE);
	fg = IMG_Load("gfx/fg.png");
	if (fg == NULL)
		exit(ERROR_NOIMGFILE);
	gray = IMG_Load("gfx/block.png");
	if (gray == NULL)
		exit(ERROR_NOIMGFILE);

	SDL_Surface *mask = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);

	cyan = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, cyan, NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(cyan->format, 0, 255, 255, 128));
	SDL_BlitSurface(mask, NULL, cyan, NULL);
	
	purple = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, purple, NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(purple->format, 255, 0, 255, 128));
	SDL_BlitSurface(mask, NULL, purple, NULL);
	
	yellow = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, yellow, NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(yellow->format, 255, 255, 0, 128));
	SDL_BlitSurface(mask, NULL, yellow, NULL);
	
	green = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, green, NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(green->format, 0, 255, 0, 128));
	SDL_BlitSurface(mask, NULL, green, NULL);
	
	red = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, red, NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(red->format, 255, 0, 0, 128));
	SDL_BlitSurface(mask, NULL, red, NULL);
	
	blue = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, blue, NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(blue->format, 0, 0, 255, 128));
	SDL_BlitSurface(mask, NULL, blue, NULL);
	
	orange = SDL_CreateRGBSurface(SDL_SRCALPHA, BLOCK_SIZE, BLOCK_SIZE, SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(gray, NULL, orange, NULL);
	SDL_FillRect(mask, NULL, SDL_MapRGBA(orange->format, 255, 128, 0, 128));
	SDL_BlitSurface(mask, NULL, orange, NULL);
	
	SDL_FreeSurface(mask);
	
	fixed_colors[FIGID_O] = yellow;
	fixed_colors[FIGID_L] = orange;
	fixed_colors[FIGID_J] = blue;
	fixed_colors[FIGID_S] = green;
	fixed_colors[FIGID_Z] = red;
	fixed_colors[FIGID_I] = cyan;
	fixed_colors[FIGID_T] = purple;

	if (!nosound)
	{
		if (!SDL_LoadWAV("sfx/gameover.wav",&audiospec,&gosound,&gosound_length))
			exit(ERROR_NOSNDFILE);
		gosound_sample = gosound;
		gosound_tmplen = gosound_length;
		if (!SDL_LoadWAV("sfx/bgmusic.wav",&audiospec,&bgmusic,&bgmusic_length))
			exit(ERROR_NOSNDFILE);
		audiospec.samples = 1024;
		audiospec.callback = fillAudio;
		audiospec.userdata = NULL;
		if (SDL_OpenAudio(&audiospec,NULL) == -1)
			exit(ERROR_OPENAUDIO);
		SDL_PauseAudio(0);
	}
	SDL_EnableKeyRepeat(1, KEY_REPEAT_RATE);

	board = malloc(sizeof(int)*BOARD_WIDTH*BOARD_HEIGHT);
	for (int i = 0; i < (BOARD_WIDTH*BOARD_HEIGHT); ++i)
		board[i] = 0;
	srand((unsigned)time(NULL));
	pause = false;
	gameover = false;
	lines = 0;
	initFigures();
}

void finalize(void)
{
	if(!nosound)
	{
		if(bgmusic != NULL)
			SDL_FreeWAV(bgmusic);
		SDL_CloseAudio();
	}
	SDL_Quit();
	if(board != NULL)
		free(board);
	for (int i = 0; i < FIG_NUM; ++i)
		free(figures[i]);
}

void fillAudio(void *userdata, Uint8 *stream, int len)
{
	if(!gameover)
	{
		if(bgmusic_tmplen == 0)
		{
			bgmusic_tmplen = bgmusic_length;
			bgmusic_sample = bgmusic;
			return;
		}
		len = (len > bgmusic_tmplen) ? bgmusic_tmplen : len;
		SDL_MixAudio(stream, bgmusic_sample, len, SDL_MIX_MAXVOLUME);
		bgmusic_sample += len;
		bgmusic_tmplen -= len;
	}
	else
	{
		if(gosound_tmplen == 0)
		{
			return;
		}
		len = (len > gosound_tmplen) ? gosound_tmplen : len;
		SDL_MixAudio(stream, gosound_sample, len, SDL_MIX_MAXVOLUME);
		gosound_sample += len;
		gosound_tmplen -= len;
	}
}

// TODO - take rotation into account
void drawFigure(void)
{
	if (figures[0] != NULL)
		for (int i = 0; i < (FIG_DIM*FIG_DIM); ++i)
		{
			SDL_Rect rect;
			rect.x = (i % FIG_DIM + f_x) * BLOCK_SIZE + BOARD_X;
			rect.y = (i / FIG_DIM + f_y) * BLOCK_SIZE + BOARD_Y;
			const int *shape = getShape(figures[0]->id);
			if (shape[i] == 1)
				SDL_BlitSurface(figures[0]->color, NULL, screen, &rect);
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
	/*
	// display next figure
	for (int i = 0; i < (FIG_DIM*FIG_DIM); ++i)
	{
		rect.x = (i % FIG_DIM + BOARD_WIDTH) * BLOCK_SIZE + 50;
		rect.y = (i / FIG_DIM) * BLOCK_SIZE + 80;
		if (next_figure[i] == 1)
			SDL_BlitSurface(next_color, NULL, screen, &rect);
	}
	
	// display number of removed lines
	switch(lines % 10)
	{
		case 0:	digit = digit0; break;
		case 1:	digit = digit1; break;
		case 2:	digit = digit2; break;
		case 3:	digit = digit3; break;
		case 4:	digit = digit4; break;
		case 5:	digit = digit5; break;
		case 6:	digit = digit6; break;
		case 7:	digit = digit7; break;
		case 8:	digit = digit8; break;
		case 9:	digit = digit9; break;
	}
	rect.x = BOARD_WIDTH*BLOCK_SIZE + 115 + 16;
	rect.y = 160;
	SDL_BlitSurface(digit, NULL, screen, &rect);
	switch( (lines / 10) % 10 )
	{
		case 0:	digit = digit0; break;
		case 1:	digit = digit1; break;
		case 2:	digit = digit2; break;
		case 3:	digit = digit3; break;
		case 4:	digit = digit4; break;
		case 5:	digit = digit5; break;
		case 6:	digit = digit6; break;
		case 7:	digit = digit7; break;
		case 8:	digit = digit8; break;
		case 9:	digit = digit9; break;
	}
	rect.x = BOARD_WIDTH*BLOCK_SIZE + 115;
	rect.y = 350;
	SDL_BlitSurface(digit, NULL, screen, &rect);
*/
	// display board
	for (int i = 0; i < (BOARD_WIDTH*BOARD_HEIGHT); ++i)
	{
		rect.x = (i % BOARD_WIDTH) * BLOCK_SIZE + BOARD_X;
		rect.y = (i / BOARD_WIDTH) * BLOCK_SIZE + BOARD_Y;
		if (board[i] == 1)
			SDL_BlitSurface(gray, NULL, screen, &rect);
	}
	drawFigure();

	// display foreground
	rect.x = 0;
	rect.y = 0;
	SDL_BlitSurface(fg, NULL, screen, &rect);

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
}

// TODO - take rotation into account
void dropSoft(void)
{
	if (figures[0] == NULL)
	{
		spawnFigure();
	}
	else
	{
		const int *shape = getShape(figures[0]->id);
		
		++f_y;
		if (isFigureColliding())
		{
			--f_y;
			for (int i = 0; i < (FIG_DIM*FIG_DIM); ++i)
				if (shape[i] != 0)
				{
					int x = i % FIG_DIM + f_x;
					int y = i / FIG_DIM + f_y;
					if ((x >= 0) && (x < BOARD_WIDTH) && (y >= 0) && (y < BOARD_HEIGHT))
						board[y*BOARD_WIDTH + x] = 1;
				}
			free(figures[0]);
			figures[0] = NULL;
		}
	}

	checkFullLines();
	delay = (lines > 45) ? 50 : (500 - 10 * lines);
	gameover = checkGameEnd();

	next_time = SDL_GetTicks() + delay;
	screenFlagUpdate(true);
}

// TODO - take rotation into account
void dropHard(void)
{
	if (figures[0] != NULL)
	{
		const int *shape = getShape(figures[0]->id);
		
		while (!isFigureColliding())
			++f_y;
		--f_y;
		for (int i = 0; i < (FIG_DIM*FIG_DIM); ++i)
			if (shape[i] != 0)
			{
				int x = i % FIG_DIM + f_x;
				int y = i / FIG_DIM + f_y;
				if ((x >= 0) && (x < BOARD_WIDTH) && (y >= 0) && (y < BOARD_HEIGHT))
					board[y*BOARD_WIDTH + x] = 1;
			}
		free(figures[0]);
		figures[0] = NULL;
		
		screenFlagUpdate(true);
	}
}

void checkFullLines(void)
{
	// checking and removing full lines
	for (int y = 1; y < BOARD_HEIGHT; ++y)
	{
		int i = 1;
		for (int x = 0; x < BOARD_WIDTH; ++x)
		{
			if (board[y*BOARD_WIDTH + x] == 0)
			{
				i = 0;
				break;
			}
		}
		// removing
		if (i)
		{
			++lines;
			for (int ys = y-1; ys >= 0; --ys)
				for (int x = 0; x < BOARD_WIDTH; ++x)
					board[(ys+1)*BOARD_WIDTH + x] = board[ys*BOARD_WIDTH + x];
		}
	}
}

void handleInput(void)
{
	static bool rotate_key_state = false;
	static bool pause_key_state = false;
	static bool harddrop_key_state = false;
	SDL_Event event;

	if(SDL_PollEvent(&event))
		switch(event.type)
		{
			case SDL_KEYUP:
				switch (event.key.keysym.sym)
				{
					case KEY_ROTATE:
						rotate_key_state = false;
						break;
					case KEY_PAUSE:
						pause_key_state = false;
						break;
					case KEY_HARDDROP:
						harddrop_key_state = false;
						break;
				}
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case KEY_ROTATE:
						if (!rotate_key_state)
						{
							rotate_key_state = true;
							if(!pause && !gameover)
							{
								rotateFigureClockwise();
								if(isFigureColliding())
								{
									++f_x;
									if(isFigureColliding())
									{
										f_x -= 2;
										if(isFigureColliding())
										{
											rotateFigureClockwise();
											rotateFigureClockwise();
											rotateFigureClockwise();
											++f_x;
										}
									}
								}
							}
							screenFlagUpdate(true);
						}
						break;
					case KEY_SOFTDROP:
						if(!pause && !gameover)
						{
							dropSoft();
						}
						screenFlagUpdate(true);
						break;
					case KEY_HARDDROP:
						if (!harddrop_key_state)
						{
							harddrop_key_state = true;
							if(!pause && !gameover)
							{
								dropHard();
							}
						}
						break;
					case KEY_LEFT:
						if(!pause && !gameover)
						{
							--f_x;
							if(isFigureColliding())
								++f_x;
						}
						screenFlagUpdate(true);
						break;
					case KEY_RIGHT:
						if(!pause && !gameover)
						{
							++f_x;
							if(isFigureColliding())
								--f_x;
						}
						screenFlagUpdate(true);
						break;
					case KEY_PAUSE:
						if (!pause_key_state)
						{
							pause_key_state = true;
							pause = !pause;
							screenFlagUpdate(true);
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
		if (board[i] != 0)
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
	f_y = -FIG_DIM + 1;						// ...to gain a 'slide' effect from the top of screen
	f_x = (BOARD_WIDTH - FIG_DIM) / 2;		// ...to center a figure
	rotation = 0;
}

struct Figure *getNextFigure(void)
{
	struct Figure *f = malloc(sizeof(struct Figure));
	
	f->id = getNextId();
	f->color = getNextColor(f->id);
	
	return f;
}

const int* getShape(enum FigureId id)
{
	switch (id)
	{
		case FIGID_O:
			return shape_O;
		case FIGID_L:
			return shape_L;
		case FIGID_J:
			return shape_J;
		case FIGID_S:
			return shape_S;
		case FIGID_Z:
			return shape_Z;
		case FIGID_I:
			return shape_I;
		case FIGID_T:
			return shape_T;
	}
}

enum FigureId getNextId(void)
{
	// placeholder for different RNG implementations
	return getRandomId();
}

enum FigureId getRandomId(void)
{
	return rand() % FIGID_NUM;
}

SDL_Surface* getNextColor(enum FigureId id)
{	
	if (randomcolor)
		return getRandomColor();
	else
		return fixed_colors[id];
}

SDL_Surface* getRandomColor(void)
{
	SDL_Surface* temp;
	switch( rand() % 7 )
	{
		case 0:
			temp = blue;
			break;
		case 1:
			temp = green;
			break;
		case 2:
			temp = orange;
			break;
		case 3:
			temp = red;
			break;
		case 4:
			temp = yellow;
			break;
		case 5:
			temp = purple;
			break;
		case 6:
			temp = cyan;
			break;
	}
	return temp;
}

bool isFigureColliding(void)
{
	int i, empty_columns_right, empty_columns_left, empty_rows_down;
	if (figures[0] != NULL)
	{
		const int *shape = getShape(figures[0]->id);
		
		// counting empty columns on the righthand side of figure
		// 'x' and 'y' used as counters
		empty_columns_right = 0;
		for (int x = FIG_DIM-1; x >= 0; --x)
		{
			i = 1;		// empty by default
			for (int y = 0; y < FIG_DIM; ++y)
				if (shape[y*FIG_DIM + x] != 0)
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
				if (shape[y*FIG_DIM + x] != 0)
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
				if (shape[y*FIG_DIM + x] != 0)
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
			if (shape[i] != 0)
			{
				int x = i % FIG_DIM + f_x;
				int y = i / FIG_DIM + f_y;
				if (((y*BOARD_WIDTH + x) < (BOARD_WIDTH*BOARD_HEIGHT)) && ((y*BOARD_WIDTH + x) >= 0))
					if (board[y*BOARD_WIDTH + x] & shape[i])
						return true;
			}
	}
	return false;
}

// TODO
void rotateFigureClockwise(void)
{
	/*
	int i, empty_rows, x, y;
	int temp[FIG_DIM*FIG_DIM];

	if(fig != NULL)
	{
		for( i = 0; i < FIG_DIM*FIG_DIM; ++i )
			temp[i] = fig[i];
		for( i = 0; i < FIG_DIM*FIG_DIM; ++i )
		{
			fig[i] = temp[(FIG_DIM-1-(i % FIG_DIM))*FIG_DIM + (i / FIG_DIM)];	// check this out :)
		}
	}
	* */
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
