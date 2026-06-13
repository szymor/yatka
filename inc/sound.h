#ifndef _H_SOUND
#define _H_SOUND

#include <SDL/SDL_mixer.h>

#define MUSIC_FADE_TIME		3000

enum SfxSpeech
{
	SS_B2B,
	SS_TSPIN,
	SS_MINITSPIN,
	SS_SINGLE,
	SS_DOUBLE,
	SS_TRIPLE,
	SS_TETRIS,
	SS_END
};

extern int initmusvol;
extern Mix_Music *music;
extern char music_name[];
extern Mix_Chunk *menu_click;

void initSound(void);
void deinitSound(void);
void trackFinished(void);
void playMusic(void);
void stopMusic(void);
void letMusicFinish(void);
void playNextTrack(void);
void playPrevTrack(void);
void playSpeech(int ssflags);

#endif
