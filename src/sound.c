
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#include <sys/types.h>
#include <dirent.h>
#include "sound.h"
#include "main.h"
#include "data_persistence.h"

#define SFXEFFECT_CHANNEL	(2)
#define SFXSPEECH_CHANNEL	(3)

int initmusvol = MIX_MAX_VOLUME / 4;
Mix_Music *music = NULL;
char music_name[32] = "<none>";
Mix_Chunk *sfx_hit = NULL;
Mix_Chunk *sfx_clr = NULL;
Mix_Chunk *sfx_click = NULL;
Mix_Chunk *sfx_b2b = NULL;
Mix_Chunk *sfx_single = NULL;
Mix_Chunk *sfx_double = NULL;
Mix_Chunk *sfx_triple = NULL;
Mix_Chunk *sfx_tetris = NULL;
Mix_Chunk *sfx_tspin = NULL;
Mix_Chunk *sfx_minitspin = NULL;
Mix_Chunk *sfx_combo1x = NULL;
Mix_Chunk *sfx_combo2x = NULL;
Mix_Chunk *sfx_combo3x = NULL;
Mix_Chunk *sfx_combo4x = NULL;
Mix_Chunk *sfx_combo5x = NULL;
Mix_Chunk *sfx_combo6x = NULL;
Mix_Chunk *sfx_combo7x = NULL;

int ssflags_planned = 0;

static DIR *music_dp;
static const char default_music_dir[] = "music/";
static char custom_music_dir[256] = "";
static const char *music_dir = NULL;

static struct dirent *getNextDirEntry(void);
static void loadNextTrack(void);
static void channelDone(int channel);

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
		sprintf(buff, "%s%.63s", music_dir, ep->d_name);
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
		sprintf(music_name, "%.31s", ep->d_name);
		break;
	}

	if (!music)
	{
		sprintf(music_name, "%s", "<none>");
	}
}

static void channelDone(int channel)
{
	if (SFXSPEECH_CHANNEL == channel)
	{
		playSpeech(ssflags_planned);
	}
}

void initSound(void)
{
	int mixflags = -1;
	int retflags = Mix_Init(mixflags);
	if (retflags != mixflags)
	{
		retflags = Mix_Init(retflags);
	}
	if (retflags & MIX_INIT_FLAC)
	{
		log("Mix_Init: FLAC supported.\n");
	}
	if (retflags & MIX_INIT_MOD)
	{
		log("Mix_Init: MOD supported.\n");
	}
	if (retflags & MIX_INIT_MP3)
	{
		log("Mix_Init: MP3 supported.\n");
	}
	if (retflags & MIX_INIT_OGG)
	{
		log("Mix_Init: OGG supported.\n");
	}
	if (retflags & MIX_INIT_FLUIDSYNTH)
	{
		log("Mix_Init: MIDI supported (FluidSynth?).\n");
	}
	if (Mix_OpenAudio(22050, MIX_DEFAULT_FORMAT, 1, 1024) < 0)
	{
		printf("Mix_OpenAudio failed.\n");
		exit(ERROR_OPENAUDIO);
	}
	sfx_hit = Mix_LoadWAV("sfx/hit.wav");
	if (!sfx_hit)
		exit(ERROR_NOSNDFILE);
	sfx_clr = Mix_LoadWAV("sfx/clear.wav");
	if (!sfx_clr)
		exit(ERROR_NOSNDFILE);
	sfx_click = Mix_LoadWAV("sfx/click.wav");
	if (!sfx_click)
		exit(ERROR_NOSNDFILE);
	sfx_b2b = Mix_LoadWAV("sfx/b2b.wav");
	if (!sfx_b2b)
		exit(ERROR_NOSNDFILE);
	sfx_single = Mix_LoadWAV("sfx/single.wav");
	if (!sfx_single)
		exit(ERROR_NOSNDFILE);
	sfx_double = Mix_LoadWAV("sfx/double.wav");
	if (!sfx_double)
		exit(ERROR_NOSNDFILE);
	sfx_triple = Mix_LoadWAV("sfx/triple.wav");
	if (!sfx_triple)
		exit(ERROR_NOSNDFILE);
	sfx_tetris = Mix_LoadWAV("sfx/tetris.wav");
	if (!sfx_tetris)
		exit(ERROR_NOSNDFILE);
	sfx_tspin = Mix_LoadWAV("sfx/tspin.wav");
	if (!sfx_tspin)
		exit(ERROR_NOSNDFILE);
	sfx_minitspin = Mix_LoadWAV("sfx/minitspin.wav");
	if (!sfx_minitspin)
		exit(ERROR_NOSNDFILE);
	sfx_combo1x = Mix_LoadWAV("sfx/combo_1.wav");
	if (!sfx_combo1x)
		exit(ERROR_NOSNDFILE);
	sfx_combo2x = Mix_LoadWAV("sfx/combo_2.wav");
	if (!sfx_combo2x)
		exit(ERROR_NOSNDFILE);
	sfx_combo3x = Mix_LoadWAV("sfx/combo_3.wav");
	if (!sfx_combo3x)
		exit(ERROR_NOSNDFILE);
	sfx_combo4x = Mix_LoadWAV("sfx/combo_4.wav");
	if (!sfx_combo4x)
		exit(ERROR_NOSNDFILE);
	sfx_combo5x = Mix_LoadWAV("sfx/combo_5.wav");
	if (!sfx_combo5x)
		exit(ERROR_NOSNDFILE);
	sfx_combo6x = Mix_LoadWAV("sfx/combo_6.wav");
	if (!sfx_combo6x)
		exit(ERROR_NOSNDFILE);
	sfx_combo7x = Mix_LoadWAV("sfx/combo_7.wav");
	if (!sfx_combo7x)
		exit(ERROR_NOSNDFILE);

	Mix_VolumeMusic(initmusvol);
	log("Number of channels: %d\n", Mix_AllocateChannels(-1));
	Mix_ChannelFinished(channelDone);

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

void playSpeech(int ssflags)
{
	if (0 == ssflags)
		return;
	int shift = 0;
	int sstemp = ssflags;
	while (!(sstemp & 1))
	{
		++shift;
		sstemp >>= 1;
	}
	Mix_Chunk *chunk = NULL;
	int toplay = 1 << shift;
	switch (toplay)
	{
		case SS_B2B:
			chunk = sfx_b2b;
			break;
		case SS_TSPIN:
			chunk = sfx_tspin;
			break;
		case SS_MINITSPIN:
			chunk = sfx_minitspin;
			break;
		case SS_SINGLE:
			chunk = sfx_single;
			break;
		case SS_DOUBLE:
			chunk = sfx_double;
			break;
		case SS_TRIPLE:
			chunk = sfx_triple;
			break;
		case SS_TETRIS:
			chunk = sfx_tetris;
	}
	if (Mix_Playing(SFXSPEECH_CHANNEL))
	{
		Mix_ChannelFinished(NULL);
		Mix_HaltChannel(SFXSPEECH_CHANNEL);
		Mix_ChannelFinished(channelDone);
	}
	Mix_PlayChannel(SFXSPEECH_CHANNEL, chunk, 0);
	ssflags_planned = ssflags & ~toplay;
}

void playEffect(enum SfxEffect se)
{
	switch (se)
	{
		case SE_HIT:
			Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_hit, 0);
			break;
		case SE_CLEAR:
			Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_clr, 0);
			break;
		case SE_CLICK:
			Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_click, 0);
	}
}

void playcombo(int combo)
{
		switch (combo)
	{
		case 0:
			break;
		case 1:
			Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_clr, 0);
			break;
		case 2:
			Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_combo1x, 0);
			break;
		case 3:
			Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_combo2x, 0);
			break;
		case 4:
			Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_combo3x, 0);
			break;
		case 5:
			Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_combo4x, 0);
			break;
		case 6:
			Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_combo5x, 0);
			break;
		case 7:
			Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_combo6x, 0);
			break;
		case 8:
			Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_combo7x, 0);
			break;
		default:
			Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_combo7x, 0);
			break;
	}

}
