# Daikatana is x86-64 only: other hosts cross-compile (CROSS= or CC=).
HOSTARCH := $(shell uname -m)
ifeq ($(HOSTARCH),x86_64)
CROSS   ?=
else
CROSS   ?= x86_64-linux-gnu-
endif
CC      := $(CROSS)gcc
BUILD   := build
CFLAGS  ?= -std=gnu99 -O2 -g -Wall -Wextra -Wno-unused-parameter \
           -fno-strict-aliasing -fwrapv -fPIC -Iinclude
LDFLAGS ?= -shared

SHIM_SRC := shim/dk_preload.c
MOD_COMMON := dkbot/entry.c dkbot/engine.c dkbot/frame.c dkbot/bots.c dkbot/botlib_glue.c dkbot/snapshot.c dkbot/cmd.c dkbot/clientbuf.c
MOD_SRC  := $(MOD_COMMON) dkbot/dk_platform_posix.c

SHIM := $(BUILD)/dk_preload.so
MOD  := $(BUILD)/dkbot.so

.PHONY: all shim mod clean check-boundary

all: shim mod

shim: $(SHIM)
mod:  $(MOD)

$(SHIM): $(SHIM_SRC) | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -ldl
	@file $@ | sed 's/^/  /'

$(MOD): $(MOD_SRC) $(wildcard dkbot/*.h include/dk/*.h) | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(MOD_SRC) -ldl -lm
	@file $@ | sed 's/^/  /'

$(BUILD):
	@mkdir -p $(BUILD)

DK_BUILD ?= $(lastword $(sort $(wildcard vendor/dk-*-x64)))
check-boundary:
	@test -n "$(DK_BUILD)" || { echo "no vendor/dk-*-x64 -- run tools/fetch-dk.sh" >&2; exit 1; }
	@tools/dump-boundary.py $(DK_BUILD)

.PHONY: check-pdb
check-pdb:
	@WINCC=$(WINCC) tools/check-pdb.py $(if $(DK_WIN),"$(DK_WIN)")

clean:
	rm -rf $(BUILD)

# Host-native: needs no x86-64 runtime.
.PHONY: test
test: | $(BUILD)
	gcc $(filter-out -fPIC,$(CFLAGS)) -shared -fPIC -o $(BUILD)/test_shim.so $(SHIM_SRC) -ldl
	gcc $(filter-out -fPIC,$(CFLAGS)) -shared -fPIC -o $(BUILD)/test_mod.so tests/fake_mod.c
	gcc $(filter-out -fPIC,$(CFLAGS)) -rdynamic -o $(BUILD)/test_engine tests/fake_engine.c -ldl
	LD_PRELOAD=$(PWD)/$(BUILD)/test_shim.so DK_MOD=$(PWD)/$(BUILD)/test_mod.so \
		DK_SHIM_VERBOSE=1 $(BUILD)/test_engine

MAP  ?= e1dm1
SECS ?=
.PHONY: run
run: all
	@tools/run-dkded.sh $(MAP) $(SECS)

.PHONY: gmalloc
gmalloc: $(BUILD)/gmalloc.so
$(BUILD)/gmalloc.so: tools/gmalloc.c | $(BUILD)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $< -ldl -lpthread

GLAD ?= gladiator-bot-restored
# Upstream's gate for its live-server fixes; 0 builds the faithful 1999 code.
GLAD_SERVERFIX ?= 1
BOTLIB_TUS := be_aas_bspq2 be_aas_cluster be_aas_debug be_aas_entity \
  be_aas_file be_aas_light be_aas_main be_aas_move be_aas_optimize \
  be_aas_reach be_aas_route be_aas_routealt be_aas_sample be_aas_sound \
  be_ai2_dmnet be_ai2_dmdk be_ai2_main be_ai_char be_ai_chat be_ai_goal \
  be_ai_load be_ai_move be_ai_weap be_ai_weight be_ea be_interface \
  l_crc l_libvar l_log l_memory l_precomp l_script l_struct l_utils \
  botlib_debug q_shared dk_maps
BOTLIB_OBJS := $(patsubst %,$(BUILD)/botlib/%.o,$(BOTLIB_TUS))
# Zeroed locals keep play independent of stack garbage; pattern exposes uninitialized reads.
AUTOINIT ?= zero
# GCC 14+ makes the last two errors, which -w does not silence.
BOTLIB_CFLAGS := -O0 -ftrivial-auto-var-init=$(AUTOINIT) -Dstricmp=strcasecmp -DBOTLIB -DC_ONLY \
  -DGLAD_SERVERFIX=$(GLAD_SERVERFIX) -Iinclude \
  -I$(GLAD)/botlib -I$(GLAD)/game -Wno-all -w -Wno-int-conversion -Wno-incompatible-pointer-types
BOTLIB := $(BUILD)/gladiator_x64.so

.PHONY: botlib
botlib: $(BOTLIB)

$(GLAD)/botlib/%.c:
	$(error $(GLAD)/botlib is missing -- run `git submodule update --init`, or set GLAD=<checkout>)

# Objects depend on a stamp rewritten when the flags change.
BOTLIB_STAMP := $(BUILD)/botlib/cflags
$(BOTLIB_STAMP): FORCE
	@mkdir -p $(@D)
	@echo '$(BOTLIB_CFLAGS)' | cmp -s - $@ || echo '$(BOTLIB_CFLAGS)' > $@

.PHONY: FORCE
FORCE:

# First, so that a botlib_patch/ copy wins over upstream's.
$(BUILD)/botlib/%.o: botlib_patch/%.c $(BOTLIB_STAMP)
	@mkdir -p $(@D)
	$(CC) -c -std=gnu99 -g -fPIC -fno-strict-aliasing -fwrapv $(BOTLIB_CFLAGS) -o $@ $<

$(BUILD)/botlib/%.o: $(GLAD)/botlib/%.c $(BOTLIB_STAMP)
	@mkdir -p $(@D)
	$(CC) -c -std=gnu99 -g -fPIC -fno-strict-aliasing -fwrapv $(BOTLIB_CFLAGS) -o $@ $<

# -Bsymbolic: else the library's globals (ctf, logfile, ...) bind to the engine's exports.
$(BOTLIB): $(BOTLIB_OBJS)
	$(CC) -shared -Wl,-Bsymbolic -o $@ $^ -lm
	@file $@ | sed 's/^/  /'
	@nm -D --defined-only $@ | grep -E " T GetBotAPI" || echo "  !! GetBotAPI not exported"

WINCC     ?= x86_64-w64-mingw32-gcc
WINCFLAGS ?= -std=gnu99 -O2 -g -Wall -Wextra -Wno-unused-parameter \
             -fno-strict-aliasing -fwrapv -Iinclude -D_WIN32_WINNT=0x0601
WIN_MOD_SRC := $(MOD_COMMON) dkbot/dk_win.c

WINMOD   := $(BUILD)/dkbot.dll
WINLAUNCH:= $(BUILD)/dkbot-launch.exe
WINBOTLIB:= $(BUILD)/gladiator_x64.dll

.PHONY: windows winmod winlaunch winbotlib
windows: winmod winlaunch winbotlib
winmod:    $(WINMOD)
winlaunch: $(WINLAUNCH)
winbotlib: $(WINBOTLIB)

# -static folds in libgcc, so no MinGW runtime DLL has to ship.
$(WINMOD): $(WIN_MOD_SRC) $(wildcard dkbot/*.h include/dk/*.h) | $(BUILD)
	$(WINCC) $(WINCFLAGS) -shared -o $@ $(WIN_MOD_SRC) -static -ldbghelp -lm
	@file $@ | sed 's/^/  /'

$(WINLAUNCH): launcher/dkbot-launch.c | $(BUILD)
	$(WINCC) $(WINCFLAGS) -o $@ $< -static
	@file $@ | sed 's/^/  /'

BOTLIB_WIN_OBJS := $(patsubst %,$(BUILD)/botlib_win/%.o,$(BOTLIB_TUS))
BOTLIB_WIN_STAMP := $(BUILD)/botlib_win/cflags
$(BOTLIB_WIN_STAMP): FORCE
	@mkdir -p $(@D)
	@echo '$(BOTLIB_CFLAGS) [win]' | cmp -s - $@ || echo '$(BOTLIB_CFLAGS) [win]' > $@

$(BUILD)/botlib_win/%.o: botlib_patch/%.c $(BOTLIB_WIN_STAMP)
	@mkdir -p $(@D)
	$(WINCC) -c -std=gnu99 -g -fno-strict-aliasing -fwrapv $(BOTLIB_CFLAGS) -o $@ $<

$(BUILD)/botlib_win/%.o: $(GLAD)/botlib/%.c $(BOTLIB_WIN_STAMP)
	@mkdir -p $(@D)
	$(WINCC) -c -std=gnu99 -g -fno-strict-aliasing -fwrapv $(BOTLIB_CFLAGS) -o $@ $<

$(WINBOTLIB): $(BOTLIB_WIN_OBJS)
	$(WINCC) -shared -o $@ $^ -static -lm
	@file $@ | sed 's/^/  /'
	@x86_64-w64-mingw32-nm $@ 2>/dev/null | grep -qE " T GetBotAPI" && echo "  GetBotAPI exported" || echo "  (check GetBotAPI export)"
