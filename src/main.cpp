#include "engine_api.h"

#include <windows.h>

#include <stdio.h>
#include <string.h>

/* Thin replacement for Valve's hl.exe stub. RevEmu stays in cstrike.exe
 * (revloader). This process is the game: load optional vellum.dll, then
 * hw.dll / sw.dll and IEngineAPI::Run. Must stay 32-bit. */

#define ENGINE_MUTEX_NAME "ValveHalfLifeLauncherMutex"

static void Fail(const char *text)
{
    MessageBoxA(NULL, text, "Error", MB_OK | MB_ICONERROR);
}

static void DirFromModulePath(char *dir, size_t dirSize)
{
    char *slash;

    GetModuleFileNameA(NULL, dir, (DWORD)dirSize);
    dir[dirSize - 1] = '\0';
    slash = strrchr(dir, '\\');
    if (slash != NULL) {
        *slash = '\0';
    }
}

static int HasArg(const char *cmd, const char *arg)
{
    const char *p = cmd;
    size_t n = strlen(arg);

    while ((p = strstr(p, arg)) != NULL) {
        if (p == cmd || p[-1] == ' ' || p[-1] == '\t') {
            char end = p[n];
            if (end == '\0' || end == ' ' || end == '\t') {
                return 1;
            }
        }
        p += n;
    }
    return 0;
}

static CreateInterfaceFn ModuleFactory(HMODULE module)
{
    if (module == NULL) {
        return NULL;
    }
    return (CreateInterfaceFn)GetProcAddress(module, "CreateInterface");
}

static IBaseInterface *LauncherFactory(const char *name, int *returnCode)
{
    (void)name;
    if (returnCode != NULL) {
        *returnCode = 1;
    }
    return NULL;
}

static HMODULE LoadGameLibrary(const char *dir, const char *file)
{
    char path[MAX_PATH];
    HMODULE module;

    _snprintf(path, sizeof(path), "%s\\%s", dir, file);
    path[sizeof(path) - 1] = '\0';
    module = LoadLibraryA(path);
    if (module == NULL) {
        module = LoadLibraryA(file);
    }
    return module;
}

static void TryLoadSidecar(const char *dir, HMODULE engineModule)
{
    HMODULE sidecar;
    typedef void (*VellumInitFn)(void *);
    VellumInitFn initFn;

    sidecar = LoadGameLibrary(dir, "vellum.dll");
    if (sidecar == NULL) {
        return;
    }
    initFn = (VellumInitFn)GetProcAddress(sidecar, "Vellum_Init");
    if (initFn != NULL) {
        initFn(engineModule);
    }
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmd, int show)
{
    char dir[MAX_PATH];
    char cmdline[4096];
    char postRestart[4096];
    const char *engineFile;
    HMODULE fsModule;
    HMODULE engineModule;
    CreateInterfaceFn fsFactory;
    IEngineAPI *engine;
    HANDLE mutex;
    EngineRunResult result;

    (void)prev;
    (void)cmd;
    (void)show;

    DirFromModulePath(dir, sizeof(dir));
    SetCurrentDirectoryA(dir);

    mutex = CreateMutexA(NULL, FALSE, ENGINE_MUTEX_NAME);
    if (mutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
        Fail("The game could not be started because it is already running.");
        CloseHandle(mutex);
        return 1;
    }

    _snprintf(cmdline, sizeof(cmdline), "%s", GetCommandLineA());
    cmdline[sizeof(cmdline) - 1] = '\0';
    if (!HasArg(cmdline, "-game")) {
        size_t n = strlen(cmdline);
        _snprintf(cmdline + n, sizeof(cmdline) - n, " -game cstrike");
        cmdline[sizeof(cmdline) - 1] = '\0';
    }

    engineFile = (HasArg(cmdline, "-sw") || HasArg(cmdline, "-software")) ? "sw.dll" : "hw.dll";

    fsModule = LoadGameLibrary(dir, "FileSystem_Stdio.dll");
    if (fsModule == NULL) {
        Fail("Can't find FileSystem_Stdio.dll");
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1;
    }
    fsFactory = ModuleFactory(fsModule);
    if (fsFactory == NULL) {
        Fail("FileSystem_Stdio.dll has no CreateInterface");
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1;
    }

    engineModule = LoadGameLibrary(dir, engineFile);
    if (engineModule == NULL) {
        char msg[256];
        _snprintf(msg, sizeof(msg), "Can't find %s", engineFile);
        Fail(msg);
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1;
    }

    TryLoadSidecar(dir, engineModule);

    if (ModuleFactory(engineModule) == NULL) {
        Fail("Engine DLL has no CreateInterface");
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1;
    }
    engine = (IEngineAPI *)ModuleFactory(engineModule)(VENGINE_LAUNCHER_API_VERSION, NULL);
    if (engine == NULL) {
        Fail("CreateInterface(VENGINE_LAUNCHER_API_VERSION002) failed");
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1;
    }

    do {
        postRestart[0] = '\0';
        result = engine->Run(instance, dir, cmdline, postRestart, LauncherFactory, fsFactory);
        if (result == ENGRUN_CHANGED_VIDEOMODE && postRestart[0] != '\0') {
            _snprintf(cmdline, sizeof(cmdline), "%s", postRestart);
            cmdline[sizeof(cmdline) - 1] = '\0';
        }
    } while (result == ENGRUN_CHANGED_VIDEOMODE);

    if (mutex != NULL) {
        CloseHandle(mutex);
    }
    return (result == ENGRUN_QUITTING) ? 0 : 1;
}
