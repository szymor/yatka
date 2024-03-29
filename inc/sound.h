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

// clear and combo order matters!
// add new sound effects after combos
enum SfxEffect
{
	SE_NONE,
	SE_CLEAR,
	SE_COMBO_1X,
	SE_COMBO_2X,
	SE_COMBO_3X,
	SE_COMBO_4X,
	SE_COMBO_5X,
	SE_COMBO_6X,
	SE_COMBO_7X,
	SE_HIT,
	SE_CLICK,
	SE_END
};

extern int initmusvol;
extern Mix_Music *music;
extern char music_name[];
extern Mix_Chunk *sfx_hit;
extern Mix_Chunk *sfx_clr;
extern Mix_Chunk *sfx_click;
extern Mix_Chunk *sfx_b2b;
extern Mix_Chunk *sfx_single;
extern Mix_Chunk *sfx_double;
extern Mix_Chunk *sfx_triple;
extern Mix_Chunk *sfx_tetris;
extern Mix_Chunk *sfx_tspin;
extern Mix_Chunk *sfx_minitspin;

void initSound(void);
void deinitSound(void);
void trackFinished(void);
void playMusic(void);
void stopMusic(void);
void letMusicFinish(void);
void playNextTrack(void);
void playPrevTrack(void);
void playSpeech(int ssflags);
void playEffect(enum SfxEffect se);
void playcombo(int combo);

#endif
