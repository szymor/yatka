#ifndef _H_HISCORE
#define _H_HISCORE

extern char dirpath[];

void initPaths(void);

int loadHiscore(void);
void saveHiscore(int hi);

void loadSettings(void);
void saveSettings(void);

#endif
