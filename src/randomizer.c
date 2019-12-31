
#include "randomizer.h"

enum RandomAlgo randomalgo = RA_8BAG;

static enum FigureId getRandomId(void);
static enum FigureId getRandom7BagId(void);
static enum FigureId getRandom8BagId(void);

enum FigureId getNextId(void)
{
	switch (randomalgo)
	{
		case RA_NAIVE:
			return getRandomId();
		case RA_7BAG:
			return getRandom7BagId();
		case RA_8BAG:
			return getRandom8BagId();
		default:
			// should never happen
			return FIGID_I;
	}
}

static enum FigureId getRandomId(void)
{
	return rand() % FIGID_END;
}

static enum FigureId getRandom7BagId(void)
{
	static enum FigureId tab[FIGID_END];
	static int pos = FIGID_END;

	if (pos >= FIGID_END)
	{
		pos = 0;
		for (int i = 0; i < FIGID_END; ++i)
			tab[i] = i;

		for (int i = 0; i < 1000; ++i)
		{
			int one = rand() % FIGID_END;
			int two = rand() % FIGID_END;
			int temp = tab[one];
			tab[one] = tab[two];
			tab[two] = temp;
		}
	}

	return tab[pos++];
}

static enum FigureId getRandom8BagId(void)
{
	static enum FigureId tab[MAX8BAGID];
	static int pos = MAX8BAGID;

	if (pos >= MAX8BAGID)
	{
		pos = 0;
		for (int i = 0; i < FIGID_END; ++i)
			tab[i] = i;
		for (int i = FIGID_END; i < MAX8BAGID; ++i)
			tab[i] = rand() % FIGID_END;

		for (int i = 0; i < 1000; ++i)
		{
			int one = rand() % MAX8BAGID;
			int two = rand() % MAX8BAGID;
			int temp = tab[one];
			tab[one] = tab[two];
			tab[two] = temp;
		}
	}

	return tab[pos++];
}
