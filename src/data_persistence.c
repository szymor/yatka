
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

static char dirpath[256];
static char hiscore_path[256];
static char settings_path[256];

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

int loadHiscore(void)
{
	FILE *hifile = fopen(hiscore_path, "r");
	if (hifile)
	{
		int hi = 0;
		fscanf(hifile, "%d", &hi);
		fclose(hifile);
		return hi;
	}
	else
		return 0;
}

void saveHiscore(int hi)
{
	createGameDir();
	FILE *hifile = fopen(hiscore_path, "w");
	if (hifile)
	{
		fprintf(hifile, "%d", hi);
		fclose(hifile);
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
		else if (!strcmp(buff, "numericbars"))
			numericbars = true;
		else if (!strcmp(buff, "repeattrack"))
			repeattrack = true;
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
		else if (!strcmp(buff,"rng"))
		{
			fscanf(settingsFile, "%s", buff);
			if (!strcmp(buff, "naive"))
				randomalgo = RA_NAIVE;
			else if (!strcmp(buff, "7bag"))
				randomalgo = RA_7BAG;
			else if (!strcmp(buff, "8bag"))
				randomalgo = RA_8BAG;
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
	if (numericbars)
		fprintf(settingsFile, "numericbars\n");
	if (repeattrack)
		fprintf(settingsFile, "repeattrack\n");
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

	fclose(settingsFile);
}
