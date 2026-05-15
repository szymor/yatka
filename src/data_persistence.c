#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL/SDL_mixer.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cjson/cJSON.h>

#include "main.h"
#include "state_mainmenu.h"
#include "sound.h"
#include "video.h"
#include "data_persistence.h"
#include "randomizer.h"

#define GAMEDIR			".yatka"
#define CONFIG_FILE		"config.json"

char dirpath[256];
static char config_path[256];
static int records[RT_END] = { 0 };

bool settings_changed = false;

static void createGameDir(void)
{
#ifdef __MINGW32__
	mkdir(dirpath);
#else
	mkdir(dirpath, 0744);
#endif
}

void initPaths(void)
{
	const char *home = getenv("HOME");
	sprintf(dirpath, "%s/" GAMEDIR, home);
	sprintf(config_path, "%.240s/" CONFIG_FILE, dirpath);
}

int getRecord(enum RecordType rt)
{
	return records[rt];
}

void setRecord(enum RecordType rt, int record)
{
	records[rt] = record;
	settings_changed = true;
}

/* ───────── internal: full config load / save ───────── */

static void saveConfig(void)
{
	createGameDir();
	FILE *f = fopen(config_path, "w");
	if (!f)
		return;

	cJSON *root = cJSON_CreateObject();

	cJSON_AddNumberToObject(root, "version", 1);

	/* settings */
	cJSON *settings = cJSON_CreateObject();
	cJSON_AddBoolToObject(settings, "nosound", nosound);
	cJSON_AddBoolToObject(settings, "smoothanim", smoothanim);
	cJSON_AddBoolToObject(settings, "easyspin", easyspin);
	cJSON_AddBoolToObject(settings, "lockdelay", lockdelay);
	cJSON_AddBoolToObject(settings, "sonicdrop", sonicdrop);
	cJSON_AddBoolToObject(settings, "repeattrack", repeattrack);
	cJSON_AddBoolToObject(settings, "speechon", speechon);
	cJSON_AddNumberToObject(settings, "screenscale", screenscale);
	if (!nosound)
		cJSON_AddNumberToObject(settings, "musicvol", Mix_VolumeMusic(-1));
	cJSON_AddNumberToObject(settings, "tetrominocolor", (int)tetrominocolor);
	cJSON_AddStringToObject(settings, "rng", getRandomizerString());
	cJSON_AddItemToObject(root, "settings", settings);

	/* key bindings */
	cJSON *keys = cJSON_CreateObject();
	cJSON_AddNumberToObject(keys, "left", kleft);
	cJSON_AddNumberToObject(keys, "right", kright);
	cJSON_AddNumberToObject(keys, "softdrop", ksoftdrop);
	cJSON_AddNumberToObject(keys, "harddrop", kharddrop);
	cJSON_AddNumberToObject(keys, "rotatecw", krotatecw);
	cJSON_AddNumberToObject(keys, "rotateccw", krotateccw);
	cJSON_AddNumberToObject(keys, "hold", khold);
	cJSON_AddNumberToObject(keys, "pause", kpause);
	cJSON_AddNumberToObject(keys, "quit", kquit);
	cJSON_AddItemToObject(root, "keys", keys);

	/* records */
	cJSON *records_obj = cJSON_CreateObject();
	cJSON_AddNumberToObject(records_obj, "marathon_score", records[RT_MARATHON_SCORE]);
	cJSON_AddNumberToObject(records_obj, "marathon_lines", records[RT_MARATHON_LINES]);
	cJSON_AddNumberToObject(records_obj, "sprint_time", records[RT_SPRINT_TIME]);
	cJSON_AddNumberToObject(records_obj, "ultra_score", records[RT_ULTRA_SCORE]);
	cJSON_AddNumberToObject(records_obj, "ultra_lines", records[RT_ULTRA_LINES]);
	cJSON_AddItemToObject(root, "records", records_obj);

	char *json = cJSON_Print(root);
	fprintf(f, "%s\n", json);
	free(json);
	cJSON_Delete(root);
	fclose(f);
}

static void loadConfig(void)
{
	FILE *f = fopen(config_path, "r");
	if (!f)
		return;

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	rewind(f);

	char *data = malloc((size_t)len + 1);
	if (!data)
	{
		fclose(f);
		return;
	}
	size_t rlen = fread(data, 1, (size_t)len, f);
	data[rlen] = '\0';
	fclose(f);

	cJSON *root = cJSON_Parse(data);
	free(data);
	if (!root)
		return;

	cJSON *item;

	/* settings */
	cJSON *settings = cJSON_GetObjectItem(root, "settings");
	if (cJSON_IsObject(settings))
	{
		item = cJSON_GetObjectItem(settings, "nosound");
		if (cJSON_IsBool(item)) nosound = item->valueint;

		item = cJSON_GetObjectItem(settings, "smoothanim");
		if (cJSON_IsBool(item)) smoothanim = item->valueint;

		item = cJSON_GetObjectItem(settings, "easyspin");
		if (cJSON_IsBool(item)) easyspin = item->valueint;

		item = cJSON_GetObjectItem(settings, "lockdelay");
		if (cJSON_IsBool(item)) lockdelay = item->valueint;

		item = cJSON_GetObjectItem(settings, "sonicdrop");
		if (cJSON_IsBool(item)) sonicdrop = item->valueint;

		item = cJSON_GetObjectItem(settings, "repeattrack");
		if (cJSON_IsBool(item)) repeattrack = item->valueint;

		item = cJSON_GetObjectItem(settings, "speechon");
		if (cJSON_IsBool(item)) speechon = item->valueint;

		item = cJSON_GetObjectItem(settings, "screenscale");
		if (cJSON_IsNumber(item)) screenscale = item->valueint;

		item = cJSON_GetObjectItem(settings, "musicvol");
		if (cJSON_IsNumber(item)) initmusvol = item->valueint;

		item = cJSON_GetObjectItem(settings, "tetrominocolor");
		if (cJSON_IsNumber(item)) tetrominocolor = (enum TetrominoColor)item->valueint;

		item = cJSON_GetObjectItem(settings, "rng");
		if (cJSON_IsString(item))
		{
			for (int i = 0; i < RA_END; ++i)
			{
				randomalgo = i;
				if (!strcmp(item->valuestring, getRandomizerString()))
					break;
			}
		}
	}

	/* key bindings */
	cJSON *keys = cJSON_GetObjectItem(root, "keys");
	if (cJSON_IsObject(keys))
	{
		item = cJSON_GetObjectItem(keys, "left");
		if (cJSON_IsNumber(item)) kleft = item->valueint;

		item = cJSON_GetObjectItem(keys, "right");
		if (cJSON_IsNumber(item)) kright = item->valueint;

		item = cJSON_GetObjectItem(keys, "softdrop");
		if (cJSON_IsNumber(item)) ksoftdrop = item->valueint;

		item = cJSON_GetObjectItem(keys, "harddrop");
		if (cJSON_IsNumber(item)) kharddrop = item->valueint;

		item = cJSON_GetObjectItem(keys, "rotatecw");
		if (cJSON_IsNumber(item)) krotatecw = item->valueint;

		item = cJSON_GetObjectItem(keys, "rotateccw");
		if (cJSON_IsNumber(item)) krotateccw = item->valueint;

		item = cJSON_GetObjectItem(keys, "hold");
		if (cJSON_IsNumber(item)) khold = item->valueint;

		item = cJSON_GetObjectItem(keys, "pause");
		if (cJSON_IsNumber(item)) kpause = item->valueint;

		item = cJSON_GetObjectItem(keys, "quit");
		if (cJSON_IsNumber(item)) kquit = item->valueint;
	}

	/* records */
	cJSON *records_obj = cJSON_GetObjectItem(root, "records");
	if (cJSON_IsObject(records_obj))
	{
		item = cJSON_GetObjectItem(records_obj, "marathon_score");
		if (cJSON_IsNumber(item)) records[RT_MARATHON_SCORE] = item->valueint;

		item = cJSON_GetObjectItem(records_obj, "marathon_lines");
		if (cJSON_IsNumber(item)) records[RT_MARATHON_LINES] = item->valueint;

		item = cJSON_GetObjectItem(records_obj, "sprint_time");
		if (cJSON_IsNumber(item)) records[RT_SPRINT_TIME] = item->valueint;

		item = cJSON_GetObjectItem(records_obj, "ultra_score");
		if (cJSON_IsNumber(item)) records[RT_ULTRA_SCORE] = item->valueint;

		item = cJSON_GetObjectItem(records_obj, "ultra_lines");
		if (cJSON_IsNumber(item)) records[RT_ULTRA_LINES] = item->valueint;
	}

	cJSON_Delete(root);
	settings_changed = false;
}

/* ───────── public API ───────── */

void loadSettings(void)
{
	loadConfig();
}

void saveSettings(void)
{
	saveConfig();
	settings_changed = false;
}

void loadRecords(void)
{
	/* records live inside the same config.json;
	 * loadSettings() already loads them. */
}

void saveRecords(void)
{
	saveConfig();
}
