#ifndef HL_METAHHOOK_EMBED_H
#define HL_METAHHOOK_EMBED_H

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*MetaHookFactoryFn)(const char *name, int *returnCode);

int MetaHook_Startup(const char *cmdline);
void MetaHook_SetCmdLine(const char *cmdline);
const char *MetaHook_CmdLine(void);
MetaHookFactoryFn MetaHook_GetFactory(void);

int MetaHook_BindFileSystem(HMODULE fsModule, const char *gameDir);
void MetaHook_UnbindFileSystem(void);

void MetaHook_LoadEngine(HMODULE engineModule, const char *engineDll, const char *gameDir);
void MetaHook_ExitGame(int runResult);
void MetaHook_ShutdownPlugins(void);
void MetaHook_Finish(void);

#ifdef __cplusplus
}
#endif

#endif
