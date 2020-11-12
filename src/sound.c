
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#include <sys/types.h>
#include <dirent.h>
#include "sound.h"
#include "main.h"
#include "data_persistence.h"

int initmusvol = MIX_MAX_VOLUME / 2;
Mix_Music *music = NULL;
char music_name[32] = "<none>";
Mix_Chunk *hit = NULL;
Mix_Chunk *clr = NULL;
Mix_Chunk *click = NULL;

static DIR *music_dp;
static const char default_music_dir[] = "music/";
static char custom_music_dir[256] = "";
static const char *music_dir = NULL;

static struct dirent *getNextDirEntry(void);
static void loadNextTrack(void);

static struct dirent *getNextDirEntry(void)
{
	struct dirent *ep = NULL;
	ep = readdir(music_dp);
	if (!ep)
	{
		rewinddir(music_dp);
		ep = readdir(music_dp);
	}
	return ep;
}

static void loadNextTrack(void)
{
	if (music)
	{
		Mix_FreeMusic(music);
		music = NULL;
	}

	struct dirent *ep = NULL;
	char buff[64];
	bool invalidINodeOccured = false;
	ino_t invalidINode = 0;
	ep = getNextDirEntry();
	while (ep && (!invalidINodeOccured || (invalidINodeOccured && (ep->d_ino != invalidINode))))
	{
		sprintf(buff, "%s%s", music_dir, ep->d_name);
		music = Mix_LoadMUS(buff);
		if (!music)
		{
			if (!invalidINodeOccured)
			{
				invalidINodeOccured = true;
				invalidINode = ep->d_ino;
			}
			ep = getNextDirEntry();
			continue;
		}
		sprintf(music_name, "%s", ep->d_name);
		break;
	}

	if (!music)
	{
		sprintf(music_name, "%s", "<none>");
	}
}

void initSound(void)
{
#if defined(_RETROFW)
	int mixflags = MIX_INIT_OGG | MIX_INIT_MOD;
#elif defined(_BITTBOY)
	int mixflags = MIX_INIT_OGG;
#else
	int mixflags = MIX_INIT_OGG | MIX_INIT_MOD | MIX_INIT_MP3;
#endif
	if (Mix_Init(mixflags) != mixflags)
	{
		printf("Mix_Init failed.\n");
		exit(ERROR_MIXINIT);
	}
	if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 1, 1024) < 0)
	{
		printf("Mix_OpenAudio failed.\n");
		exit(ERROR_OPENAUDIO);
	}
	hit = Mix_LoadWAV("sfx/hit.ogg");
	if (!hit)
		exit(ERROR_NOSNDFILE);
	clr = Mix_LoadWAV("sfx/clear.ogg");
	if (!clr)
		exit(ERROR_NOSNDFILE);
	click = Mix_LoadWAV("sfx/click.wav");
	if (!click)
		exit(ERROR_NOSNDFILE);

	Mix_VolumeMusic(initmusvol);

	sprintf(custom_music_dir, "%s/%s", dirpath, default_music_dir);
	music_dp = opendir(custom_music_dir);
	if (music_dp != NULL)
	{
		music_dir = custom_music_dir;
	}
	else
	{
		music_dp = opendir(default_music_dir);
		if (music_dp != NULL)
		{
			music_dir = default_music_dir;
		}
		else
		{
			music_dir = NULL;
		}
	}

	if (music_dp != NULL)
	{
		loadNextTrack();
	}
	else
	{
		perror("Couldn't open the directory");
	}
}

void deinitSound(void)
{
	closedir(music_dp);
	if (music)
	{
		Mix_FreeMusic(music);
		music = NULL;
	}
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
			if (music_dp != NULL)
				loadNextTrack();
		}
		Mix_PlayMusic(music, 1);

		// informing event
		SDL_Event event;

		event.type = SDL_USEREVENT;
		event.user.code = 0;
		event.user.data1 = 0;
		event.user.data2 = 0;
		SDL_PushEvent(&event);
	}
}

void playMusic(void)
{
	Mix_HookMusicFinished(trackFinished);
	if (!Mix_PlayingMusic())
		Mix_FadeInMusic(music, 1, MUSIC_FADE_TIME);
}

void stopMusic(void)
{
	Mix_FadeOutMusic(MUSIC_FADE_TIME);
}

void letMusicFinish(void)
{
	Mix_HookMusicFinished(NULL);
}

void playNextTrack(void)
{
	if (music_dp != NULL)
		loadNextTrack();
	Mix_PlayMusic(music, 1);
}

void playPrevTrack(void)
{
	playNextTrack();
}
