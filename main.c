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
	ERROR_NOWAVFILE,
	ERROR_OPENAUDIO,
	ERROR_END
};

enum Collision
{
	COLLISION_NONE,
	COLLISION_DOWN,
	COLLISION_SIDE,
	COLLISION_END
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
SDL_Surface *digit0 = NULL;			// digits
SDL_Surface *digit1 = NULL;
SDL_Surface *digit2 = NULL;
SDL_Surface *digit3 = NULL;
SDL_Surface *digit4 = NULL;
SDL_Surface *digit5 = NULL;
SDL_Surface *digit6 = NULL;
SDL_Surface *digit7 = NULL;
SDL_Surface *digit8 = NULL;
SDL_Surface *digit9 = NULL;

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

int f_x, f_y;	// coordinates of falling figure

bool pause, gameover, nosound;
int lines;
int next_time, delay = 500;

// shapes - 4x4 figure size
const int square[]		= { 0, 0, 0, 0,
							0, 1, 1, 0,
							0, 1, 1, 0,
							0, 0, 0, 0 };
const int Lshape[]		= { 0, 0, 0, 0,
							0, 1, 0, 0,
							0, 1, 0, 0,
							0, 1, 1, 0 };
const int mirrorL[]		= { 0, 0, 0, 0,
							0, 0, 1, 0,
							0, 0, 1, 0,
							0, 1, 1, 0 };
const int fourshape[]	= { 0, 0, 0, 0,
							0, 1, 0, 0,
							0, 1, 1, 0,
							0, 0, 1, 0 };
const int mirrorfour[]	= { 0, 0, 0, 0,
							0, 0, 1, 0,
							0, 1, 1, 0,
							0, 1, 0, 0 };
const int stick[]		= { 0, 1, 0, 0,
							0, 1, 0, 0,
							0, 1, 0, 0,
							0, 1, 0, 0 };
const int shortT[]		= { 0, 0, 0, 0,
							1, 1, 1, 0,
							0, 1, 0, 0,
							0, 0, 0, 0 };

void Initialize(void);
void Finalize(void);

void FillAudio(void *userdata, Uint8 *stream, int len);

void DisplayBoard(void);
void MakeOneStep(void);
void CheckForFullLines(void);
void HandleInput(void);
bool CheckForGameEnd(void);
int* RandomFigure(void);
SDL_Surface* RandomColor(void);
enum Collision IsFigureColliding(void);
void RotateFigureClockwise(int *fig);
bool ScreenUpdated(bool v);

int main(int argc, char *argv[])
{
	nosound = ((argc == 2) && (!strcmp(argv[1],"--nosound")));

	Initialize();
	next_time = SDL_GetTicks() + delay;
	while(1)
	{
		HandleInput();
		DisplayBoard();

		if(SDL_GetTicks() > next_time)
		{
			if(!(pause || gameover))
				MakeOneStep();
		}

		// workaround for high CPU usage
		SDL_Delay(10);
	}
	return 0;
}

bool ScreenUpdated(bool v)
{
	static bool value = true;
	bool old = value;
	value = v;
	return old;
}

void Initialize(void)
{
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0)
		exit(ERROR_SDLINIT);
	atexit(Finalize);
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

	digit0 = SDL_LoadBMP("gfx/0.bmp");
	if(digit0 == NULL)
		exit(ERROR_NOIMGFILE);
	digit1 = SDL_LoadBMP("gfx/1.bmp");
	if(digit1 == NULL)
		exit(ERROR_NOIMGFILE);
	digit2 = SDL_LoadBMP("gfx/2.bmp");
	if(digit2 == NULL)
		exit(ERROR_NOIMGFILE);
	digit3 = SDL_LoadBMP("gfx/3.bmp");
	if(digit3 == NULL)
		exit(ERROR_NOIMGFILE);
	digit4 = SDL_LoadBMP("gfx/4.bmp");
	if(digit4 == NULL)
		exit(ERROR_NOIMGFILE);
	digit5 = SDL_LoadBMP("gfx/5.bmp");
	if(digit5 == NULL)
		exit(ERROR_NOIMGFILE);
	digit6 = SDL_LoadBMP("gfx/6.bmp");
	if(digit6 == NULL)
		exit(ERROR_NOIMGFILE);
	digit7 = SDL_LoadBMP("gfx/7.bmp");
	if(digit7 == NULL)
		exit(ERROR_NOIMGFILE);
	digit8 = SDL_LoadBMP("gfx/8.bmp");
	if(digit8 == NULL)
		exit(ERROR_NOIMGFILE);
	digit9 = SDL_LoadBMP("gfx/9.bmp");
	if(digit9 == NULL)
		exit(ERROR_NOIMGFILE);
	if(!nosound)
	{
		if(!SDL_LoadWAV("sfx/gameover.wav",&audiospec,&gosound,&gosound_length))
			exit(ERROR_NOWAVFILE);
		gosound_sample = gosound;
		gosound_tmplen = gosound_length;
		if(!SDL_LoadWAV("sfx/bgmusic.wav",&audiospec,&bgmusic,&bgmusic_length))
			exit(ERROR_NOWAVFILE);
		audiospec.samples = 1024;
		audiospec.callback = FillAudio;
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
	next_figure = RandomFigure();
	next_color = RandomColor();
	pause = false;
	gameover = false;
	lines = 0;
}

void Finalize(void)
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

void FillAudio(void *userdata, Uint8 *stream, int len)
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

void DisplayBoard(void)
{
	if (!ScreenUpdated(false))
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

void MakeOneStep(void)
{
	if (figure == NULL)
	{
		figure = next_figure;
		color = next_color;
		next_figure = RandomFigure();
		next_color = RandomColor();
		f_y = -FIG_DIM;							// to gain a 'slide' effect from the top of screen
		f_x = (BOARD_WIDTH - FIG_DIM) >> 1;			// to center a figure
	}
	else
	{
		++f_y;
		if (IsFigureColliding() == COLLISION_DOWN)
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

	CheckForFullLines();
	delay = (lines > 45) ? 50 : (500 - 10 * lines);
	gameover = CheckForGameEnd();

	next_time = SDL_GetTicks() + delay;
	ScreenUpdated(true);
}

void CheckForFullLines(void)
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

void HandleInput(void)
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
								RotateFigureClockwise(figure);
								if(IsFigureColliding())
								{
									++f_x;
									if(IsFigureColliding())
									{
										f_x -= 2;
										if(IsFigureColliding())
										{
											RotateFigureClockwise(figure);
											RotateFigureClockwise(figure);
											RotateFigureClockwise(figure);
											++f_x;
										}
									}
								}
							}
							ScreenUpdated(true);
						}
						break;
					case KEY_SOFTDROP:
						if(!pause && !gameover)
						{
							MakeOneStep();
						}
						ScreenUpdated(true);
						break;
					case KEY_HARDDROP:
						if (!harddrop_key_state)
						{
							harddrop_key_state = true;
						}
						break;
					case KEY_LEFT:
						if(!pause && !gameover)
						{
							--f_x;
							if(IsFigureColliding())
								++f_x;
						}
						ScreenUpdated(true);
						break;
					case KEY_RIGHT:
						if(!pause && !gameover)
						{
							++f_x;
							if(IsFigureColliding())
								--f_x;
						}
						ScreenUpdated(true);
						break;
					case KEY_PAUSE:
						if (!pause_key_state)
						{
							pause_key_state = true;
							pause = !pause;
							ScreenUpdated(true);
						}
						break;
				}
				break;
			case SDL_QUIT:
				exit(0);
				break;
		}
}

bool CheckForGameEnd(void)
{
	for (int i = 0; i < BOARD_WIDTH; ++i)
		if (board[i] != 0)
			return true;
	return false;
}

int* RandomFigure(void)
{
	const int *temp;
	int *temp2;

	switch (rand() % 7)
	{
		case 0:
			temp = square;
			break;
		case 1:
			temp = Lshape;
			break;
		case 2:
			temp = mirrorL;
			break;
		case 3:
			temp = fourshape;
			break;
		case 4:
			temp = mirrorfour;
			break;
		case 5:
			temp = stick;
			break;
		case 6:
			temp = shortT;
			break;
	}

	// creating a new instance of figure (simply copying ;p)
	temp2 = malloc(sizeof(int)*FIG_DIM*FIG_DIM);
	for (int i = 0; i < FIG_DIM*FIG_DIM; ++i)
		temp2[i] = temp[i];

	// random turn of figure
	for (int i = 0; i <= (rand() % 4); ++i)
		RotateFigureClockwise(temp2);

	return temp2;
}

SDL_Surface* RandomColor(void)
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

enum Collision IsFigureColliding(void)
// returns 0 when no collision occured
// returns 1 when colliding at the bottom
// returns 2 when colliding at sides
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
			return COLLISION_SIDE;
		if( f_y > (BOARD_HEIGHT-FIG_DIM+empty_rows_down) )
			return COLLISION_DOWN;
		for( i = 0; i < (FIG_DIM*FIG_DIM); ++i )
			if( figure[i] != 0 )
			{
				x = i % FIG_DIM + f_x;
				y = i / FIG_DIM + f_y;
				if( ((y*BOARD_WIDTH + x) < (BOARD_WIDTH*BOARD_HEIGHT)) && ((y*BOARD_WIDTH + x) >= 0) )
					if( board[y*BOARD_WIDTH + x] & figure[i] )
						return COLLISION_DOWN;
			}
	}
	return COLLISION_NONE;
}

void RotateFigureClockwise(int *fig)
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
