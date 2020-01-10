
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>

#include "sound.h"
#include "main.h"

int initmusvol = MIX_MAX_VOLUME / 2;
Mix_Music *music[MUSIC_TRACK_NUM];
int current_track = 0;
Mix_Chunk *hit = NULL;
Mix_Chunk *clr = NULL;

void initSound(void)
{
	int mixflags = MIX_INIT_OGG;
	if (Mix_Init(mixflags) != mixflags)
		exit(ERROR_MIXINIT);
	if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 1, 1024) < 0)
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
	clr = Mix_LoadWAV("sfx/clear.ogg");
	if (!clr)
		exit(ERROR_NOSNDFILE);

	current_track = 0;
	Mix_FadeInMusic(music[current_track], 1, MUSIC_FADE_TIME);
	Mix_HookMusicFinished(trackFinished);
	Mix_VolumeMusic(initmusvol);
}

void deinitSound(void)
{
	Mix_Quit();
}

void trackFinished(void)
{
	if (GS_GAMEOVER != gamestate)
	{
		if (!repeattrack)
		{
			incMod(&current_track, MUSIC_TRACK_NUM, false);
		}
		Mix_PlayMusic(music[current_track], 1);

		// informing event
		SDL_Event event;

		event.type = SDL_USEREVENT;
		event.user.code = current_track;
		event.user.data1 = 0;
		event.user.data2 = 0;
		SDL_PushEvent(&event);
	}
}
