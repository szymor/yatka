#ifndef _H_SOUND
#define _H_SOUND

#define MUSIC_FADE_TIME		3000

extern int initmusvol;
extern Mix_Music *music;
extern char music_name[];
extern Mix_Chunk *hit;
extern Mix_Chunk *clr;
extern Mix_Chunk *click;

void initSound(void);
void deinitSound(void);
void trackFinished(void);
void playMusic(void);
void stopMusic(void);
void letMusicFinish(void);
void playNextTrack(void);
void playPrevTrack(void);

#endif
