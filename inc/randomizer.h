#ifndef _H_RANDOMIZER
#define _H_RANDOMIZER

#include "main.h"

#define MAX8BAGID		8
#define MAX14BAGID		14

enum RandomAlgo
{
	RA_NAIVE,
	RA_NINTENDO,
	RA_TGM98,
	RA_7BAG,
	RA_8BAG,
	RA_14BAG,
	RA_END
};

void randomizer_reset(void);
enum FigureId getNextId(void);
char *getRandomizerString(void);

extern enum RandomAlgo randomalgo;

#endif
