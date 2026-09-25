#ifndef DK_ENTSTATE_H
#define DK_ENTSTATE_H

/* DK_ENT_* travel in entity_state_t.modelindex2, unused by upstream botlib. */
#define DK_ENT_PLAYER    0x0001
#define DK_ENT_ALIVE     0x0002
#define DK_ENT_SHOOTING  0x0004
#define DK_ENT_INVULN    0x0008

/* The entity's team plus one, so that zero means "not stated". */
#define DK_ENT_TEAM_SHIFT   16
#define DK_ENT_TEAM_MASK    0xF

/* stats slots 20..23 are free: botlib reads only 1, 4, 9, 10 and 16. */
#define DK_STAT_INVULN_SECS    20
#define DK_STAT_ENVIROSUIT_SECS 21
#define DK_STAT_OXYLUNG_SECS   22

#define DK_STAT_ARMOR          23

#endif
