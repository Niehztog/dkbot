/* Overrides of the engine's boundary functions; each chains to the original. */
#include "dkbot.h"

int dll_Entry(HINSTANCE hParent, DWORD dwReasonForCall, PVOID pvData)
{
	int rc;

	if (dk_engine_init() != 0)
		return 0;
	/* Before the original: its dll_ServerLoad registers the game's commands through gstate. */
	if (dwReasonForCall == DK_MSG_SERVER_LOAD) {
		dk_glob.ss = pvData;
		dkbot_record_commands();
	}
	rc = dk_orig.dll_Entry(hParent, dwReasonForCall, pvData);
	switch (dwReasonForCall) {
	case DK_MSG_SERVER_LOAD:
		/* gi is set from here */
		dkbot_record_commands();
		dkbot_redirect_args();
		break;
	case DK_MSG_SERVER_INIT:
		dkbot_server_init();
		break;
	case DK_MSG_LEVEL_LOAD:
		dkbot_level_load();
		break;
	case DK_MSG_LEVEL_EXIT:
		dkbot_level_exit();
		break;
	default:
		break;
	}
	return rc;
}

int dll_ClientConnect(userEntity_t *self, void *userinfo, int loadgame)
{
	if (!dkbot_adding_bot())
		dkbot_release_edict((const edict_t *)self);
	return dk_orig.dll_ClientConnect
	       ? dk_orig.dll_ClientConnect(self, userinfo, loadgame) : 1;
}
