#include "launcher.h"
#include "engine_api.h"
#ifdef HL_METAHHOOK
#include "metahook_embed.h"
#endif

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

static int IsSpaceChar(char c)
{
    return c == ' ' || c == '\t';
}

static int ParmTakesValue(const char *parm)
{
    return _stricmp(parm, "-w") == 0 || _stricmp(parm, "-width") == 0
        || _stricmp(parm, "-h") == 0 || _stricmp(parm, "-height") == 0
        || _stricmp(parm, "-game") == 0 || _stricmp(parm, "+load") == 0
        || _stricmp(parm, "+connect") == 0 || _stricmp(parm, "-dll") == 0;
}

static void RemoveParm(char *cmd, const char *parm)
{
    size_t n = strlen(parm);
    char *p = cmd;

    while ((p = strstr(p, parm)) != NULL) {
        char before;
        char after;
        char *end;

        before = (p == cmd) ? ' ' : p[-1];
        if (before != ' ' && before != '\t') {
            p += n;
            continue;
        }
        after = p[n];
        if (after != '\0' && after != ' ' && after != '\t') {
            p += n;
            continue;
        }
        end = p + n;
        while (IsSpaceChar(*end)) {
            end++;
        }
        if (ParmTakesValue(parm) && *end != '\0' && *end != '-' && *end != '+') {
            if (*end == '"') {
                end++;
                while (*end != '\0' && *end != '"') {
                    end++;
                }
                if (*end == '"') {
                    end++;
                }
            } else {
                while (*end != '\0' && !IsSpaceChar(*end)) {
                    end++;
                }
            }
        }
        while (IsSpaceChar(*end)) {
            end++;
        }
        memmove(p, end, strlen(end) + 1);
    }
}

static void AppendCmd(char *cmd, size_t cmdSize, const char *arg)
{
    size_t n = strlen(cmd);
    if (arg == NULL || arg[0] == '\0') {
        return;
    }
    if (n > 0 && n + 1 < cmdSize && !IsSpaceChar(cmd[n - 1])) {
        cmd[n++] = ' ';
        cmd[n] = '\0';
    }
    _snprintf(cmd + n, cmdSize - n, "%s", arg);
    cmd[cmdSize - 1] = '\0';
}

static void MergePostRestart(char *cmdline, size_t cmdSize, const char *postRestart)
{
    static const char *kDrop[] = {
        "-sw", "-startwindowed", "-windowed", "-window",
        "-full", "-fullscreen", "-soft", "-software",
        "-gl", "-d3d", "-w", "-width", "-h", "-height", "-novid",
        NULL
    };
    int i;

    if (postRestart == NULL || postRestart[0] == '\0') {
        return;
    }
    for (i = 0; kDrop[i] != NULL; i++) {
        RemoveParm(cmdline, kDrop[i]);
    }
    if (strstr(postRestart, "-game") != NULL) {
        RemoveParm(cmdline, "-game");
    }
    if (strstr(postRestart, "+load") != NULL) {
        RemoveParm(cmdline, "+load");
    }
    AppendCmd(cmdline, cmdSize, postRestart);
}

static CreateInterfaceFn ModuleFactory(HMODULE module)
{
    if (module == NULL) {
        return NULL;
    }
    return (CreateInterfaceFn)GetProcAddress(module, "CreateInterface");
}

#ifndef HL_METAHHOOK
static IBaseInterface *LauncherFactory(const char *name, int *returnCode)
{
    (void)name;
    if (returnCode != NULL) {
        *returnCode = 1;
    }
    return NULL;
}
#endif

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

#ifdef HL_LAUNCHER_DLLS
    /* Sidecars stay loaded across a video restart. The engine itself must
     * not: stock hl.exe / Thanatos unload hw.dll + filesystem after Run.
     * Strip -dll after load: GoldSrc treats -dll as the game DLL
     * (GiveFnptrsToDll / mp.dll), not a launcher sidecar. */
    if (!LoadCommandLineDlls(dir, cmdline)) {
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1;
    }
    RemoveParm(cmdline, "-dll");
#endif

#ifdef HL_METAHHOOK
    if (!MetaHook_Startup(cmdline)) {
        Fail("MetaHook WSAStartup failed");
        if (mutex != NULL) {
            CloseHandle(mutex);
        }
        return 1;
    }
#endif

    result = ENGRUN_QUITTING;
    for (;;) {
        postRestart[0] = '\0';
        engineFile = (HasArg(cmdline, "-sw") || HasArg(cmdline, "-software")) ? "sw.dll" : "hw.dll";

#ifdef HL_METAHHOOK
        MetaHook_SetCmdLine(cmdline);
#endif

        fsModule = LoadGameLibrary(dir, "FileSystem_Stdio.dll");
        if (fsModule == NULL) {
            Fail("Can't find FileSystem_Stdio.dll");
            result = ENGRUN_UNSUPPORTED_VIDEOMODE;
            break;
        }
        fsFactory = ModuleFactory(fsModule);

#ifdef HL_METAHHOOK
        if (!MetaHook_BindFileSystem(fsModule, dir)) {
            Fail("MetaHook could not bind FileSystem_Stdio.dll");
            FreeLibrary(fsModule);
            result = ENGRUN_UNSUPPORTED_VIDEOMODE;
            break;
        }
#endif

        engineModule = LoadGameLibrary(dir, engineFile);
        if (engineModule == NULL || ModuleFactory(engineModule) == NULL) {
            Fail("Can't load engine DLL");
#ifdef HL_METAHHOOK
            MetaHook_UnbindFileSystem();
#endif
            FreeLibrary(fsModule);
            result = ENGRUN_UNSUPPORTED_VIDEOMODE;
            break;
        }
        engine = (IEngineAPI *)ModuleFactory(engineModule)(VENGINE_LAUNCHER_API_VERSION, NULL);
        if (engine == NULL) {
            Fail("CreateInterface(VENGINE_LAUNCHER_API_VERSION002) failed");
#ifdef HL_METAHHOOK
            MetaHook_UnbindFileSystem();
#endif
            FreeLibrary(engineModule);
            FreeLibrary(fsModule);
            result = ENGRUN_UNSUPPORTED_VIDEOMODE;
            break;
        }

#ifdef HL_METAHHOOK
        MetaHook_LoadEngine(engineModule, engineFile, dir);
        result = engine->Run(instance, dir, MetaHook_CmdLine(), postRestart,
                             (CreateInterfaceFn)MetaHook_GetFactory(), fsFactory);
        engine = NULL;
        MetaHook_ExitGame((int)result);
        FreeLibrary(engineModule);
        MetaHook_ShutdownPlugins();
        MetaHook_UnbindFileSystem();
        FreeLibrary(fsModule);
#else
        result = engine->Run(instance, dir, cmdline, postRestart, LauncherFactory, fsFactory);
        engine = NULL;
        FreeLibrary(engineModule);
        FreeLibrary(fsModule);
#endif

        if (result != ENGRUN_CHANGED_VIDEOMODE) {
            break;
        }
        MergePostRestart(cmdline, sizeof(cmdline), postRestart);
    }

#ifdef HL_METAHHOOK
    MetaHook_Finish();
#endif

    if (mutex != NULL) {
        CloseHandle(mutex);
    }
    return (result == ENGRUN_QUITTING) ? 0 : 1;
}
