#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <SDL\SDL.h>

#define B_WIDTH		15
#define B_HEIGHT	30
#define F_DIM		4					// height and width
#define BLOCK_SIZE	16

SDL_Surface *screen = NULL;
SDL_Surface *sidemenu = NULL;
SDL_Surface *blue = NULL;				// sprites for blocks
SDL_Surface *gray = NULL;
SDL_Surface *green = NULL;
SDL_Surface *orange = NULL;
SDL_Surface *red = NULL;
SDL_Surface *color = NULL;				// color of currently falling figure
SDL_Surface *next_color = NULL;
SDL_Surface *digit0 = NULL;				// digits
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
// 0 - no block
// 1 - block

int f_x, f_y;							// coordinates of falling figure

int pause, gameover, lines, nosound;

// shapes - correct for 4x4 figure size
int square[]		= { 0, 0, 0, 0,
						0, 1, 1, 0,
						0, 1, 1, 0,
						0, 0, 0, 0 };
int Lshape[]		= { 0, 0, 0, 0,
						0, 1, 0, 0,
						0, 1, 0, 0,
						0, 1, 1, 0 };
int mirrorL[]		= { 0, 0, 0, 0,
						0, 0, 1, 0,
						0, 0, 1, 0,
						0, 1, 1, 0 };
int fourshape[]		= { 0, 0, 0, 0,
						0, 1, 0, 0,
						0, 1, 1, 0,
						0, 0, 1, 0 };
int mirrorfour[]	= { 0, 0, 0, 0,
						0, 0, 1, 0,
						0, 1, 1, 0,
						0, 1, 0, 0 };
int stick[]			= { 0, 1, 0, 0,
						0, 1, 0, 0,
						0, 1, 0, 0,
						0, 1, 0, 0 };
int shortT[]		= { 0, 0, 0, 0,
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
int CheckForGameEnd(void);
int* RandomFigure(void);
SDL_Surface* RandomColor(void);
int IsFigureColliding(void);
void RotateFigureClockwise(int *fig);

int main(int argc, char *argv[])
{
	int next_time;
	int delay = 500;
	
	if((argc == 2) && (!strcmp(argv[1],"-nosound")))
		nosound = 1;
	else
		nosound = 0;
		
	Initialize();
	next_time = SDL_GetTicks() + delay;
	while(1)
	{
		HandleInput();
		DisplayBoard();
		if(SDL_GetTicks() > next_time)
		{
			if(!(pause || gameover))
			{
				MakeOneStep();
				CheckForFullLines();
				delay = (lines > 45) ? 50 : (500 - 10*lines);
				if(CheckForGameEnd())
					gameover = 1;
			}
			next_time += delay;
		}
	}
	return 0;
}

void Initialize(void)
{
	int i;
	if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_AUDIO) < 0)
		exit(1);
	atexit(Finalize);
	screen = SDL_SetVideoMode(400, 480, 32, SDL_HWSURFACE | SDL_DOUBLEBUF);
	if(screen == NULL)
		exit(1);
	SDL_WM_SetCaption("Y A T K A", NULL);
	sidemenu = SDL_LoadBMP("gfx\\side.bmp");
	if(sidemenu == NULL)
		exit(1);
	blue = SDL_LoadBMP("gfx\\blue.bmp");
	if(blue == NULL)
		exit(1);
	gray = SDL_LoadBMP("gfx\\gray.bmp");
	if(gray == NULL)
		exit(1);
	green = SDL_LoadBMP("gfx\\green.bmp");
	if(green == NULL)
		exit(1);
	orange = SDL_LoadBMP("gfx\\orange.bmp");
	if(orange == NULL)
		exit(1);
	red = SDL_LoadBMP("gfx\\red.bmp");
	if(red == NULL)
		exit(1);
	digit0 = SDL_LoadBMP("gfx\\0.bmp");
	if(digit0 == NULL)
		exit(1);
	digit1 = SDL_LoadBMP("gfx\\1.bmp");
	if(digit1 == NULL)
		exit(1);
	digit2 = SDL_LoadBMP("gfx\\2.bmp");
	if(digit2 == NULL)
		exit(1);
	digit3 = SDL_LoadBMP("gfx\\3.bmp");
	if(digit3 == NULL)
		exit(1);
	digit4 = SDL_LoadBMP("gfx\\4.bmp");
	if(digit4 == NULL)
		exit(1);
	digit5 = SDL_LoadBMP("gfx\\5.bmp");
	if(digit5 == NULL)
		exit(1);
	digit6 = SDL_LoadBMP("gfx\\6.bmp");
	if(digit6 == NULL)
		exit(1);
	digit7 = SDL_LoadBMP("gfx\\7.bmp");
	if(digit7 == NULL)
		exit(1);
	digit8 = SDL_LoadBMP("gfx\\8.bmp");
	if(digit8 == NULL)
		exit(1);
	digit9 = SDL_LoadBMP("gfx\\9.bmp");
	if(digit9 == NULL)
		exit(1);
	if(!nosound)
	{
		if(!SDL_LoadWAV("sfx\\gameover.wav",&audiospec,&gosound,&gosound_length))
			exit(1);
		gosound_sample = gosound;
		gosound_tmplen = gosound_length;
		if(!SDL_LoadWAV("sfx\\bgmusic.wav",&audiospec,&bgmusic,&bgmusic_length))
			exit(1);
		audiospec.samples = 1024;
		audiospec.callback = FillAudio;
		audiospec.userdata = NULL;
		if(SDL_OpenAudio(&audiospec,NULL) == -1)
			exit(1);
		SDL_PauseAudio(0);
	}
	SDL_EnableKeyRepeat(100, 100);
		
	board = malloc(sizeof(int)*B_WIDTH*B_HEIGHT);
	for( i = 0; i < (B_WIDTH*B_HEIGHT); ++i )
		board[i] = 0;
	srand((unsigned)time(NULL));
	next_figure = RandomFigure();
	next_color = RandomColor();
	pause = 0;
	gameover = 0;
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
	int i;
	SDL_Rect rect;
	SDL_Surface *mask;
	SDL_Surface *digit;
	SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 255, 255, 255));
	for( i = 0; i < (B_WIDTH*B_HEIGHT); ++i )
	{
		rect.x = (i % B_WIDTH) * BLOCK_SIZE;
		rect.y = (i / B_WIDTH) * BLOCK_SIZE;
		if( board[i] == 1 )
			SDL_BlitSurface(gray, NULL, screen, &rect);
	}
	if( figure != NULL )
		for( i = 0; i < (F_DIM*F_DIM); ++i )
		{
			rect.x = (i % F_DIM + f_x) * BLOCK_SIZE;
			rect.y = (i / F_DIM + f_y) * BLOCK_SIZE;
			if( figure[i] == 1 )
				SDL_BlitSurface(color, NULL, screen, &rect);
		}
	
	// display the side menu
	rect.x = B_WIDTH*BLOCK_SIZE;
	rect.y = 0;
	SDL_BlitSurface(sidemenu, NULL, screen, &rect);
	for( i = 0; i < (F_DIM*F_DIM); ++i )
	{
		rect.x = (i % F_DIM + B_WIDTH) * BLOCK_SIZE + 50;
		rect.y = (i / F_DIM) * BLOCK_SIZE + 180;
		if( next_figure[i] == 1 )
			SDL_BlitSurface(next_color, NULL, screen, &rect);
	}
	// display number of removed lines
	switch( lines % 10 )
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
	rect.x = B_WIDTH*BLOCK_SIZE + 115 + 16;
	rect.y = 350;
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
	rect.x = B_WIDTH*BLOCK_SIZE + 115;
	rect.y = 350;
	SDL_BlitSurface(digit, NULL, screen, &rect);
	
	// pause
	if(pause || gameover)
	{
		mask = SDL_CreateRGBSurface(SDL_SRCALPHA, 640, 480, 32, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
		SDL_FillRect(mask, NULL, SDL_MapRGBA(mask->format,0,0,0,128));
		SDL_BlitSurface(mask, NULL, screen, NULL);
		SDL_FreeSurface(mask);
	}
	
	SDL_Flip(screen);
}

void MakeOneStep(void)
{
	int i, x, y;
	if( figure == NULL )
	{
		figure = next_figure;
		color = next_color;
		next_figure = RandomFigure();
		next_color = RandomColor();
		f_y = -F_DIM;							// to gain a 'slide' effect from the top of screen
		f_x = (B_WIDTH - F_DIM) >> 1;			// to center a figure
	}
	else
	{
		++f_y;
		if(IsFigureColliding() == 1)
		{
			--f_y;
			for( i = 0; i < (F_DIM*F_DIM); ++i )
				if( figure[i] != 0 )
				{
					x = i % F_DIM + f_x;
					y = i / F_DIM + f_y;
					if( (x >= 0) && (x < B_WIDTH) && (y >= 0) && (y < B_HEIGHT) )
						board[y*B_WIDTH + x] = 1;
				}
			free(figure);
			figure = NULL;
		}
	}
}

void CheckForFullLines(void)
{
	int x, y, ys, i;
	// checking and removing full lines
	for( y = 1; y < B_HEIGHT; ++y )
	{
		i = 1;
		for( x = 0; x < B_WIDTH; ++x )
		{
			if( board[y*B_WIDTH + x] == 0 )
			{
				i = 0;
				break;
			}
		}
		// removing
		if(i)
		{
			++lines;
			for( ys = y-1; ys >= 0; --ys )
				for( x = 0; x < B_WIDTH; ++x )
					board[(ys+1)*B_WIDTH + x] = board[ys*B_WIDTH + x];
		}
	}
}

void HandleInput(void)
{
	SDL_Event event;
	if(SDL_PollEvent(&event))
		switch(event.type)
		{
			case SDL_KEYDOWN:
				switch(event.key.keysym.sym)
				{
					case SDLK_UP:
						if(!pause)
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
						break;
					case SDLK_DOWN:	
						if(!pause)
						{
							++f_y;
							if(IsFigureColliding())
								--f_y;
						}
						break;
					case SDLK_LEFT:
						if(!pause)
						{
							--f_x;
							if(IsFigureColliding())
								++f_x;
						}
						break;
					case SDLK_RIGHT:
						if(!pause)
						{
							++f_x;
							if(IsFigureColliding())
								--f_x;
						}
						break;
					case SDLK_ESCAPE:
						pause = pause ? 0 : 1;
						break;
				}
				break;
			case SDL_QUIT:
				exit(0);
				break;
		}
}

int CheckForGameEnd(void)
{
	int i;
	for( i = 0; i < B_WIDTH; ++i )
		if( board[i] != 0 )
			return 1;
	return 0;
}

int* RandomFigure(void)
{
	int *temp;
	int *temp2;
	int i;
	switch( rand() % 7 )
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
	// random turn of figure
	for( i = 0; i <= (rand() % 4); ++i )
		RotateFigureClockwise(temp);
	
	// creating a new instance of figure (simply copying ;p)
	temp2 = malloc(sizeof(int)*F_DIM*F_DIM);
	for( i = 0; i < F_DIM*F_DIM; ++i )
		temp2[i] = temp[i];
	
	return temp2;
}

SDL_Surface* RandomColor(void)
{
	SDL_Surface* temp;
	switch( rand() % 4 )
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
	}
	return temp;
}

int IsFigureColliding(void)
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
		for( x = F_DIM-1; x >= 0; --x )
		{
			i = 1;		// empty by default
			for( y = 0; y < F_DIM; ++y )
				if( figure[y*F_DIM + x] != 0 )
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
		for( x = 0; x < F_DIM; ++x )
		{
			i = 1;		// empty by default
			for( y = 0; y < F_DIM; ++y )
				if( figure[y*F_DIM + x] != 0 )
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
		for( y = F_DIM-1; y >= 0; --y )
		{
			i = 1;		// empty by default
			for( x = 0; x < F_DIM; ++x )
				if( figure[y*F_DIM + x] != 0 )
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
		if( (f_x < -empty_columns_left) || (f_x > B_WIDTH-F_DIM+empty_columns_right) )
			return 2;
		if( f_y > (B_HEIGHT-F_DIM+empty_rows_down) )
			return 1;
		for( i = 0; i < (F_DIM*F_DIM); ++i )
			if( figure[i] != 0 )
			{
				x = i % F_DIM + f_x;
				y = i / F_DIM + f_y;
				if( ((y*B_WIDTH + x) < (B_WIDTH*B_HEIGHT)) && ((y*B_WIDTH + x) >= 0) )
					if( board[y*B_WIDTH + x] & figure[i] )
						return 1;
			}
	}
	return 0;
}

void RotateFigureClockwise(int *fig)
{
	int i, empty_rows, x, y;
	int temp[F_DIM*F_DIM];
	
	if(fig != NULL)
	{
		for( i = 0; i < F_DIM*F_DIM; ++i )
			temp[i] = fig[i];
		for( i = 0; i < F_DIM*F_DIM; ++i )
		{
			fig[i] = temp[(F_DIM-1-(i % F_DIM))*F_DIM + (i / F_DIM)];	// check this out :)
		}
	}
}
