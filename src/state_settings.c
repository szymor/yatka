
#include <stdio.h>
#include <stdbool.h>

#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>

#include "state_settings.h"
#include "joystick.h"
#include "main.h"
#include "video.h"
#include "sound.h"
#include "skin.h"


enum SettingsLine
{
	SL_TRACK_SELECT,
	SL_MUSIC_VOL,
	SL_MUSIC_REPEAT,
	SL_END
};

static int settings_pos = 0;
static SDL_Surface *pause_bg = NULL;

static const char *entry_names[SL_END] = {
	"Track Selection",
	"Music Volume",
	"Repeat Mode"
};

static void up(void);
static void down(void);
static void left(void);
static void right(void);
static void action(void);

void settings_updateScreen(void)
{
	/* cache a frozen background on first frame so DEV mouse
	 * handlers (which modify last_game_screen) don't cause
	 * overlay flicker */
	if (!pause_bg)
	{
		SDL_PixelFormat *f = screen->format;
		pause_bg = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT,
			f->BitsPerPixel, f->Rmask, f->Gmask, f->Bmask, 0);
		SDL_BlitSurface(last_game_screen, NULL, pause_bg, NULL);
	}

	SDL_BlitSurface(pause_bg, NULL, screen, NULL);

	/* semi-transparent overlay */
	SDL_PixelFormat *fmt = screen->format;
	SDL_Surface *overlay = SDL_CreateRGBSurface(
		0, SCREEN_WIDTH, SCREEN_HEIGHT, fmt->BitsPerPixel,
		fmt->Rmask, fmt->Gmask, fmt->Bmask, 0);
	SDL_FillRect(overlay, NULL, SDL_MapRGB(fmt, 0, 0, 0));
	SDL_SetAlpha(overlay, SDL_SRCALPHA, 128);
	SDL_BlitSurface(overlay, NULL, screen, NULL);
	SDL_FreeSurface(overlay);

	/* title */
	draw_text(SCREEN_WIDTH / 2, 40, "PAUSED", 1, 0);

	/* entries */
	static const char *rep_names[] = { "all", "track once", "shuffled" };

	for (int i = 0; i < SL_END; ++i)
	{
		int y = 80 + i * 15;
		char val[64];

		switch (i)
		{
			case SL_TRACK_SELECT:
				strcpy(val, music_name);
				break;
			case SL_MUSIC_VOL:
				snprintf(val, sizeof val, "%d", Mix_VolumeMusic(-1));
				break;
			case SL_MUSIC_REPEAT:
				strcpy(val, rep_names[repeattrack]);
				break;
		}

		if (i == settings_pos)
		{
			char buf[80];
			snprintf(buf, sizeof buf, "> %s", entry_names[i]);
			draw_text_col(10, y, buf, 0, 0, 255, 255, 0);
			draw_text_col(310, y, val, 2, 0, 255, 255, 0);
		}
		else
		{
			draw_text(10, y, entry_names[i], 0, 0);
			draw_text(310, y, val, 2, 0);
		}
	}

	draw_text(SCREEN_WIDTH / 2, 200, "Press ESC to resume", 1, 0);

	flipScreenScaled();
}

static void up(void)
{
	decMod(&settings_pos, SL_END, false);
}

static void down(void)
{
	incMod(&settings_pos, SL_END, false);
}

static void left(void)
{
	switch (settings_pos)
	{
		case SL_TRACK_SELECT:
		{
			playPrevTrack();
		} break;
		case SL_MUSIC_VOL:
		{
			int vol = Mix_VolumeMusic(-1);
			vol -= 1;
			if (vol < 0)
				vol = 0;
			Mix_VolumeMusic(vol);
			initmusvol = vol;
		} break;
		case SL_MUSIC_REPEAT:
		{
			repeattrack = (repeattrack + 2) % MR_END;
		} break;
		default:
			break;
	}
}

static void right(void)
{
	switch (settings_pos)
	{
		case SL_TRACK_SELECT:
		{
			playNextTrack();
		} break;
		case SL_MUSIC_VOL:
		{
			int vol = Mix_VolumeMusic(-1);
			vol += 1;
			Mix_VolumeMusic(vol);
			initmusvol = vol;
		} break;
		case SL_MUSIC_REPEAT:
		{
			repeattrack = (repeattrack + 1) % MR_END;
		} break;
		default:
			break;
	}
}

static void action(void)
{
	if (pause_bg)
	{
		SDL_FreeSurface(pause_bg);
		pause_bg = NULL;
	}
	setGameState(GS_INGAME);
}

void settings_processInputEvents(void)
{
	SDL_Event event;

	if (SDL_WaitEvent(&event))
		switch (event.type)
		{
			case SDL_JOYAXISMOTION:
				if ((event.jaxis.value < -JOY_THRESHOLD) || (event.jaxis.value > JOY_THRESHOLD))
				{
					if(event.jaxis.axis == 0)
					{
						if (event.jaxis.value < 0)
							left();
						else
							right();
					}

					if(event.jaxis.axis == 1)
					{
						if (event.jaxis.value < 0)
							up();
						else
							down();
					}
				}
				break;
			case SDL_JOYBUTTONDOWN:
				if (event.jbutton.button == JOY_PAUSE)
				{
					action();
				}
				break;
			case SDL_KEYDOWN:
				switch (event.key.keysym.sym)
				{
					case SDLK_UP:
					{
						up();
					} break;
					case SDLK_DOWN:
					{
						down();
					} break;
					case SDLK_LEFT:
					{
						left();
					} break;
					case SDLK_RIGHT:
					{
						right();
					} break;
					case SDLK_ESCAPE:
					{
						action();
					} break;
					default:
					{
						if (event.key.keysym.sym == kpause)
						{
							action();
						}
					}
				}
				break;
#ifdef DEV
			case SDL_MOUSEBUTTONDOWN:
			{
				bool changed = false;
				if (SDL_BUTTON_LEFT == event.button.button)
				{
					setBlockAtScreenXY(event.button.x, event.button.y, BO_FULL);
					changed = true;
				}
				else if (SDL_BUTTON_RIGHT == event.button.button)
				{
					setBlockAtScreenXY(event.button.x, event.button.y, BO_EMPTY);
					changed = true;
				}
				if (changed && pause_bg)
				{
					/* re‑render board_cache and apply to frozen background */
					skin_draw_board(&gameskin);
					SDL_BlitSurface(last_game_screen, NULL, pause_bg, NULL);
					SDL_BlitSurface(gameskin.board_cache, NULL, pause_bg, NULL);
				}
			}
				break;
			case SDL_MOUSEMOTION:
				event.button.state = SDL_GetMouseState(NULL, NULL);
				if (SDL_BUTTON_LMASK & event.button.state)
				{
					setBlockAtScreenXY(event.button.x, event.button.y, BO_FULL);
					skin_draw_board(&gameskin);
					SDL_BlitSurface(last_game_screen, NULL, pause_bg, NULL);
					SDL_BlitSurface(gameskin.board_cache, NULL, pause_bg, NULL);
				}
				else if (SDL_BUTTON_RMASK & event.button.state)
				{
					setBlockAtScreenXY(event.button.x, event.button.y, BO_EMPTY);
					skin_draw_board(&gameskin);
					SDL_BlitSurface(last_game_screen, NULL, pause_bg, NULL);
					SDL_BlitSurface(gameskin.board_cache, NULL, pause_bg, NULL);
				}
				break;
#endif
			case SDL_QUIT:
				exit(0);
				break;
		}
}
