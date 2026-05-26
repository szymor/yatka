
#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>
#include <sys/types.h>
#include <dirent.h>
#include <stdlib.h>
#include "sound.h"
#include "main.h"
#include "data_persistence.h"

#define MAX_TRACKS		256
#define SFXSPEECH_CHANNEL	(0)

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
	"sfx/combo_8.wav",
	"sfx/combo_9.wav",
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

static const char default_music_dir[] = "music/";
static char custom_music_dir[256] = "";
static const char *music_dir = NULL;

static char track_names[MAX_TRACKS][64];
static int track_count = 0;
static int track_index = -1;
static int shuffle_order[MAX_TRACKS];
static int shuffle_pos = 0;

static void init_shuffle(void)
{
	for (int i = 0; i < track_count; ++i)
		shuffle_order[i] = i;
	for (int i = track_count - 1; i > 0; --i)
	{
		int j = rand() % (i + 1);
		int tmp = shuffle_order[i];
		shuffle_order[i] = shuffle_order[j];
		shuffle_order[j] = tmp;
	}
	shuffle_pos = 0;
}

static int track_cmp(const void *a, const void *b)
{
	return strcmp((const char *)a, (const char *)b);
}

static void scan_tracks(void)
{
	track_count = 0;
	DIR *dp = opendir(music_dir);
	if (!dp) return;

	struct dirent *ep;
	while ((ep = readdir(dp)) != NULL && track_count < MAX_TRACKS)
	{
		if (!strcmp(ep->d_name, ".") || !strcmp(ep->d_name, ".."))
			continue;
		char path[64];
		snprintf(path, sizeof path, "%s%.63s", music_dir, ep->d_name);
		Mix_Music *m = Mix_LoadMUS(path);
		if (m)
		{
			Mix_FreeMusic(m);
			snprintf(track_names[track_count], 64, "%s", ep->d_name);
			++track_count;
		}
	}
	closedir(dp);

	if (track_count > 1)
		qsort(track_names, track_count, 64, track_cmp);
}

static void load_track(int idx)
{
	if (music)
	{
		Mix_FreeMusic(music);
		music = NULL;
	}

	if (idx < 0 || idx >= track_count)
	{
		sprintf(music_name, "%s", "<none>");
		return;
	}

	char path[64];
	snprintf(path, sizeof path, "%s%.63s", music_dir, track_names[idx]);
	music = Mix_LoadMUS(path);
	if (music)
		snprintf(music_name, 32, "%s", track_names[idx]);
	else
		sprintf(music_name, "%s", "<none>");

	track_index = idx;
}

static void channelDone(int channel);

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
	Mix_ReserveChannels(SFXSPEECH_CHANNEL + 1);
	Mix_ChannelFinished(channelDone);

	sprintf(custom_music_dir, "%s/%s", dirpath, default_music_dir);
	music_dir = custom_music_dir;
	DIR *dp = opendir(custom_music_dir);
	if (!dp)
	{
		music_dir = default_music_dir;
		dp = opendir(default_music_dir);
	}
	if (dp)
	{
		closedir(dp);
		scan_tracks();
		init_shuffle();
		load_track(0);
	}
	else
	{
		music_dir = NULL;
		perror("Couldn't open the directory");
	}
}

void deinitSound(void)
{
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
		if (MR_TRACK_ONCE == repeattrack) return;
		if (track_count < 1) return;

		if (MR_SHUFFLED == repeattrack)
		{
			shuffle_pos = (shuffle_pos + 1) % track_count;
			if (0 == shuffle_pos) init_shuffle();
			load_track(shuffle_order[shuffle_pos]);
		}
		else                                    /* all (sequential) */
		{
			load_track((track_index + 1) % track_count);
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
	if (track_count < 1) return;

	if (MR_SHUFFLED == repeattrack)
	{
		shuffle_pos = (shuffle_pos + 1) % track_count;
		if (0 == shuffle_pos) init_shuffle();
		load_track(shuffle_order[shuffle_pos]);
	}
	else
	{
		load_track((track_index + 1) % track_count);
	}
	if (music)
		Mix_PlayMusic(music, 1);
}

void playPrevTrack(void)
{
	if (track_count < 1) return;

	if (MR_SHUFFLED == repeattrack)
	{
		shuffle_pos = (shuffle_pos - 1 + track_count) % track_count;
		load_track(shuffle_order[shuffle_pos]);
	}
	else
	{
		load_track((track_index - 1 + track_count) % track_count);
	}
	if (music)
		Mix_PlayMusic(music, 1);
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
	Mix_PlayChannel(-1, sfx_effects[se], 0);
}
