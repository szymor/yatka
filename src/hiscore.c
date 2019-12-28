
#include <stdio.h>

#include "hiscore.h"

int loadHiscore(void)
{
	FILE *hifile = fopen("hiscore.dat", "r");
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
	FILE *hifile = fopen("hiscore.dat", "w");
	if (hifile)
	{
		fprintf(hifile, "%d", hi);
		fclose(hifile);
	}
}
