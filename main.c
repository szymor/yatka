#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <string.h>

#include <SDL/SDL.h>
#include <SDL/SDL_image.h>

#define KEY_LEFT		SDLK_LEFT
#define KEY_RIGHT		SDLK_RIGHT
#define KEY_SOFTDROP	SDLK_DOWN
#define KEY_HARDDROP	SDLK_SPACE
#define KEY_ROTATE		SDLK_UP
#define KEY_PAUSE		SDLK_ESCAPE

#define SCREEN_WIDTH	320
#define SCREEN_HEIGHT	240
#define SCREEN_BPP		32

#define BOARD_X			100
#define BOARD_Y			0
#define BOARD_WIDTH		10
#define BOARD_HEIGHT	20
#define FIG_DIM			4
#define BLOCK_SIZE		12

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

enum Figure
{
	FIG_O,
	FIG_L,
	FIG_J,
	FIG_S,
	FIG_Z,
	FIG_I,
	FIG_T,
	FIG_NUM
};

SDL_Surface *screen = NULL;
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

SDL_Surface *color = NULL;			// color of currently falling figure
SDL_Surface *next_color = NULL;
SDL_Surface *fixed_colors[FIG_NUM];

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
int *figure = NULL;
int *next_figure = NULL;
int next_figure_id = -1;	// temporary solution

int f_x, f_y;	// coordinates of falling figure

bool pause, gameover, nosound, randomcolor;
int lines;
int next_time, delay = 500;

// shapes - 4x4 figure size
const int square[]		= { 0, 0, 0, 0,
							0, 1, 1, 0,
							0, 1, 1, 0,
							0, 0, 0, 0 };
const int Lshape[]		= { 0, 0, 0, 0,
							0, 0, 1, 0,
							1, 1, 1, 0,
							0, 0, 0, 0 };
const int mirrorL[]		= { 0, 0, 0, 0,
							1, 0, 0, 0,
							1, 1, 1, 0,
							0, 0, 0, 0 };
const int fourshape[]	= { 0, 0, 0, 0,
							0, 1, 1, 0,
							1, 1, 0, 0,
							0, 0, 0, 0 };
const int mirrorfour[]	= { 0, 0, 0, 0,
							1, 1, 0, 0,
							0, 1, 1, 0,
							0, 0, 0, 0 };
const int stick[]		= { 0, 0, 0, 0,
							0, 0, 0, 0,
							1, 1, 1, 1,
							0, 0, 0, 0 };
const int shortT[]		= { 0, 0, 0, 0,
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
int* getRandomFigure(void);
SDL_Surface* getNextColor(void);
SDL_Surface* getRandomColor(void);
bool isFigureColliding(void);
void rotateFigureClockwise(int *fig);
bool screenFlagUpdate(bool v);

int main(int argc, char *argv[])
{
	nosound = ((argc == 2) && (!strcmp(argv[1],"--nosound")));
	randomcolor = ((argc == 2) && (!strcmp(argv[1],"--randomcolor")));;

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

void initialize(void)
{
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0)
		exit(ERROR_SDLINIT);
	atexit(finalize);
	screen = SDL_SetVideoMode(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, SDL_HWSURFACE | SDL_DOUBLEBUF);
	if(screen == NULL)
		exit(ERROR_SDLVIDEO);
	SDL_WM_SetCaption("Y A T K A", NULL);
	
	bg = IMG_Load("gfx/bg.png");
	if(bg == NULL)
		exit(ERROR_NOIMGFILE);
	fg = IMG_Load("gfx/fg.png");
	if(fg == NULL)
		exit(ERROR_NOIMGFILE);
	gray = IMG_Load("gfx/block.png");
	if(gray == NULL)
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
	
	fixed_colors[FIG_O] = yellow;
	fixed_colors[FIG_L] = orange;
	fixed_colors[FIG_J] = blue;
	fixed_colors[FIG_S] = green;
	fixed_colors[FIG_Z] = red;
	fixed_colors[FIG_I] = cyan;
	fixed_colors[FIG_T] = purple;

	if(!nosound)
	{
		if(!SDL_LoadWAV("sfx/gameover.wav",&audiospec,&gosound,&gosound_length))
			exit(ERROR_NOSNDFILE);
		gosound_sample = gosound;
		gosound_tmplen = gosound_length;
		if(!SDL_LoadWAV("sfx/bgmusic.wav",&audiospec,&bgmusic,&bgmusic_length))
			exit(ERROR_NOSNDFILE);
		audiospec.samples = 1024;
		audiospec.callback = fillAudio;
		audiospec.userdata = NULL;
		if(SDL_OpenAudio(&audiospec,NULL) == -1)
			exit(ERROR_OPENAUDIO);
		SDL_PauseAudio(0);
	}
	SDL_EnableKeyRepeat(1, KEY_REPEAT_RATE);

	board = malloc(sizeof(int)*BOARD_WIDTH*BOARD_HEIGHT);
	for(int i = 0; i < (BOARD_WIDTH*BOARD_HEIGHT); ++i)
		board[i] = 0;
	srand((unsigned)time(NULL));
	next_figure = getRandomFigure();
	next_color = getNextColor();
	pause = false;
	gameover = false;
	lines = 0;
}

void finalize(void)
{
	SDL_Quit();
	if(board != NULL)
		free(board);
	if(figure != NULL)
		free(figure);
	if(next_figure != NULL)
		free(next_figure);
	if(!nosound)
	{
		if(bgmusic != NULL)
			SDL_FreeWAV(bgmusic);
		SDL_CloseAudio();
	}
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
	if (figure != NULL)
		for (int i = 0; i < (FIG_DIM*FIG_DIM); ++i)
		{
			rect.x = (i % FIG_DIM + f_x) * BLOCK_SIZE + BOARD_X;
			rect.y = (i / FIG_DIM + f_y) * BLOCK_SIZE + BOARD_Y;
			if (figure[i] == 1)
				SDL_BlitSurface(color, NULL, screen, &rect);
		}

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

	SDL_Flip(screen);
}

void dropSoft(void)
{
	if (figure == NULL)
	{
		figure = next_figure;
		color = next_color;
		next_figure = getRandomFigure();
		next_color = getNextColor();
		f_y = -FIG_DIM + 1;						// ...to gain a 'slide' effect from the top of screen
		f_x = (BOARD_WIDTH - FIG_DIM) >> 1;		// ...to center a figure
	}
	else
	{
		++f_y;
		if (isFigureColliding())
		{
			--f_y;
			for (int i = 0; i < (FIG_DIM*FIG_DIM); ++i)
				if (figure[i] != 0)
				{
					int x = i % FIG_DIM + f_x;
					int y = i / FIG_DIM + f_y;
					if ((x >= 0) && (x < BOARD_WIDTH) && (y >= 0) && (y < BOARD_HEIGHT))
						board[y*BOARD_WIDTH + x] = 1;
				}
			free(figure);
			figure = NULL;
		}
	}

	checkFullLines();
	delay = (lines > 45) ? 50 : (500 - 10 * lines);
	gameover = checkGameEnd();

	next_time = SDL_GetTicks() + delay;
	screenFlagUpdate(true);
}

void dropHard(void)
{
	if (figure != NULL)
	{
		while (!isFigureColliding())
			++f_y;
		--f_y;
		for (int i = 0; i < (FIG_DIM*FIG_DIM); ++i)
			if (figure[i] != 0)
			{
				int x = i % FIG_DIM + f_x;
				int y = i / FIG_DIM + f_y;
				if ((x >= 0) && (x < BOARD_WIDTH) && (y >= 0) && (y < BOARD_HEIGHT))
					board[y*BOARD_WIDTH + x] = 1;
			}
		free(figure);
		figure = NULL;
		
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
								rotateFigureClockwise(figure);
								if(isFigureColliding())
								{
									++f_x;
									if(isFigureColliding())
									{
										f_x -= 2;
										if(isFigureColliding())
										{
											rotateFigureClockwise(figure);
											rotateFigureClockwise(figure);
											rotateFigureClockwise(figure);
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
							dropHard();
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

int* getRandomFigure(void)
{
	const int *temp;
	int *temp2;
	
	int id = rand() % FIG_NUM;
	switch (id)
	{
		case FIG_O:
			temp = square;
			break;
		case FIG_L:
			temp = Lshape;
			break;
		case FIG_J:
			temp = mirrorL;
			break;
		case FIG_S:
			temp = fourshape;
			break;
		case FIG_Z:
			temp = mirrorfour;
			break;
		case FIG_I:
			temp = stick;
			break;
		case FIG_T:
			temp = shortT;
			break;
	}
	
	next_figure_id = id;

	// creating a new instance of figure (simply copying ;p)
	temp2 = malloc(sizeof(int)*FIG_DIM*FIG_DIM);
	for (int i = 0; i < FIG_DIM*FIG_DIM; ++i)
		temp2[i] = temp[i];

	return temp2;
}

SDL_Surface* getNextColor(void)
{	
	if (randomcolor)
		return getRandomColor();
	else
		return fixed_colors[next_figure_id];
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
	int i, x, y, empty_columns_right, empty_columns_left, empty_rows_down;
	if( figure != NULL)
	{
		// counting empty columns on the righthand side of figure
		// 'x' and 'y' used as counters
		empty_columns_right = 0;
		for( x = FIG_DIM-1; x >= 0; --x )
		{
			i = 1;		// empty by default
			for( y = 0; y < FIG_DIM; ++y )
				if( figure[y*FIG_DIM + x] != 0 )
				{
					i = 0;
					break;
				}
			if(i)
				++empty_columns_right;
			else
				break;
		}

		// the same as above but for the lefthand side
		empty_columns_left = 0;
		for( x = 0; x < FIG_DIM; ++x )
		{
			i = 1;		// empty by default
			for( y = 0; y < FIG_DIM; ++y )
				if( figure[y*FIG_DIM + x] != 0 )
				{
					i = 0;
					break;
				}
			if(i)
				++empty_columns_left;
			else
				break;
		}

		// ...and for the bottom side
		empty_rows_down = 0;
		for( y = FIG_DIM-1; y >= 0; --y )
		{
			i = 1;		// empty by default
			for( x = 0; x < FIG_DIM; ++x )
				if( figure[y*FIG_DIM + x] != 0 )
				{
					i = 0;
					break;
				}
			if(i)
				++empty_rows_down;
			else
				break;
		}

		// proper collision checking
		if( (f_x < -empty_columns_left) || (f_x > BOARD_WIDTH-FIG_DIM+empty_columns_right) )
			return true;
		if( f_y > (BOARD_HEIGHT-FIG_DIM+empty_rows_down) )
			return true;
		for( i = 0; i < (FIG_DIM*FIG_DIM); ++i )
			if( figure[i] != 0 )
			{
				x = i % FIG_DIM + f_x;
				y = i / FIG_DIM + f_y;
				if( ((y*BOARD_WIDTH + x) < (BOARD_WIDTH*BOARD_HEIGHT)) && ((y*BOARD_WIDTH + x) >= 0) )
					if( board[y*BOARD_WIDTH + x] & figure[i] )
						return true;
			}
	}
	return false;
}

void rotateFigureClockwise(int *fig)
{
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
}
