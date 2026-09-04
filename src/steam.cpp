#include "steam.h"

#include <windows.h>
#include <shellapi.h>

#include <stdio.h>
#include <string.h>

#define STEAM_IPC_MAPPING_NAME "Local\\SteamStart_SharedMemFile"
#define STEAM_IPC_EVENT_NAME   "Local\\SteamStart_SharedMemLock"
#define STEAM_IPC_SIZE         0x400
#define ACTIVE_PROCESS_KEY     "Software\\Valve\\Steam\\ActiveProcess"
#define DEFAULT_STEAM_APPID    "10"

struct SteamIpc {
    HANDLE mapping;
    void *view;
    HANDLE lockEvent;
};

static SteamIpc g_ipc;
static char g_dir[MAX_PATH];
static char g_appId[256];
static int g_started = 0;

static void Fail(const char *text)
{
    MessageBoxA(NULL, text, "Error", MB_OK | MB_ICONERROR);
}

static void JoinPath(char *out, size_t outSize, const char *dir, const char *file)
{
    size_t n = strlen(dir);
    if (n > 0 && (dir[n - 1] == '\\' || dir[n - 1] == '/')) {
        _snprintf(out, outSize, "%s%s", dir, file);
    } else {
        _snprintf(out, outSize, "%s\\%s", dir, file);
    }
    out[outSize - 1] = '\0';
}

static void WriteActiveProcess(DWORD pid, const char *steamClientDll)
{
    HKEY key = NULL;
    DWORD disp = 0;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, ACTIVE_PROCESS_KEY, 0, KEY_WRITE, &key) != ERROR_SUCCESS) {
        if (RegCreateKeyExA(HKEY_CURRENT_USER, ACTIVE_PROCESS_KEY, 0, NULL, 0, KEY_WRITE,
                            NULL, &key, &disp) != ERROR_SUCCESS) {
            return;
        }
    }
    RegSetValueExA(key, "pid", 0, REG_DWORD, (const BYTE *)&pid, sizeof(pid));
    if (steamClientDll != NULL && steamClientDll[0] != '\0') {
        RegSetValueExA(key, "SteamClientDll", 0, REG_SZ,
                       (const BYTE *)steamClientDll, (DWORD)strlen(steamClientDll) + 1);
    }
    RegCloseKey(key);
}

static int SetupSteamIpc(SteamIpc *ipc)
{
    memset(ipc, 0, sizeof(*ipc));
    ipc->mapping = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0,
                                      STEAM_IPC_SIZE, STEAM_IPC_MAPPING_NAME);
    if (ipc->mapping == NULL) {
        return 0;
    }
    ipc->view = MapViewOfFile(ipc->mapping, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (ipc->view == NULL) {
        CloseHandle(ipc->mapping);
        ipc->mapping = NULL;
        return 0;
    }
    ipc->lockEvent = CreateEventA(NULL, FALSE, FALSE, STEAM_IPC_EVENT_NAME);
    if (ipc->lockEvent == NULL) {
        UnmapViewOfFile(ipc->view);
        CloseHandle(ipc->mapping);
        memset(ipc, 0, sizeof(*ipc));
        return 0;
    }
    return 1;
}

static void TeardownSteamIpc(SteamIpc *ipc)
{
    if (ipc->lockEvent != NULL) {
        CloseHandle(ipc->lockEvent);
    }
    if (ipc->view != NULL) {
        UnmapViewOfFile(ipc->view);
    }
    if (ipc->mapping != NULL) {
        CloseHandle(ipc->mapping);
    }
    memset(ipc, 0, sizeof(*ipc));
}

static int ReadSteamAppId(const char *dir, char *out, size_t outSize)
{
    char path[MAX_PATH];
    FILE *f;
    size_t n;

    JoinPath(path, sizeof(path), dir, "steam_appid.txt");
    f = fopen(path, "r");
    if (f == NULL) {
        return 0;
    }
    n = fread(out, 1, outSize - 1, f);
    fclose(f);
    out[n] = '\0';
    while (n > 0 && (out[n - 1] == '\n' || out[n - 1] == '\r' || out[n - 1] == ' ')) {
        out[--n] = '\0';
    }
    return n > 0;
}

static void WriteSteamAppId(const char *dir, const char *appId)
{
    char path[MAX_PATH];
    FILE *f;

    if (appId == NULL || appId[0] == '\0') {
        return;
    }
    JoinPath(path, sizeof(path), dir, "steam_appid.txt");
    f = fopen(path, "w");
    if (f == NULL) {
        return;
    }
    fprintf(f, "%s\n", appId);
    fclose(f);
}

int Steam_Start(const char *dir)
{
    char iniPath[MAX_PATH];
    char steamDll[MAX_PATH];
    char steamClient[MAX_PATH];
    char iniClient[256];
    int argc = 0;
    LPWSTR *argvW;
    int i;

    memset(&g_ipc, 0, sizeof(g_ipc));
    g_appId[0] = '\0';
    _snprintf(g_dir, sizeof(g_dir), "%s", dir);
    g_dir[sizeof(g_dir) - 1] = '\0';

    argvW = CommandLineToArgvW(GetCommandLineW(), &argc);
    if (argvW != NULL) {
        for (i = 1; i < argc; i++) {
            if (_wcsicmp(argvW[i], L"-appid") == 0 && i + 1 < argc) {
                WideCharToMultiByte(CP_ACP, 0, argvW[++i], -1, g_appId, (int)sizeof(g_appId), NULL, NULL);
                g_appId[sizeof(g_appId) - 1] = '\0';
            }
        }
        LocalFree(argvW);
    }

    if (g_appId[0] == '\0' && !ReadSteamAppId(g_dir, g_appId, sizeof(g_appId))) {
        strncpy(g_appId, DEFAULT_STEAM_APPID, sizeof(g_appId) - 1);
        g_appId[sizeof(g_appId) - 1] = '\0';
    }

    SetEnvironmentVariableA("SteamGameId", g_appId);
    SetEnvironmentVariableA("SteamAppId", g_appId);
    WriteSteamAppId(g_dir, g_appId);

    JoinPath(iniPath, sizeof(iniPath), g_dir, "rev.ini");
    iniClient[0] = '\0';
    GetPrivateProfileStringA("Loader", "SteamClientDll", "", iniClient, sizeof(iniClient), iniPath);
    steamDll[0] = '\0';
    if (iniClient[0] != '\0') {
        if (strchr(iniClient, '\\') != NULL || strchr(iniClient, '/') != NULL) {
            strncpy(steamDll, iniClient, sizeof(steamDll) - 1);
        } else {
            JoinPath(steamDll, sizeof(steamDll), g_dir, iniClient);
        }
    } else {
        JoinPath(steamDll, sizeof(steamDll), g_dir, "steam.dll");
    }
    JoinPath(steamClient, sizeof(steamClient), g_dir, "steamclient.dll");

    if (!SetupSteamIpc(&g_ipc)) {
        Fail("Unable to set up Steam IPC");
        return 0;
    }
    if (LoadLibraryA(steamDll) == NULL) {
        Fail("Can't find steam.dll");
        TeardownSteamIpc(&g_ipc);
        return 0;
    }
    if (GetFileAttributesA(steamClient) == INVALID_FILE_ATTRIBUTES) {
        Fail("Can't find steamclient.dll");
        TeardownSteamIpc(&g_ipc);
        return 0;
    }
    WriteActiveProcess(GetCurrentProcessId(), steamClient);
    g_started = 1;
    return 1;
}

void Steam_Stop(void)
{
    if (!g_started) {
        return;
    }
    WriteSteamAppId(g_dir, g_appId);
    TeardownSteamIpc(&g_ipc);
    g_started = 0;
}
