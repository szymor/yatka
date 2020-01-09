#ifndef _H_SOUND
#define _H_SOUND

#define MUSIC_TRACK_NUM		4
#define MUSIC_FADE_TIME		3000

extern Mix_Music *music[MUSIC_TRACK_NUM];
extern int current_track;
extern Mix_Chunk *hit;
extern Mix_Chunk *clr;

void initSound(void);
void deinitSound(void);
void selectNextTrack(void);
void selectPreviousTrack(void);
void trackFinished(void);

#endif
