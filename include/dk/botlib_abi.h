#ifndef DK_BOTLIB_ABI_H
#define DK_BOTLIB_ABI_H

#include <stddef.h>

typedef float dk_vec3_t[3];

#define DK_BL_MAX_ITEMS          256

#define DK_INVENTORY_HEALTH            41

#define DK_PRT_MESSAGE 1

#define DK_ACTION_ATTACK       1
#define DK_ACTION_USE          2
#define DK_ACTION_RESPAWN      4
#define DK_ACTION_JUMP         8
#define DK_ACTION_CROUCH       16
#define DK_ACTION_MOVEFORWARD  32
#define DK_ACTION_MOVEBACK     64
#define DK_ACTION_MOVELEFT     128
#define DK_ACTION_MOVERIGHT    256
#define DK_ACTION_DELAYEDJUMP  512

#define DK_BLERR_NOERROR              0
#define DK_BLERR_NOAASFILE            5
#define DK_BLERR_NOBSPFILE            12

typedef struct dk_cplane_s {
	dk_vec3_t     normal;
	float         dist;
	unsigned char type;
	unsigned char signbits;
	unsigned char pad[2];
} dk_cplane_t;

typedef struct dk_bsp_surface_s {
	char name[16];
	int  flags;
	int  value;
} dk_bsp_surface_t;

typedef struct dk_bsp_trace_s {
	int              allsolid;
	int              startsolid;
	float            fraction;
	dk_vec3_t        endpos;
	dk_cplane_t      plane;
	float            exp_dist;
	int              sidenum;
	dk_bsp_surface_t surface;
	int              contents;
	int              ent;
} dk_bsp_trace_t;

typedef struct dk_bot_settings_s {
	char characterfile[144];
	char charactername[144];
	char ailibrary[144];
} dk_bot_settings_t;

typedef struct dk_bot_clientsettings_s {
	char netname[16];
	char skin[128];
} dk_bot_clientsettings_t;

typedef struct dk_bot_input_s {
	float     thinktime;
	dk_vec3_t dir;
	float     speed;
	dk_vec3_t viewangles;
	int       actionflags;
} dk_bot_input_t;

typedef struct dk_bot_updateclient_s {
	int           pm_type;
	dk_vec3_t     origin;
	dk_vec3_t     velocity;
	unsigned char pm_flags;
	unsigned char pm_time;
	float         gravity;
	dk_vec3_t     delta_angles;
	dk_vec3_t     viewangles;
	dk_vec3_t     viewoffset;
	dk_vec3_t     kick_angles;
	dk_vec3_t     gunangles;
	dk_vec3_t     gunoffset;
	int           gunindex;
	int           gunframe;
	float         blend[4];
	float         fov;
	int           rdflags;
	short         stats[32];
	int           inventory[DK_BL_MAX_ITEMS];
} dk_bot_updateclient_t;

typedef struct dk_bot_updateentity_s {
	dk_vec3_t origin;
	dk_vec3_t angles;
	dk_vec3_t old_origin;
	dk_vec3_t mins;
	dk_vec3_t maxs;
	int       solid;
	int       modelindex;
	int       modelindex2, modelindex3, modelindex4;
	int       frame;
	int       skinnum;
	int       effects;
	int       renderfx;
	int       sound;
	int       event;
} dk_bot_updateentity_t;

typedef struct dk_bot_export_s {
	char *(*BotVersion)(void);
	int (*BotSetupLibrary)(void);
	int (*BotShutdownLibrary)(void);
	int (*BotLibraryInitialized)(void);
	int (*BotLibVarSet)(char *var_name, char *value);
	int (*BotDefine)(char *string);
	int (*BotLoadMap)(char *mapname, int modelindexes, char *modelindex[],
	                  int soundindexes, char *soundindex[],
	                  int imageindexes, char *imageindex[]);
	int (*BotSetupClient)(int client, dk_bot_settings_t *settings);
	int (*BotShutdownClient)(int client);
	int (*BotMoveClient)(int oldclnum, int newclnum);
	int (*BotClientSettings)(int client, dk_bot_clientsettings_t *settings);
	int (*BotSettings)(int client, dk_bot_settings_t *settings);
	int (*BotStartFrame)(float time);
	int (*BotUpdateClient)(int client, dk_bot_updateclient_t *buc);
	int (*BotUpdateEntity)(int ent, dk_bot_updateentity_t *bue);
	int (*BotAddSound)(dk_vec3_t origin, int ent, int channel, int soundindex,
	                   float volume, float attenuation, float timeofs);
	int (*BotAddPointLight)(dk_vec3_t origin, int ent, float radius, float r,
	                        float g, float b, float time, float decay);
	int (*BotAI)(int client, float thinktime);
	int (*BotConsoleMessage)(int client, int type, char *message);
	int (*Test)(int parm0, char *parm1, dk_vec3_t parm2, dk_vec3_t parm3);
} dk_bot_export_t;

typedef struct dk_bot_import_s {
	void (*BotInput)(int client, dk_bot_input_t *bi);
	void (*BotClientCommand)(int client, char *str, ...);
	void (*Print)(int type, char *fmt, ...);
	dk_bsp_trace_t (*Trace)(dk_vec3_t start, dk_vec3_t mins, dk_vec3_t maxs,
	                        dk_vec3_t end, int passent, int contentmask);
	int (*PointContents)(dk_vec3_t point);
	void *(*GetMemory)(int size);
	void (*FreeMemory)(void *ptr);
	int (*DebugLineCreate)(void);
	void (*DebugLineDelete)(int line);
	void (*DebugLineShow)(int line, dk_vec3_t start, dk_vec3_t end, int color);
} dk_bot_import_t;

typedef dk_bot_export_t *(*dk_fn_GetBotAPI)(dk_bot_import_t *import);

/* Sizes from gladiator-bot-restored/game/botlib.h on x86-64: fix the mirror, not the number. */
_Static_assert(sizeof(dk_bot_input_t) == 36, "bot_input_t drifted");
_Static_assert(sizeof(dk_bot_updateclient_t) == 1228, "bot_updateclient_t drifted");
_Static_assert(sizeof(dk_bot_updateentity_t) == 104, "bot_updateentity_t drifted");
_Static_assert(sizeof(dk_bot_settings_t) == 432, "bot_settings_t drifted");
_Static_assert(sizeof(dk_bot_clientsettings_t) == 144, "bot_clientsettings_t drifted");
_Static_assert(sizeof(dk_bot_export_t) == 160, "bot_export_t drifted");
_Static_assert(sizeof(dk_bot_import_t) == 80, "bot_import_t drifted");
_Static_assert(sizeof(dk_bsp_trace_t) == 84, "bsp_trace_t drifted");

#endif
