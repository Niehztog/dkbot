#include <stdlib.h>

#include "dkbot.h"

static void (*orig_run_frame)(void);
static unsigned long frames;

unsigned long dkbot_frame_count(void)
{
	return frames;
}

static void dkbot_run_frame(void)
{
	static int log_every = -1;

	if (orig_run_frame)
		orig_run_frame();
	frames++;
	dkbot_think_all();
	if (log_every < 0) {
		const char *e = getenv("DK_BOT_LOG_EVERY");

		log_every = e && atoi(e) > 0 ? atoi(e) : 100;
	}
	if (frames % (unsigned long)log_every == 0) {
		char who[1024];

		dkbot_status(who, sizeof who);
		dk_log("frame %lu: bots=%d%s%s\n", frames, dkbot_count(),
		       *who ? " | " : "", who);
	}
}

/* ServerInit or later: GetGameAPI fills globals after DLL_LoadDLLs returns. */
int dkbot_install_hooks(void)
{
	dk_game_export_t *ge = dk_glob.ge;

	if (!ge || ge->RunFrame == dkbot_run_frame)
		return !ge;
	if (dk_orig.P_RunFrame && (void *)ge->RunFrame != (void *)dk_orig.P_RunFrame) {
		dk_log("REFUSING to hook: globals.RunFrame=%p but P_RunFrame=%p "
		       "(game_export_t layout changed?)\n",
		       (void *)ge->RunFrame, (void *)dk_orig.P_RunFrame);
		return 1;
	}
	if (dk_orig.Client_Think && (void *)ge->ClientThink != (void *)dk_orig.Client_Think)
		dk_log("warning: globals.ClientThink=%p but Client_Think=%p\n",
		       (void *)ge->ClientThink, (void *)dk_orig.Client_Think);
	orig_run_frame = ge->RunFrame;
	ge->RunFrame = dkbot_run_frame;
	dk_log("frame hook installed (edict_size=%d max_edicts=%d)\n",
	       ge->edict_size, ge->max_edicts);
	return 0;
}
