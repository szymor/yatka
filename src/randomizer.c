
#include "randomizer.h"

enum RandomAlgo randomalgo = RA_NAIVE;

static bool firstDeal = true;

static enum FigureId getRandomId(void);
static enum FigureId getRandomId_Nintendo(void);
static enum FigureId getRandomId_TGM98(void);
static enum FigureId getRandomId_TGM3(void);
static enum FigureId getRandom7BagId(void);
static enum FigureId getRandom8BagId(void);
static enum FigureId getRandom14BagId(void);

static char *rng_strings[] = {
	"naive",
	"nintendo",
	"tgm98",
	"tgm3",
	"7bag",
	"8bag",
	"14bag"
};

void randomizer_reset(void)
{
	firstDeal = true;
}

enum FigureId getNextId(void)
{
	switch (randomalgo)
	{
		case RA_NAIVE:
			return getRandomId();
		case RA_NINTENDO:
			return getRandomId_Nintendo();
		case RA_TGM98:
			return getRandomId_TGM98();
		case RA_TGM3:
			return getRandomId_TGM3();
		case RA_7BAG:
			return getRandom7BagId();
		case RA_8BAG:
			return getRandom8BagId();
		case RA_14BAG:
			return getRandom14BagId();
		default:
			// should never happen
			return FIGID_I;
	}
}

static enum FigureId getRandomId(void)
{
	return rand() % FIGID_GRAY;
}

static enum FigureId getRandomId_Nintendo(void)
{
	static enum FigureId last;
	if (firstDeal)
	{
		last = FIGID_GRAY;
		firstDeal = false;
	}

	enum FigureId current = rand() % FIGID_GRAY;
	if (current == last)
		current = rand() % FIGID_GRAY;
	last = current;
	return current;
}

static enum FigureId getRandomId_TGM98(void)
{
	static enum FigureId history[TGM_HISTORYSZ];

	if (firstDeal)
	{
		enum FigureId pieces[4] = { FIGID_I, FIGID_J,
									FIGID_L, FIGID_T };
		history[0] = FIGID_S;
		history[1] = FIGID_Z;
		history[2] = FIGID_S;
		history[3] = pieces[rand() % 4];
		firstDeal = false;
		return history[3];
	}
	else
	{
		enum FigureId piece;
		bool inHistory;
		for (int i = 0; i < 4; ++i)
		{
			piece = rand() % FIGID_GRAY;

			// is the piece in the history?
			inHistory = false;
			for (int j = 0; j < TGM_HISTORYSZ; ++j)
			{
				if (piece == history[j])
				{
					inHistory = true;
					break;
				}
			}

			if (!inHistory)
				break;
		}

		// shift history and place the new piece
		for (int i = 1; i < TGM_HISTORYSZ; ++i)
		{
			history[i - 1] = history[i];
		}
		history[TGM_HISTORYSZ - 1] = piece;

		return piece;
	}
}

static enum FigureId getRandomId_TGM3(void)
{
	static enum FigureId pool[TGM3_POOLSZ];
	static enum FigureId history[TGM_HISTORYSZ];
	static enum FigureId queue[FIGID_GRAY];

	if (firstDeal)
	{
		// init the queue
		for (int i = 0; i < FIGID_GRAY; ++i)
		{
			queue[i] = FIGID_GRAY;
		}

		// init the pool
		for (int j = 0; j < 5; ++j)
		{
			for (int i = 0; i < FIGID_GRAY; ++i)
			{
				pool[j * FIGID_GRAY + i] = i;
			}
		}

		enum FigureId pieces[4] = { FIGID_I, FIGID_J,
									FIGID_L, FIGID_T };
		history[0] = FIGID_S;
		history[1] = FIGID_Z;
		history[2] = FIGID_S;
		history[3] = pieces[rand() % 4];
		firstDeal = false;
		return history[3];
	}
	else
	{
		int index;
		enum FigureId piece;
		bool inHistory;
		for (int i = 0; i < 6; ++i)
		{
			index = rand() % TGM3_POOLSZ;
			piece = pool[index];

			// is the piece in the history?
			inHistory = false;
			for (int j = 0; j < TGM_HISTORYSZ; ++j)
			{
				if (piece == history[j])
				{
					inHistory = true;
					break;
				}
			}

			if (!inHistory || (i == 5))
				break;
			if (queue[0] != FIGID_GRAY)
				pool[index] = queue[0];
		}

		// update the queue
		int freeSlotId = FIGID_GRAY - 1;
		for (int i = 0; i < FIGID_GRAY; ++i)
		{
			if (piece == queue[i])
			{
				// delete the piece
				for (int j = i + 1; j < FIGID_GRAY; ++j)
				{
					queue[j - 1] = queue[j];
				}
			}
			if (queue[i] == FIGID_GRAY && i < freeSlotId)
				freeSlotId = i;
		}
		// add the piece to the end of the queue
		queue[freeSlotId] = piece;
		// add the oldest piece from the queue to the pool
		pool[index] = queue[0];

		// shift history and place the new piece
		for (int i = 1; i < TGM_HISTORYSZ; ++i)
		{
			history[i - 1] = history[i];
		}
		history[TGM_HISTORYSZ - 1] = piece;

		return piece;
	}
}

static enum FigureId getRandom7BagId(void)
{
	static enum FigureId tab[FIGID_GRAY];
	static int pos;
	if (firstDeal)
	{
		pos = FIGID_GRAY;
		firstDeal = false;
	}

	if (pos >= FIGID_GRAY)
	{
		pos = 0;
		for (int i = 0; i < FIGID_GRAY; ++i)
			tab[i] = i;

		for (int i = 0; i < 1000; ++i)
		{
			int one = rand() % FIGID_GRAY;
			int two = rand() % FIGID_GRAY;
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
	static int pos;
	if (firstDeal)
	{
		pos = MAX8BAGID;
		firstDeal = false;
	}

	if (pos >= MAX8BAGID)
	{
		pos = 0;
		for (int i = 0; i < FIGID_GRAY; ++i)
			tab[i] = i;
		for (int i = FIGID_GRAY; i < MAX8BAGID; ++i)
			tab[i] = rand() % FIGID_GRAY;

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

static enum FigureId getRandom14BagId(void)
{
	static enum FigureId tab[MAX14BAGID];
	static int pos;
	if (firstDeal)
	{
		pos = MAX14BAGID;
		firstDeal = false;
	}

	if (pos >= MAX14BAGID)
	{
		pos = 0;

		// fill the bag
		for (int i = 0; i < FIGID_GRAY; ++i)
		{
			tab[i] = i;
			tab[i + FIGID_GRAY] = i;
		}

		// shuffle
		for (int i = 0; i < 1000; ++i)
		{
			int one = rand() % MAX14BAGID;
			int two = rand() % MAX14BAGID;
			int temp = tab[one];
			tab[one] = tab[two];
			tab[two] = temp;
		}
	}

	return tab[pos++];
}

char *getRandomizerString(void)
{
	return rng_strings[randomalgo];
}
