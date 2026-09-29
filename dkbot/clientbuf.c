#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "dkbot.h"

/* On a listen server the host reads the loopback ring, so bots must not send into it. */
static int listen_server(void)
{
	static int cached = -1;

	if (cached < 0) {
		const char *(*cvar_string)(const char *) = dk_sym(DK_SYM_CVAR_STRING);
		const char *v = cvar_string ? cvar_string("dedicated") : NULL;

		cached = (v && atoi(v) == 0) ? 1 : 0;
	}
	return cached;
}

static void set_bot_address(unsigned char *cl)
{
	unsigned char *na = cl + DK_CLIENT_OFF_NETCHAN + DK_NETCHAN_OFF_REMOTEADDR;

	memset(na, 0, 12); /* NA_LOOPBACK */
	if (!listen_server())
		return;
	DK_AT(na, DK_NETADR_OFF_TYPE, int) = DK_NA_IP;
	/* Windows drops sends to 0.0.0.0; a dead port's WSAECONNRESET would end SV_ReadPackets. */
#ifndef _WIN32
	/* Linux delivers 0.0.0.0 to this host */
	na[DK_NETADR_OFF_IP + 0] = 127;
	na[DK_NETADR_OFF_IP + 3] = 1;
#endif
	/* htons(1): a port the server never binds */
	DK_AT(na, DK_NETADR_OFF_PORT, unsigned short) = 0x0100;
}

static unsigned char *client_at(int slot)
{
	unsigned char *clients;

	if (!dk_glob.svs || slot < 1)
		return NULL;
	clients = DK_AT(dk_glob.svs, DK_SVS_OFF_CLIENTS, unsigned char *);
	/* client n owns edict n + 1 */
	return clients ? clients + (size_t)(slot - 1) * DK_CLIENT_SIZE : NULL;
}

static void arm(unsigned char *sb, unsigned char *buf, int size)
{
	DK_AT(sb, DK_SIZEBUF_OFF_DATA, unsigned char *) = buf;
	DK_AT(sb, DK_SIZEBUF_OFF_MAXSIZE, int)          = size;
	DK_AT(sb, DK_SIZEBUF_OFF_ALLOWOVERFLOW, int)    = 1;
	DK_AT(sb, DK_SIZEBUF_OFF_OVERFLOWED, int)       = 0;
	DK_AT(sb, DK_SIZEBUF_OFF_CURSIZE, int)          = 0;
	DK_AT(sb, DK_SIZEBUF_OFF_READCOUNT, int)        = 0;
}

static int bot_ping[DK_MAX_BOTS];

void dkbot_set_ping(int bot, int ms)
{
	if (bot >= 0 && bot < DK_MAX_BOTS)
		bot_ping[bot] = ms < 0 ? 0 : (ms > 999 ? 999 : ms);
}

/* A send to an unarmed slot aborts the server; SV_InitGame reallocates svs.clients. */
void dkbot_arm_client_buffers(void)
{
	int i, n = dkbot_count();

	for (i = 0; i < n; i++) {
		unsigned char *cl = client_at(dkbot_bot_slot(i));
		const edict_t *ent = dkbot_bot_edict(i);

		if (!cl)
			continue;
		/* Free, or the zombie a kicked bot leaves on the edict the next bot takes. */
		if (DK_AT(cl, DK_CLIENT_OFF_STATE, int) != DK_CS_SPAWNED) {
			DK_AT(cl, DK_CLIENT_OFF_STATE, int) = DK_CS_SPAWNED;
			DK_AT(cl, DK_CLIENT_OFF_EDICT, const void *) = ent;
			snprintf((char *)cl + DK_CLIENT_OFF_NAME, DK_CLIENT_NAME_SIZE,
			         "%s", dkbot_bot_name(i));
			set_bot_address(cl);
			/* No 16-bit qport matches -1: a stray loopback packet would drop the bot. */
			DK_AT(cl, DK_CLIENT_OFF_NETCHAN + DK_NETCHAN_OFF_QPORT, int) = -1;
		} else if (DK_AT(cl, DK_CLIENT_OFF_EDICT, const void *) != ent) {
			continue;
		}
		/* kept fresh against SV_CheckTimeouts' timeout and idle kick */
		DK_AT(cl, DK_CLIENT_OFF_LASTMESSAGE, int) =
		        DK_AT(dk_glob.svs, DK_SVS_OFF_REALTIME, int);
		DK_AT(cl, DK_CLIENT_OFF_IDLETIME, int) = 0;
		DK_AT(cl, DK_CLIENT_OFF_PING, int) = bot_ping[i];
		arm(cl + DK_CLIENT_OFF_DATAGRAM, cl + DK_CLIENT_OFF_DATAGRAM_BUF,
		    DK_CLIENT_DATAGRAM_BUF_SIZE);
		arm(cl + DK_CLIENT_OFF_NETCHAN + DK_NETCHAN_OFF_MESSAGE,
		    cl + DK_CLIENT_OFF_NETCHAN + DK_NETCHAN_OFF_MESSAGE_BUF,
		    DK_NETCHAN_MESSAGE_BUF_SIZE);
	}
}

void dkbot_release_client_slot(const edict_t *ent)
{
	unsigned char *clients;
	int i, mc;

	if (!dk_glob.svs || !dk_glob.maxclients || !*dk_glob.maxclients)
		return;
	clients = DK_AT(dk_glob.svs, DK_SVS_OFF_CLIENTS, unsigned char *);
	mc = (*dk_glob.maxclients)->intValue;
	for (i = 0; clients && i < mc; i++) {
		unsigned char *cl = clients + (size_t)i * DK_CLIENT_SIZE;

		if (DK_AT(cl, DK_CLIENT_OFF_EDICT, const void *) != ent)
			continue;
		if (DK_AT(cl, DK_CLIENT_OFF_STATE, int) == DK_CS_SPAWNED)
			DK_AT(cl, DK_CLIENT_OFF_STATE, int) = DK_CS_FREE;
		DK_AT(cl, DK_CLIENT_OFF_EDICT, const void *) = NULL;
		return;
	}
}
