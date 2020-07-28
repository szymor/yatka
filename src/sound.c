
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>

#include "sound.h"
#include "main.h"

int initmusvol = MIX_MAX_VOLUME / 2;
Mix_Music *music[MUSIC_TRACK_NUM];
int current_track = 0;
Mix_Chunk *hit = NULL;
Mix_Chunk *clr = NULL;

static void *LoadSound(const char *name, int mus);

void initSound(void)
{
	int mixflags = MIX_INIT_OGG | MIX_INIT_MOD | MIX_INIT_MP3;
	if (Mix_Init(mixflags) != mixflags)
		exit(ERROR_MIXINIT);
	if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 1, 1024) < 0)
		exit(ERROR_OPENAUDIO);
	music[0] = (Mix_Music*)LoadSound("sfx/korobeyniki", 1);
	if (!music[0])
		exit(ERROR_NOSNDFILE);
	music[1] = (Mix_Music*)LoadSound("sfx/bradinsky", 1);
	if (!music[1])
		exit(ERROR_NOSNDFILE);
	music[2] = (Mix_Music*)LoadSound("sfx/kalinka", 1);
	if (!music[2])
		exit(ERROR_NOSNDFILE);
	music[3] = (Mix_Music*)LoadSound("sfx/troika", 1);
	if (!music[3])
		exit(ERROR_NOSNDFILE);
	hit = (Mix_Chunk*)LoadSound("sfx/hit", 0);
	if (!hit)
		exit(ERROR_NOSNDFILE);
	clr = (Mix_Chunk*)LoadSound("sfx/clear", 0);
	if (!clr)
		exit(ERROR_NOSNDFILE);

	current_track = 0;
	Mix_VolumeMusic(initmusvol);
}

static void *LoadSound(const char *name, int mus)
{
	static char *ext[] = { ".ogg", ".mod", ".mp3", ".wav" };
	void *ret = NULL;
	char buff[256];
	int len;
	strcpy(buff, name);
	len = strlen(buff);
	for (int i = 0; i < (sizeof(ext)/sizeof(ext[0])); ++i)
	{
		buff[len] = '\0';
		strcat(buff, ext[i]);
		if (mus)
			ret = (void*)Mix_LoadMUS(buff);
		else
			ret = (void*)Mix_LoadWAV(buff);
		if (ret)
			break;
	}
	return ret;
}

void deinitSound(void)
{
	/* calling Mix_Quit() causes segmentation fault
	 * at SDL audio subsystem quit.
	 */
	// Mix_Quit();
}

void trackFinished(void)
{
	if (GS_GAMEOVER != gamestate && GS_MAINMENU != gamestate)
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

void playMusic(void)
{
	Mix_HookMusicFinished(trackFinished);
	if (!Mix_PlayingMusic())
		Mix_FadeInMusic(music[current_track], 1, MUSIC_FADE_TIME);
}

void stopMusic(void)
{
	Mix_FadeOutMusic(MUSIC_FADE_TIME);
}

void letMusicFinish(void)
{
	Mix_HookMusicFinished(NULL);
}
