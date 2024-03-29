
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
Mix_Chunk *sfx_effects[SE_END] = { NULL };
Mix_Chunk *sfx_speech[SS_END] = { NULL };

int ssflags_planned = 0;

static const char sfx_effect_paths[SE_END][32] = {
	"none",
	"sfx/clear.wav",
	"sfx/combo_1.wav",
	"sfx/combo_2.wav",
	"sfx/combo_3.wav",
	"sfx/combo_4.wav",
	"sfx/combo_5.wav",
	"sfx/combo_6.wav",
	"sfx/combo_7.wav",
	"sfx/hit.wav",
	"sfx/click.wav"
};

static const char sfx_speech_paths[SS_END][32] = {
	"sfx/b2b.wav",
	"sfx/tspin.wav",
	"sfx/minitspin.wav",
	"sfx/single.wav",
	"sfx/double.wav",
	"sfx/triple.wav",
	"sfx/tetris.wav"
};

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
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
		{
			ep = getNextDirEntry();
			continue;
		}

		sprintf(buff, "%s%.63s", music_dir, ep->d_name);
		log("Track: %s\n", buff);
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

	for (int i = SE_CLEAR; i < SE_END; ++i)
	{
		sfx_effects[i] = Mix_LoadWAV(sfx_effect_paths[i]);
		if (!sfx_effects[i])
			exit(ERROR_NOSNDFILE);
	}

	for (int i = SS_B2B; i < SS_END; ++i)
	{
		sfx_speech[i] = Mix_LoadWAV(sfx_speech_paths[i]);
		if (!sfx_speech[i])
			exit(ERROR_NOSNDFILE);
	}

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

	if (Mix_Playing(SFXSPEECH_CHANNEL))
	{
		Mix_ChannelFinished(NULL);
		Mix_HaltChannel(SFXSPEECH_CHANNEL);
		Mix_ChannelFinished(channelDone);
	}
	Mix_PlayChannel(SFXSPEECH_CHANNEL, sfx_speech[shift], 0);
	ssflags_planned = ssflags & ~(1 << shift);
}

void playEffect(enum SfxEffect se)
{
	if (SE_NONE == se)
		return;
	Mix_PlayChannel(SFXEFFECT_CHANNEL, sfx_effects[se], 0);
}
