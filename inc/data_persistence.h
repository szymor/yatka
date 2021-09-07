#ifndef _H_HISCORE
#define _H_HISCORE

enum RecordType
{
	RT_MARATHON_SCORE,
	RT_MARATHON_LINES,
	RT_SPRINT_TIME,
	RT_ULTRA_SCORE,
	RT_ULTRA_LINES,
	RT_END
};

extern char dirpath[];

void initPaths(void);

int getRecord(enum RecordType rt);
void setRecord(enum RecordType rt, int record);
void loadRecords(void);
void saveRecords(void);

void loadSettings(void);
void saveSettings(void);

#endif
