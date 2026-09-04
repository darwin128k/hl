#include "engine_api.h"
#include "module.h"
#include "prefetch.h"
#include "shell.h"
#include "steam.h"

#include <windows.h>

#include <stdio.h>
#include <string.h>

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

static void WriteEngineVideoMode(int width, int height)
{
    HKEY key = NULL;
    DWORD disp = 0;
    DWORD windowed = 1;
    DWORD w = (DWORD)width;
    DWORD h = (DWORD)height;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Half-Life\\Settings",
                      0, KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Half-Life\\Settings",
                            0, NULL, 0, KEY_SET_VALUE, NULL, &key, &disp) != ERROR_SUCCESS) {
            return;
        }
    }
    RegSetValueExA(key, "ScreenWidth", 0, REG_DWORD, (const BYTE *)&w, sizeof(w));
    RegSetValueExA(key, "ScreenHeight", 0, REG_DWORD, (const BYTE *)&h, sizeof(h));
    RegSetValueExA(key, "ScreenWindowed", 0, REG_DWORD, (const BYTE *)&windowed, sizeof(windowed));
    RegCloseKey(key);
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

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmd, int show)
{
    char dir[MAX_PATH];
    char cmdline[4096];
    char postRestart[4096];
    char connectAddr[256];
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

    if (!Shell_Create(instance, dir)) {
        Fail("Can't create launcher window");
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1;
    }

    Prefetch_Start(dir);

    if (!Shell_Wait(connectAddr, sizeof(connectAddr))) {
        Prefetch_Stop();
        Shell_Destroy();
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 0;
    }

    if (!Steam_Start(dir)) {
        Prefetch_Stop();
        Shell_Destroy();
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1;
    }

    Module_LoadVellum(dir);

    Shell_EmbedGame();

    {
        int screenW = 0;
        int screenH = 0;
        Shell_GetClientSize(&screenW, &screenH);
        if (screenW < 640) {
            screenW = 640;
        }
        if (screenH < 480) {
            screenH = 480;
        }
        WriteEngineVideoMode(screenW, screenH);

        _snprintf(cmdline, sizeof(cmdline), "%s", GetCommandLineA());
        cmdline[sizeof(cmdline) - 1] = '\0';
        if (!HasArg(cmdline, "-game")) {
            size_t n = strlen(cmdline);
            _snprintf(cmdline + n, sizeof(cmdline) - n, " -game cstrike");
            cmdline[sizeof(cmdline) - 1] = '\0';
        }
        if (!HasArg(cmdline, "-w")) {
            size_t n = strlen(cmdline);
            _snprintf(cmdline + n, sizeof(cmdline) - n, " -w %d", screenW);
            cmdline[sizeof(cmdline) - 1] = '\0';
        }
        if (!HasArg(cmdline, "-h")) {
            size_t n = strlen(cmdline);
            _snprintf(cmdline + n, sizeof(cmdline) - n, " -h %d", screenH);
            cmdline[sizeof(cmdline) - 1] = '\0';
        }
        if (!HasArg(cmdline, "-window")) {
            size_t n = strlen(cmdline);
            _snprintf(cmdline + n, sizeof(cmdline) - n, " -window");
            cmdline[sizeof(cmdline) - 1] = '\0';
        }
        if (connectAddr[0] != '\0' && !HasArg(cmdline, "+connect") && !HasArg(cmdline, "connect")) {
            size_t n = strlen(cmdline);
            _snprintf(cmdline + n, sizeof(cmdline) - n, " +connect %s", connectAddr);
            cmdline[sizeof(cmdline) - 1] = '\0';
        }
    }

    engineFile = (HasArg(cmdline, "-sw") || HasArg(cmdline, "-software")) ? "sw.dll" : "hw.dll";

    fsModule = LoadGameLibrary(dir, "FileSystem_Stdio.dll");
    if (fsModule == NULL) {
        Fail("Can't find FileSystem_Stdio.dll");
        Prefetch_Stop();
        Shell_Destroy();
        Steam_Stop();
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1;
    }
    fsFactory = ModuleFactory(fsModule);
    engineModule = LoadGameLibrary(dir, engineFile);
    if (engineModule == NULL || ModuleFactory(engineModule) == NULL) {
        Fail("Can't load engine DLL");
        Prefetch_Stop();
        Shell_Destroy();
        Steam_Stop();
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1;
    }
    engine = (IEngineAPI *)ModuleFactory(engineModule)(VENGINE_LAUNCHER_API_VERSION, NULL);
    if (engine == NULL) {
        Fail("CreateInterface(VENGINE_LAUNCHER_API_VERSION002) failed");
        Prefetch_Stop();
        Shell_Destroy();
        Steam_Stop();
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

    Shell_Destroy();
    Prefetch_Stop();
    Steam_Stop();
    if (mutex != NULL) {
        CloseHandle(mutex);
    }
    return (result == ENGRUN_QUITTING) ? 0 : 1;
}
