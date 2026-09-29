# The module boundary

How the engine loads its game modules, and how dkbot takes the first slot. Prototypes are in `include/dk/dk_boundary.h`; `make check-boundary` re-verifies the contract against a new build.

## What the engine does

1.3 links the retail game DLLs statically, but `GetGameAPI(game_import_t *)` still calls `DLL_LoadDLLs` before it fills in `globals`:

```c
dll_t dlls[50];                 /* { path, fname, ext, HINSTANCE dll, dllEntry_t *dll_Entry } */

memset(dlls, 0, sizeof dlls);
dlls[0].dll = dlopen(NULL, RTLD_NOW);  dlls[0].dll_Entry = dlsym(.., "dll_Entry");
dlls[0].dll_Entry(dlls[0].dll, 12, &serverState);
dlls[1].dll = dlopen(NULL, RTLD_NOW);  dlls[1].dll_Entry = dlsym(.., "weapons_dll_Entry");
dlls[1].dll_Entry(dlls[1].dll, 12, &serverState);
if (!dedicated) {                      /* GCE is client-side */
    dlls[2].dll = dlopen(NULL, RTLD_NOW); dlls[2].dll_Entry = dlsym(.., "gce_dll_Entry");
    dlls[2].dll_Entry(dlls[2].dll, 12, &serverState);
}
dll_ClientDisconnect_fp = DLL_FindFunction("dll_ClientDisconnect");   /* and 16 more */

void *DLL_FindFunction(const char *name) {
    for (dll_t *d = dlls; d != dlls + 50; d++)
        if (d->dll) { void *p = dlsym(d->dll, name); if (p) return p; }
    return NULL;
}
```

The first hit wins; a name no slot has is fatal (`Unable to find <name> in dlls.`).

## `dll_Entry` messages

Any other message returns 0.

| msg | Action | Returns |
|---|---|---|
| 1 | version check: `*(int *)pvData == 0xBA0` | 1/0 |
| 2 | `*(void **)pvData = NULL` | 1 |
| 3 | `*(const char **)pvData = dll_Description` | 1 |
| 10 | `dll_ServerInit()` | 1 |
| 11 | `dll_ServerKill()` | 1 |
| 12 | `dll_ServerLoad((serverState_t *)pvData)` | 1 |
| 20 | `dll_LevelLoad()` | 1 |
| 21 | `dll_LevelExit()` | 1 |
| 22 | `SIDEKICK_TriggeredNodeListSave(FILE *)` | 1 |
| 23 | `SIDEKICK_TriggeredNodeListLoad(FILE *)` | 1 |

## Taking slot 0

**Linux** (`shim/dk_preload.c`, loaded with `LD_PRELOAD`): the shim interposes `dlopen`. The first `dlopen(NULL)` called from `DLL_LoadDLLs` (found with `dladdr`) pins that call site, which then returns a handle to `$DK_MOD` and nowhere else; that survives the skipped GCE slot on a dedicated server and a second `DLL_LoadDLLs`.

**Windows** (`launcher/dkbot-launch.c`, `dkbot/dk_win.c`): the launcher starts the game suspended and injects `dkbot.dll` before the game's own code runs. Its `DllMain` patches the executable's `GetProcAddress` import to return our `dll_Entry`, `dll_ClientConnect` and `dll_ClientDisconnect` and pass every other name through. The engine's internal symbols come from `daikatana.pdb` through dbghelp.

Either way the other slots still resolve to the executable, so every name the module does not define falls through to the engine.

## Rules

1. **Substitute only in `DLL_FindFunction`.** `SPAWN_CallInitFunction` does its own `dlopen(NULL)` to look spawn functions up by classname; substituting that handle would break every class the module does not implement.
2. **`RTLD_GLOBAL` adds names, it cannot replace them.** With `DK_MOD_GLOBAL=1` the module can add entity classnames, but the executable's own definitions always win.
3. **Do not patch `globals` from `dll_Entry`.** `GetGameAPI` fills it after `DLL_LoadDLLs` returns. Wrap a `globals` field such as `RunFrame` later, from message 10 or 20; overridden `dll_*` names need no patching.
4. **Chain by pointer, never by name.** Inside the module the name `dll_Entry` binds to our own definition; resolve the engine's at runtime (`dk_sym()` in `dkbot/engine.c`).

## The overridable names

Besides the three `*_dll_Entry` points, `DLL_FindFunction` resolves 17 names: `dll_ClientThink`, `dll_ClientConnect`, `dll_ClientBegin`, `dll_ClientUserinfoChanged`, `dll_ClientDisconnect`, `dll_ClientBeginServerFrame`, `dll_SetStats`, `dll_Client_InitAttributes`, `dll_BeginIntermission`, `dll_LoadNodes`, `dll_EntityLoadCleanup`, `dll_RegisterWorldFuncs`, `dll_FLAG_GetScores`, `dll_FLAG_CheckRules`, `dll_DT_CanDamage`, `SIDEKICK_Alert` and `ShowBoundingBoxes`. dkbot overrides `dll_Entry`, `dll_ClientConnect` and `dll_ClientDisconnect`.
