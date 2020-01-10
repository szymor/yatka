#ifndef _H_RANDOMIZER
#define _H_RANDOMIZER

#include "main.h"

#define MAX8BAGID		8

enum RandomAlgo
{
	RA_NAIVE,
	RA_7BAG,
	RA_8BAG,
	RA_END
};

enum FigureId getNextId(void);
char *getRandomizerString(void);

extern enum RandomAlgo randomalgo;

#endif
