#include "launcher.h"
#include "engine_api.h"

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

#ifdef HL_LAUNCHER_DLLS
static const char *SkipSpaces(const char *p)
{
    while (*p == ' ' || *p == '\t') {
        p++;
    }
    return p;
}

static const char *NextToken(const char *p, char *out, size_t outSize)
{
    size_t n = 0;

    p = SkipSpaces(p);
    if (*p == '\0') {
        out[0] = '\0';
        return p;
    }
    if (*p == '"') {
        p++;
        while (*p != '\0' && *p != '"' && n + 1 < outSize) {
            out[n++] = *p++;
        }
        if (*p == '"') {
            p++;
        }
    } else {
        while (*p != '\0' && *p != ' ' && *p != '\t' && n + 1 < outSize) {
            out[n++] = *p++;
        }
    }
    out[n] = '\0';
    return p;
}

static int DllNameIsSafe(const char *name)
{
    size_t len;
    const char *p;

    if (name == NULL || name[0] == '\0') {
        return 0;
    }
    for (p = name; *p != '\0'; p++) {
        if (*p == '/' || *p == '\\' || *p == ':' || *p == '"' || *p == '\'') {
            return 0;
        }
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0) {
        return 0;
    }
    if (strstr(name, "..") != NULL) {
        return 0;
    }
    len = strlen(name);
    if (len < 5 || _stricmp(name + len - 4, ".dll") != 0) {
        return 0;
    }
    return 1;
}

static int LoadDllFromExeDir(const char *dir, const char *name)
{
    char path[MAX_PATH];
    HMODULE module;
    void (*init)(void);

    if (!DllNameIsSafe(name)) {
        Fail("Invalid -dll name (basename only, in the game folder).");
        return 0;
    }
    _snprintf(path, sizeof(path), "%s\\%s", dir, name);
    path[sizeof(path) - 1] = '\0';
    module = LoadLibraryA(path);
    if (module == NULL) {
        char msg[512];
        _snprintf(msg, sizeof(msg), "Can't load %s", name);
        Fail(msg);
        return 0;
    }
    init = (void (*)(void))GetProcAddress(module, "Launcher_Init");
    if (init != NULL) {
        init();
    }
    return 1;
}

static int LoadCommandLineDlls(const char *dir, const char *cmd)
{
    const char *p = cmd;
    char tok[MAX_PATH];
    char name[MAX_PATH];

    while (*p != '\0') {
        p = NextToken(p, tok, sizeof(tok));
        if (tok[0] == '\0') {
            break;
        }
        if (_stricmp(tok, "-dll") != 0) {
            continue;
        }
        p = NextToken(p, name, sizeof(name));
        if (name[0] == '\0' || name[0] == '-') {
            Fail("-dll requires a DLL file name.");
            return 0;
        }
        if (!LoadDllFromExeDir(dir, name)) {
            return 0;
        }
    }
    return 1;
}
#endif

int HlLauncher_Run(HINSTANCE instance, const char *cmdlineIn)
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

    DirFromModulePath(dir, sizeof(dir));
    SetCurrentDirectoryA(dir);

    mutex = CreateMutexA(NULL, FALSE, ENGINE_MUTEX_NAME);
    if (mutex != NULL && GetLastError() == ERROR_ALREADY_EXISTS) {
        Fail("The game could not be started because it is already running.");
        CloseHandle(mutex);
        return 1;
    }

    if (cmdlineIn != NULL && cmdlineIn[0] != '\0') {
        _snprintf(cmdline, sizeof(cmdline), "%s", cmdlineIn);
    } else {
        _snprintf(cmdline, sizeof(cmdline), "%s", GetCommandLineA());
    }
    cmdline[sizeof(cmdline) - 1] = '\0';

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

    engineModule = LoadGameLibrary(dir, engineFile);
    if (engineModule == NULL || ModuleFactory(engineModule) == NULL) {
        Fail("Can't load engine DLL");
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

#ifdef HL_LAUNCHER_DLLS
    if (!LoadCommandLineDlls(dir, cmdline)) {
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1;
    }
#endif

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
