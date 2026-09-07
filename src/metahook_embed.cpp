#include <winsock2.h>
#include <windows.h>

#include "metahook_embed.h"
#include "LoadDllNotification.h"
#include "interface.h"
#include "sys.h"

#include <string.h>

IFileSystem_HL25 *g_pFileSystem_HL25 = nullptr;
IFileSystem *g_pFileSystem = nullptr;
PVOID g_BlobLoaderSectionBase = NULL;
ULONG g_BlobLoaderSectionSize = 0;

PVOID MH_GetEngineBase(void);
DWORD MH_GetEngineSize(void);
void MH_LoadEngine(HMODULE hEngineModule, BlobHandle_t hBlobEngine, const char *szGameName, const char *szFullGamePath, const char *pszEngineDLL);
void MH_ExitGame(int iResult);
void MH_Shutdown(void);

static int g_wsaReady;
static int g_registryReady;
static int g_fsBound;

static void GameNameFromCmd(char *out, size_t outSize)
{
    const char *value = NULL;

    CommandLine()->CheckParm("-game", &value);
    if (value == NULL || value[0] == '\0') {
        value = "cstrike";
    }
    strncpy(out, value, outSize - 1);
    out[outSize - 1] = '\0';
}

int MetaHook_Startup(const char *cmdline)
{
    WSADATA wsa;

    CommandLine()->CreateCmdLine(cmdline != NULL ? cmdline : GetCommandLineA());
    SetEnvironmentVariableA("SDL_HINT_MOUSE_EMULATE_WARP_WITH_RELATIVE", "0");
    SetEnvironmentVariableA("SDL_MOUSE_EMULATE_WARP_WITH_RELATIVE", "0");

    if (!g_wsaReady) {
        if (WSAStartup(0x202, &wsa) != 0) {
            return 0;
        }
        g_wsaReady = 1;
    }
    if (!g_registryReady) {
        registry->Init();
        g_registryReady = 1;
    }
    return 1;
}

void MetaHook_SetCmdLine(const char *cmdline)
{
    CommandLine()->CreateCmdLine(cmdline != NULL ? cmdline : GetCommandLineA());
}

const char *MetaHook_CmdLine(void)
{
    return CommandLine()->GetCmdLine();
}

MetaHookFactoryFn MetaHook_GetFactory(void)
{
    return (MetaHookFactoryFn)Sys_GetFactoryThis();
}

int MetaHook_BindFileSystem(HMODULE fsModule, const char *gameDir)
{
    CreateInterfaceFn factory;
    void *iface;
    void **vtable;

    (void)gameDir;
    g_pFileSystem = NULL;
    g_pFileSystem_HL25 = NULL;

    factory = Sys_GetFactory((HINTERFACEMODULE)fsModule);
    if (factory == NULL) {
        return 0;
    }
    iface = factory(FILESYSTEM_INTERFACE_VERSION, NULL);
    if (iface == NULL) {
        return 0;
    }
    vtable = *(void ***)iface;
    if (memcmp(vtable[16], vtable[17], 16) == 0) {
        g_pFileSystem_HL25 = (IFileSystem_HL25 *)iface;
    } else {
        g_pFileSystem = (IFileSystem *)iface;
    }

    FILESYSTEM_ANY_MOUNT();
    FILESYSTEM_ANY_ADDSEARCHPATH(Sys_GetLongPathName(), "ROOT");
    g_fsBound = 1;
    return 1;
}

void MetaHook_UnbindFileSystem(void)
{
    if (!g_fsBound) {
        return;
    }
    FILESYSTEM_ANY_UNMOUNT();
    g_pFileSystem = NULL;
    g_pFileSystem_HL25 = NULL;
    g_fsBound = 0;
}

void MetaHook_LoadEngine(HMODULE engineModule, const char *engineDll, const char *gameDir)
{
    char gameName[32];

    GameNameFromCmd(gameName, sizeof(gameName));
    MH_LoadEngine(engineModule, NULL, gameName, gameDir, engineDll);
}

void MetaHook_ExitGame(int runResult)
{
    MH_ExitGame(runResult);
    MH_DispatchLoadLdrDllNotificationCallback(NULL, NULL, MH_GetEngineBase(), MH_GetEngineSize(), LOAD_DLL_NOTIFICATION_IS_UNLOAD);
}

void MetaHook_ShutdownPlugins(void)
{
    MH_Shutdown();
}

void MetaHook_Finish(void)
{
    if (g_registryReady) {
        registry->Shutdown();
        g_registryReady = 0;
    }
    if (g_wsaReady) {
        WSACleanup();
        g_wsaReady = 0;
    }
}
