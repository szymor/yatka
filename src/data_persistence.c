
#include <stdio.h>

#include "main.h"
#include "video.h"
#include "data_persistence.h"
#include "randomizer.h"

#define HISCORE_PATH		"hiscore.dat"
#define SETTINGS_PATH		"settings.txt"

int loadHiscore(void)
{
	FILE *hifile = fopen(HISCORE_PATH, "r");
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
	FILE *hifile = fopen(HISCORE_PATH, "w");
	if (hifile)
	{
		fprintf(hifile, "%d", hi);
		fclose(hifile);
	}
}

void loadSettings(void)
{
	char buff[256];
	FILE *settingsFile = fopen(SETTINGS_PATH, "r");
	if (!settingsFile)
		return;

	while (!feof(settingsFile))
	{
		fscanf(settingsFile, "%s", buff);
		if (!strcmp(buff, "nosound"))
			nosound = true;
		else if (!strcmp(buff, "randomcolors"))
			randomcolors = true;
		else if (!strcmp(buff, "numericbars"))
			numericbars = true;
		else if (!strcmp(buff, "repeattrack"))
			repeattrack = true;
		else if (!strcmp(buff, "debug"))
			debug = true;
		else if (!strcmp(buff, "scale2x"))
			screenscale = 2;
		else if (!strcmp(buff, "scale3x"))
			screenscale = 3;
		else if (!strcmp(buff, "scale4x"))
			screenscale = 4;
		else if (!strcmp(buff, "holdoff"))
			holdoff = true;
		else if (!strcmp(buff, "grayblocks"))
			grayblocks = true;
		else if (!strcmp(buff, "ghostoff"))
			ghostoff = true;
		else if (!strcmp(buff, "startlevel"))
		{
			fscanf(settingsFile, "%d", &startlevel);
		}
		else if (!strcmp(buff, "nextblocks"))
		{
			fscanf(settingsFile, "%d", &nextblocks);
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
	FILE *settingsFile = fopen(SETTINGS_PATH, "w");
	
	if (nosound)
		fprintf(settingsFile, "nosound\n");
	if (randomcolors)
		fprintf(settingsFile, "randomcolors\n");;
	if (numericbars)
		fprintf(settingsFile, "numericbars\n");;
	if (repeattrack)
		fprintf(settingsFile, "repeattrack\n");;
	if (2 == screenscale)
		fprintf(settingsFile, "scale2x\n");;
	if (3 == screenscale)
		fprintf(settingsFile, "scale3x\n");;
	if (4 == screenscale)
		fprintf(settingsFile, "scale4x\n");;
	if (holdoff)
		fprintf(settingsFile, "holdoff\n");;
	if (grayblocks)
		fprintf(settingsFile, "grayblocks\n");;
	if (ghostoff)
		fprintf(settingsFile, "ghostoff\n");;
	if (debug)
		fprintf(settingsFile, "debug\n");;
	fprintf(settingsFile, "startlevel %d\n", startlevel);
	fprintf(settingsFile, "nextblocks %d\n", nextblocks);
	fprintf(settingsFile, "rng %s\n", getRandomizerString());
	
	fclose(settingsFile);
}
