
#include <stdio.h>

#include <SDL/SDL_mixer.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "main.h"
#include "state_mainmenu.h"
#include "sound.h"
#include "video.h"
#include "data_persistence.h"
#include "randomizer.h"

#define GAMEDIR				".yatka"
#define HISCORE_FILE		"hiscore.dat"
#define SETTINGS_FILE		"settings.txt"

char dirpath[256];
static char hiscore_path[256];
static char settings_path[256];
static int records[RT_END] = { 0 };
static bool records_need_save = false;

static void createGameDir(void)
{
	mkdir(dirpath, 0744);
}

void initPaths(void)
{
	const char *home = getenv("HOME");
	sprintf(dirpath, "%s/%s", home, GAMEDIR);
	sprintf(hiscore_path, "%s/%s", dirpath, HISCORE_FILE);
	sprintf(settings_path, "%s/%s", dirpath, SETTINGS_FILE);
}

int getRecord(enum RecordType rt)
{
	return records[rt];
}

void setRecord(enum RecordType rt, int record)
{
	records_need_save = records[rt] != record;
	records[rt] = record;
}

void loadRecords(void)
{
	FILE *hifile = fopen(hiscore_path, "r");
	if (hifile)
	{
		for (int i = 0; i < RT_END; ++i)
		{
			int value = 0;
			if (1 == fscanf(hifile, "%d", &value))
			{
				setRecord(i, value);
			}
		}
		fclose(hifile);
		records_need_save = false;
	}
}

void saveRecords(void)
{
	if (!records_need_save)
		return;
	createGameDir();
	FILE *hifile = fopen(hiscore_path, "w");
	if (hifile)
	{
		for (int i = 0; i < RT_END; ++i)
		{
			fprintf(hifile, "%d\n", getRecord(i));
		}
		fclose(hifile);
		records_need_save = false;
	}
}

void loadSettings(void)
{
	char buff[256];
	FILE *settingsFile = fopen(settings_path, "r");
	if (!settingsFile)
		return;

	while (!feof(settingsFile))
	{
		fscanf(settingsFile, "%s", buff);
		if (!strcmp(buff, "nosound"))
			nosound = true;
		else if (!strcmp(buff, "smoothanim"))
			smoothanim = true;
		else if (!strcmp(buff, "easyspin"))
			easyspin = true;
		else if (!strcmp(buff, "lockdelay"))
			lockdelay = true;
		else if (!strcmp(buff, "repeattrack"))
			repeattrack = true;
		else if (!strcmp(buff, "fullscreen"))
			screenscale = 0;
		else if (!strcmp(buff, "scale1x"))
			screenscale = 1;
		else if (!strcmp(buff, "scale2x"))
			screenscale = 2;
		else if (!strcmp(buff, "scale3x"))
			screenscale = 3;
		else if (!strcmp(buff, "scale4x"))
			screenscale = 4;
		else if (!strcmp(buff, "holdoff"))
			holdoff = true;
		else if (!strcmp(buff, "musicvol"))
		{
			fscanf(settingsFile, "%d", &initmusvol);
		}
		else if (!strcmp(buff, "tetrominocolor"))
		{
			fscanf(settingsFile, "%d", &tetrominocolor);
		}
		else if (!strcmp(buff, "rng"))
		{
			fscanf(settingsFile, "%s", buff);
			for (int i = 0; i < RA_END; ++i)
			{
				randomalgo = i;
				if (!strcmp(buff, getRandomizerString()))
					break;
			}
		}
		else if (!strcmp(buff, "kleft"))
		{
			fscanf(settingsFile, "%d", &kleft);
		}
		else if (!strcmp(buff, "kright"))
		{
			fscanf(settingsFile, "%d", &kright);
		}
		else if (!strcmp(buff, "ksoftdrop"))
		{
			fscanf(settingsFile, "%d", &ksoftdrop);
		}
		else if (!strcmp(buff, "kharddrop"))
		{
			fscanf(settingsFile, "%d", &kharddrop);
		}
		else if (!strcmp(buff, "krotatecw"))
		{
			fscanf(settingsFile, "%d", &krotatecw);
		}
		else if (!strcmp(buff, "krotateccw"))
		{
			fscanf(settingsFile, "%d", &krotateccw);
		}
		else if (!strcmp(buff, "khold"))
		{
			fscanf(settingsFile, "%d", &khold);
		}
		else if (!strcmp(buff, "kpause"))
		{
			fscanf(settingsFile, "%d", &kpause);
		}
		else if (!strcmp(buff, "kquit"))
		{
			fscanf(settingsFile, "%d", &kquit);
		}
	}

	fclose(settingsFile);
}

void saveSettings(void)
{
	createGameDir();
	FILE *settingsFile = fopen(settings_path, "w");

	if (nosound)
		fprintf(settingsFile, "nosound\n");
	if (smoothanim)
		fprintf(settingsFile, "smoothanim\n");
	if (easyspin)
		fprintf(settingsFile, "easyspin\n");
	if (lockdelay)
		fprintf(settingsFile, "lockdelay\n");
	if (repeattrack)
		fprintf(settingsFile, "repeattrack\n");
	if (0 == screenscale)
		fprintf(settingsFile, "fullscreen\n");
	if (1 == screenscale)
		fprintf(settingsFile, "scale1x\n");
	if (2 == screenscale)
		fprintf(settingsFile, "scale2x\n");
	if (3 == screenscale)
		fprintf(settingsFile, "scale3x\n");
	if (4 == screenscale)
		fprintf(settingsFile, "scale4x\n");
	if (holdoff)
		fprintf(settingsFile, "holdoff\n");
	if (!nosound)
		fprintf(settingsFile, "musicvol %d\n", Mix_VolumeMusic(-1));
	fprintf(settingsFile, "tetrominocolor %d\n", tetrominocolor);
	fprintf(settingsFile, "rng %s\n", getRandomizerString());

	fprintf(settingsFile, "kleft %d\n", kleft);
	fprintf(settingsFile, "kright %d\n", kright);
	fprintf(settingsFile, "ksoftdrop %d\n", ksoftdrop);
	fprintf(settingsFile, "kharddrop %d\n", kharddrop);
	fprintf(settingsFile, "krotatecw %d\n", krotatecw);
	fprintf(settingsFile, "krotateccw %d\n", krotateccw);
	fprintf(settingsFile, "khold %d\n", khold);
	fprintf(settingsFile, "kpause %d\n", kpause);
	fprintf(settingsFile, "kquit %d\n", kquit);

	fclose(settingsFile);
}
