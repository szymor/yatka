
#include <SDL/SDL.h>
#include <SDL/SDL_ttf.h>

#include "main.h"
#include "video.h"

SDL_Surface *screen = NULL;
SDL_Surface *screen_scaled = NULL;
SDL_Surface *last_game_screen = NULL;
TTF_Font *arcade_font = NULL;
int screenscale = 1;
int fps;

void saveLastGameScreen(void)
{
	if (last_game_screen)
		SDL_FreeSurface(last_game_screen);

	last_game_screen = SDL_CreateRGBSurface(SDL_SRCALPHA, SCREEN_WIDTH, SCREEN_HEIGHT, ALT_SCREEN_BPP, 0x000000ff, 0x0000ff00, 0x00ff0000, 0xff000000);
	SDL_BlitSurface(screen, NULL, last_game_screen, NULL);
}

void flipScreenScaled(void)
{
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

bool screenFlagUpdate(bool v)
{
	static bool value = true;
	bool old = value;
	value = v;
	return old;
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
