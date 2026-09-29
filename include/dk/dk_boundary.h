/* Daikatana 1.3 engine ABI; make check-boundary and check-pdb verify it. */
#ifndef DK_BOUNDARY_H
#define DK_BOUNDARY_H

#include <stdint.h>

#include "dk/dk_symnames.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int qboolean;
#ifndef _WINDOWS_
typedef void *HINSTANCE;
typedef unsigned int DWORD;
typedef void *PVOID;
typedef unsigned char byte;
#endif

typedef struct userEntity_s userEntity_t;
typedef struct edict_s      edict_t;
typedef struct pmove_s      pmove_t;
typedef struct teamInfo_s   teamInfo_t;

typedef struct usercmd_s {
	byte  msec;
	byte  buttons;
	short angles[3];
	short forwardmove;
	short sidemove;
	short upmove;
	byte  impulse;
	byte  lightlevel;
} usercmd_t;

#define DK_BUTTON_ATTACK        1
#define DK_BUTTON_USE           2

#define DK_ANGLE2SHORT(x) ((short)(int)((x) * 65536.0f / 360.0f))
#define DK_SHORT2ANGLE(x) ((float)(x) * (360.0f / 65536.0f))

enum {
	DK_MSG_VERSION_CHECK = 1,
	DK_MSG_CLEAR_PTR     = 2,
	DK_MSG_DESCRIPTION   = 3,
	DK_MSG_SERVER_INIT   = 10,
	DK_MSG_SERVER_KILL   = 11,
	DK_MSG_SERVER_LOAD   = 12,
	DK_MSG_LEVEL_LOAD    = 20,
	DK_MSG_LEVEL_EXIT    = 21,
	DK_MSG_NODES_SAVE    = 22,
	DK_MSG_NODES_LOAD    = 23,
	DK_MSG_MAX           = 23
};

#define DK_WORLD_API_VERSION 0xBA0

/* These and the *_dll_Entry points are all a slot-0 module can override. */
void      dll_ClientThink(userEntity_t *self, usercmd_t *ucmd, pmove_t *pm);
int       dll_ClientConnect(userEntity_t *self, void *userinfo, int loadgame);
void      dll_ClientBegin(userEntity_t *self, int loadgame);
void      dll_ClientUserinfoChanged(edict_t *ent, char *userinfo);
void      dll_ClientDisconnect(userEntity_t *self);
void      dll_ClientBeginServerFrame(userEntity_t *self);
void      dll_SetStats(userEntity_t *ent);
void      dll_Client_InitAttributes(userEntity_t *self);
void      dll_BeginIntermission(const char *nextMap);
void      dll_LoadNodes(const char *pMapName);
void      dll_EntityLoadCleanup(int nIndex);
void      dll_RegisterWorldFuncs(void);
int       dll_FLAG_GetScores(teamInfo_t *scorearr, int maxscores);
qboolean  dll_FLAG_CheckRules(void);
short     dll_DT_CanDamage(userEntity_t *self, userEntity_t *attacker,
                           unsigned int damage_flags);
void      SIDEKICK_Alert(userEntity_t *owner, userEntity_t *target);
void      ShowBoundingBoxes(userEntity_t *self);

int dll_Entry(HINSTANCE hParent, DWORD dwReasonForCall, PVOID pvData);
int weapons_dll_Entry(HINSTANCE hParent, DWORD dwReasonForCall, PVOID pvData);
int gce_dll_Entry(HINSTANCE hParent, DWORD dwReasonForCall, PVOID pvData);

/* GetGameAPI fills `globals` after dll_Entry: patch it from a later hook. */
typedef struct dk_game_export_s {
	int  apiversion;
	void (*SetServerTime)(float);
	void (*SpawnEntities)(const char *, char *, qboolean);
	void (*WriteGame)(const char *, char);
	void (*ReadGame)(const char *);
	void (*WriteHeader)(const char *, const char *, qboolean);
	void (*WriteLevel)(const char *);
	void (*ReadLevel)(const char *);
	qboolean (*ClientConnect)(edict_t *, char *, qboolean);
	void (*ClientBegin)(edict_t *, qboolean);
	void (*ClientUserinfoChanged)(edict_t *, char *);
	void (*ClientDisconnect)(edict_t *);
	void (*ClientCommand)(edict_t *);
	void (*ClientThink)(edict_t *, usercmd_t *);
	void (*RunFrame)(void);
	void (*RegisterFunc)(const char *, void *);
	void (*ServerCommand)(void);
	edict_t *edicts;
	int  edict_size;
	int  num_edicts;
	int  max_edicts;
	unsigned char (*CanSave)(edict_t *, unsigned char);
	int  endIntermission;
	void (*LevelLoad)(void);
	void (*LevelExit)(void);
	void (*InitDLLs)(void);
	void (*UnloadDLLs)(void);
	void (*InitChangelevel)(void);
	void (*LoadNodes)(const char *);
	void (*RegisterWorldFuncs)(void);
	void (*EntityLoadCleanup)(int);
} dk_game_export_t;

typedef struct dk_cvar_s {
	char  *name;
	char  *string;
	char  *latched_string;
	int    flags;
	qboolean modified;
	float  value;
	int    intValue;
	char  *defaultValue;
	char  *description;
	int    defaultFlags;
	struct dk_cvar_s *next;
} dk_cvar_t;

/* Edicts are ge->edict_size apart; no sizeof matches the game's entity. */
#define DK_EDICT_OFF_ORIGIN     0x004
#define DK_EDICT_OFF_ANGLES     0x010
#define DK_EDICT_OFF_CLIENT     0x0e8
#define DK_EDICT_OFF_INUSE      0x0f0
/* absmin/absmax are the link box: the bounding box plus one unit per side. */
#define DK_EDICT_OFF_ABSMIN     0x15c
#define DK_EDICT_OFF_ABSMAX     0x168
#define DK_EDICT_OFF_SOLID      0x180
#define DK_EDICT_OFF_CLASSNAME  0x190

#define DK_ES_OFF_OLD_ORIGIN    0x01c
#define DK_ES_OFF_MODELINDEX    0x034
#define DK_ES_OFF_MODELINDEX2   0x038
#define DK_ES_OFF_FRAME         0x044
#define DK_ES_OFF_SKINNUM       0x048
#define DK_ES_OFF_EFFECTS       0x04c
#define DK_ES_OFF_RENDERFX      0x054

#define DK_UENT_OFF_RECORD      0x198
#define DK_UENT_OFF_INVENTORY   0x1f0
#define DK_UENT_OFF_VELOCITY    0x218
#define DK_UENT_OFF_ARMOR_VAL   0x28c
#define DK_UENT_OFF_HEALTH      0x294
#define DK_UENT_OFF_DEADFLAG    0x2a4
#define DK_UENT_OFF_TEAM        0x2d4
#define DK_UENT_OFF_WATERLEVEL  0x2dc
#define DK_UENT_OFF_CURWEAPON   0x368

/* player_record_t: the engine writes frags only, never deaths. */
#define DK_REC_OFF_FRAGS        0x000

#define DK_INVLIST_OFF_HEAD       0x000
#define DK_INVITEM_OFF_DATA       0x000
#define DK_INVITEM_OFF_NEXT       0x010
#define DK_INVDEF_OFF_NAME        0x000
/* userInventory_s.modelName is char[MAX_PATH]: Linux 4096, Windows 260. */
#ifdef _WIN32
#define DK_INVDEF_OFF_FLAGS       0x120
#define DK_INVDEF_OFF_SIZE        0x130
#define DK_INVDEF_OFF_AMMO_COUNT  0x140
#define DK_INVDEF_OFF_WINFO       0x148
#define DK_INVDEF_AMMO_SIZE       0x150
#else
#define DK_INVDEF_OFF_FLAGS       0x101c
#define DK_INVDEF_OFF_SIZE        0x1028
#define DK_INVDEF_OFF_AMMO_COUNT  0x1038
#define DK_INVDEF_OFF_WINFO       0x1040
#define DK_INVDEF_AMMO_SIZE       0x1048
#endif
#define DK_INVDEF_FLAG_AMMO       0x20000
/* weaponInfo_s; common_export_s (com) */
#define DK_WINFO_OFF_SELECT_FUNC  0x3a0
#define DK_COM_OFF_FIND_REGISTERED_WEAPON 0x218

#define DK_HOOK_OFF_INVULN_TIME   0x064
#define DK_HOOK_OFF_ENVIRO_TIME   0x068
#define DK_HOOK_OFF_OXYLUNG_TIME  0x070
#define DK_HOOK_OFF_POWER_BOOST   0x0a4
#define DK_HOOK_OFF_ATTACK_BOOST  0x0a8
#define DK_HOOK_OFF_SPEED_BOOST   0x0ac
#define DK_HOOK_OFF_ACRO_BOOST    0x0b0
#define DK_HOOK_OFF_VITA_BOOST    0x0b4
#define DK_HOOK_OFF_FX_FRAME_FUNC 0x128

/* gclient_s */
#define DK_CLIENT_OFF_PS        0x000
/* Past pers, whose saved inventory holds char[MAX_PATH] names. */
#ifdef _WIN32
#define DK_CLIENT_OFF_VERIFIED_BOT 0x4b04
#else
#define DK_CLIENT_OFF_VERIFIED_BOT 0x2a264
#endif

#define DK_PS_OFF_PMOVE         0x000
#define DK_PS_OFF_VIEWANGLES    0x024
#define DK_PS_OFF_VIEWOFFSET    0x030
#define DK_PS_OFF_KICKANGLES    0x03c
#define DK_PS_OFF_FOV           0x060
#define DK_PS_OFF_RDFLAGS       0x064

#define DK_PM_OFF_TYPE          0x000
#define DK_PM_OFF_FLAGS         0x016
#define DK_PM_OFF_TIME          0x018
#define DK_PM_OFF_GRAVITY       0x01a
#define DK_PM_OFF_DELTA_ANGLES  0x01c

#define DK_SV_OFF_CONFIGSTRINGS     0x2058
#define DK_CONFIGSTRING_SIZE        128
#define DK_CS_MODELS                32
#define DK_MAX_MODELS               1024

/* server_static_t (svs) and its client_t slots */
#define DK_SVS_OFF_REALTIME         0x0004
#define DK_SVS_OFF_CLIENTS          0x0210
#define DK_CLIENT_SIZE              0x22558
#define DK_CLIENT_OFF_STATE         0x0000
#define DK_CLIENT_OFF_PING          0x045c
#define DK_CLIENT_OFF_EDICT         0x0490
#define DK_CLIENT_OFF_NAME          0x0498
#define DK_CLIENT_NAME_SIZE         17
#define DK_CLIENT_OFF_DATAGRAM      0x0538
#define DK_CLIENT_OFF_DATAGRAM_BUF  0x0558
#define DK_CLIENT_DATAGRAM_BUF_SIZE 0xaf00
#define DK_CLIENT_OFF_LASTMESSAGE   0xc6e8
#define DK_CLIENT_OFF_IDLETIME      0xc6f4
#define DK_CLIENT_OFF_NETCHAN       0xc710
#define DK_NETCHAN_OFF_REMOTEADDR   0x0014
#define DK_NETCHAN_OFF_QPORT        0x0020
#define DK_NETCHAN_OFF_MESSAGE      0x0040
#define DK_NETCHAN_OFF_MESSAGE_BUF  0x0060
#define DK_NETCHAN_MESSAGE_BUF_SIZE 0xaef0

#define DK_NETADR_OFF_TYPE  0x00
#define DK_NETADR_OFF_IP    0x04
#define DK_NETADR_OFF_PORT  0x08
#define DK_NA_LOOPBACK   0
#define DK_NA_IP         2

#define DK_CS_FREE       0
#define DK_CS_SPAWNED    3

#define DK_SIZEBUF_OFF_ALLOWOVERFLOW 0x00
#define DK_SIZEBUF_OFF_OVERFLOWED    0x04
#define DK_SIZEBUF_OFF_DATA          0x08
#define DK_SIZEBUF_OFF_MAXSIZE       0x10
#define DK_SIZEBUF_OFF_CURSIZE       0x14
#define DK_SIZEBUF_OFF_READCOUNT     0x18

typedef struct dk_eng_cplane_s {
	float          normal[3];
	float          dist;
	unsigned char  type;
	unsigned char  signbits;
	unsigned char  pad[2];
	unsigned short planeIndex;
	unsigned char  tailpad[2];
} dk_eng_cplane_t;

typedef struct dk_eng_csurface_s {
	char name[16];
	int  flags;
	int  value;
	int  index;
	unsigned short color;
	unsigned char  tailpad[2];
} dk_eng_csurface_t;

typedef struct dk_trace_s {
	int            allsolid;
	int            startsolid;
	float          fraction;
	float          endpos[3];
	dk_eng_cplane_t    plane;
	dk_eng_csurface_t *surface;
	int            contents;
	void          *ent;
} dk_trace_t;

#define DK_ASSERT_OFF(type, member, off) \
	_Static_assert(__builtin_offsetof(type, member) == (off), \
	               #type "." #member " moved")
DK_ASSERT_OFF(dk_eng_cplane_t, normal,   0x00);
DK_ASSERT_OFF(dk_eng_cplane_t, dist,     0x0c);
DK_ASSERT_OFF(dk_eng_cplane_t, type,     0x10);
DK_ASSERT_OFF(dk_eng_cplane_t, signbits, 0x11);
DK_ASSERT_OFF(dk_eng_cplane_t, planeIndex, 0x14);
_Static_assert(sizeof(dk_eng_cplane_t) == 0x18, "cplane_s is 0x18 in the DWARF");
DK_ASSERT_OFF(dk_eng_csurface_t, name,  0x00);
DK_ASSERT_OFF(dk_eng_csurface_t, flags, 0x10);
DK_ASSERT_OFF(dk_eng_csurface_t, value, 0x14);
DK_ASSERT_OFF(dk_eng_csurface_t, index, 0x18);
DK_ASSERT_OFF(dk_eng_csurface_t, color, 0x1c);
_Static_assert(sizeof(dk_eng_csurface_t) == 0x20, "csurface_s is 0x20 in the DWARF");
DK_ASSERT_OFF(dk_trace_t, allsolid,   0x00);
DK_ASSERT_OFF(dk_trace_t, startsolid, 0x04);
DK_ASSERT_OFF(dk_trace_t, fraction,   0x08);
DK_ASSERT_OFF(dk_trace_t, endpos,     0x0c);
DK_ASSERT_OFF(dk_trace_t, plane,      0x18);
DK_ASSERT_OFF(dk_trace_t, surface,    0x30);
DK_ASSERT_OFF(dk_trace_t, contents,   0x38);
DK_ASSERT_OFF(dk_trace_t, ent,        0x40);
_Static_assert(sizeof(dk_trace_t) == 0x48, "trace_t is 0x48 in the DWARF");

/* The float * parameters are CVector references: never pass NULL. */
typedef dk_trace_t (*dk_fn_tracebox)(const float *start, const float *mins,
                                     const float *maxs, const float *end,
                                     void *passent, int contentmask);
typedef int (*dk_fn_pointcontents)(const float *point);
typedef int (*dk_fn_fs_loadfile)(const char *path, void **buffer);
typedef void (*dk_fn_fs_freefile)(void *buffer);

#define DK_GI_OFF_CON_PRINTF       0x010
#define DK_GI_OFF_TRACEBOX         0x0b8
#define DK_GI_OFF_POINTCONTENTS    0x0c0
#define DK_GI_OFF_GETARGC          0x1b8
#define DK_GI_OFF_GETARGV          0x1c0
#define DK_GI_OFF_GETARGS          0x1c8
#define DK_GI_OFF_ADDCOMMAND       0x1f0

/* serverState_t (gstate) */
#define DK_SS_OFF_TIME            0x01c
#define DK_SS_OFF_MAPNAME         0x028
#define DK_SS_OFF_ADDCOMMAND      0x250
#define DK_SS_OFF_INVFINDITEM     0x308
#define DK_SS_OFF_GETARGV         0x3b0
#define DK_SS_OFF_GETARGC         0x3b8
#define DK_SS_OFF_FS_LOADFILE     0x5a8
#define DK_SS_OFF_FS_FREEFILE     0x5b0

typedef void (*dk_fn_conprintf)(const char *, ...);
typedef void (*dk_fn_cmdhandler)(edict_t *);
typedef void (*dk_fn_addcommand)(const char *, dk_fn_cmdhandler);
typedef int  (*dk_fn_argc)(void);
typedef const char *(*dk_fn_argv)(int);
typedef const char *(*dk_fn_args)(void);
typedef void *(*dk_fn_invfind)(void *, const char *);
typedef void (*dk_fn_weaponselect)(edict_t *, const void *);
typedef short (*dk_fn_select)(edict_t *);
typedef const void *(*dk_fn_findweapon)(const char *);

#define DK_AT(base, off, type) (*(type *)((unsigned char *)(base) + (off)))

#ifdef __cplusplus
}
#endif
#endif
