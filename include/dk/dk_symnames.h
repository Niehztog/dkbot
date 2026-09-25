/* Linux names, macros only: dkbot/dk_win.c maps each to its PDB name beside <windows.h>. */
#ifndef DK_SYMNAMES_H
#define DK_SYMNAMES_H

#define DK_SYM_CLIENT_THINK    "_Z12Client_ThinkP7edict_sP9usercmd_s"
#define DK_SYM_RUN_FRAME       "_Z10P_RunFramev"
/* Never gi.cvar(): it creates the cvar, and an empty teamplay aborts the game. */
#define DK_SYM_CVAR_STRING     "_Z19Cvar_VariableStringPKc"
#define DK_SYM_CMD_ADDCOMMAND  "_Z14Cmd_AddCommandPKcPFvvE"
#define DK_SYM_GET_PLAYER_HOOK "_Z15P_GetPlayerHookP7edict_s"
/* Weapons switch by InventoryFindItem + this; cycling curItem does nothing. */
#define DK_SYM_WEAPON_SELECT   "_Z12weaponSelectP7edict_sPK12weaponInfo_s"

#endif
