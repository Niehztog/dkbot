#ifndef DKBOT_H
#define DKBOT_H

#include "dk/dk_boundary.h"
#include "dk/botlib_abi.h"

#define DK_MAX_BOTS 32
/* The engine's maxclients ceiling. */
#define DK_MAX_CLIENTS 256

/* NULL where this build lacks the symbol: check before chaining. */
struct dk_originals {
	int  (*dll_Entry)(HINSTANCE, DWORD, PVOID);
	int  (*dll_ClientConnect)(userEntity_t *, void *, int);
	void (*dll_ClientDisconnect)(userEntity_t *);
	void (*dll_ClientBeginServerFrame)(userEntity_t *);
	void (*Client_Think)(edict_t *, usercmd_t *);
	void (*P_RunFrame)(void);
};

struct dk_globals {
	void *gi;
	dk_game_export_t *ge;
	dk_cvar_t **maxclients;
	void *ss;
	void *sv;
	void *svs;
	void **com;
};

extern struct dk_originals dk_orig;
extern struct dk_globals   dk_glob;

void *dk_sym(const char *name);
int   dk_engine_init(void);
void  dk_log(const char *fmt, ...);
void  dk_con_printf(const char *fmt, ...);

int   dkbot_install_hooks(void);
unsigned long dkbot_frame_count(void);

void  dkbot_arm_client_buffers(void);
void  dkbot_release_client_slot(const edict_t *ent);
void  dkbot_set_ping(int bot, int ms);

int   dkbot_redirect_args(void);
void  dkbot_record_commands(void);
void  dkbot_register_console(void);
int   dkbot_client_command_argv(edict_t *ent, const char **argv, int argc);
#define DK_SELECT_OK           0
#define DK_SELECT_RETRY        1
#define DK_SELECT_NOT_CARRIED  2
int   dkbot_select_weapon(edict_t *ent, const char *classname);
const char *dkbot_current_weapon(const edict_t *ent);
const unsigned char *dkbot_player_hook(const edict_t *ent);

void  dkbot_fill_client(const edict_t *ent, dk_bot_updateclient_t *buc);
void  dkbot_fill_entity(const edict_t *ent, dk_bot_updateentity_t *bue);
int   dkbot_entity_number(const edict_t *ent);
void  dk_angle_vectors(const float *angles, float *forward, float *right,
                       float *up);
float dk_dot(const float *a, const float *b);

int   dkbot_botlib_init(void);
void  dkbot_botlib_shutdown(void);
void  dkbot_botlib_shutdown_client(int client);
int   dkbot_botlib_active(void);
/* Why bots cannot play this level, or NULL when they can. */
const char *dkbot_botlib_problem(void);
int   dkbot_botlib_load_map(const char *mapname);
void  dkbot_dump_gamemode(void);
int   dkbot_botlib_setup_client(int client);
void  dkbot_botlib_start_frame(float time);
void  dkbot_botlib_update_client(int client, dk_bot_updateclient_t *buc);
void  dkbot_botlib_ai(int client, float thinktime);
const dk_bot_input_t *dkbot_botlib_input(int client);
void  dkbot_push_entities(void);
/* Area 0 is outside the navigation data. */
int   dkbot_aas_area(const float *origin);
int   dkbot_aas_area_reachable(int area);
float dkbot_server_time(void);
const char *dkbot_map_name(void);

void  dkbot_server_init(void);
void  dkbot_level_load(void);
void  dkbot_level_exit(void);
void  dkbot_think_all(void);
void  dkbot_status(char *buf, unsigned long n);
void  dkbot_request(int n);
const char *dkbot_remove(const char *name);
int   dkbot_count(void);
int   dkbot_bot_slot(int i);
const edict_t *dkbot_bot_edict(int i);
const char *dkbot_bot_name(int i);
int   dkbot_is_bot(const edict_t *ent);
int   dkbot_adding_bot(void);
int   dkbot_release_edict(const edict_t *ent);
void  dkbot_forget_edict(const edict_t *ent);
edict_t *dkbot_client_edict(int client);
int   dkbot_slot_is_firing(int entnum);
void  dkbot_note_weapon_request(const edict_t *ent, const char *classname);

#endif
