#include "prefetch.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define QUERY_HOST "37.230.210.218"
#define QUERY_PORT 27015
#define FASTDL_BASE "http://37.230.210.218:17015/"
#define CURL_EXE "C:\\Windows\\System32\\curl.exe"

static HANDLE g_thread = NULL;
static volatile LONG g_stop = 0;
static volatile LONG g_active = 0;
static volatile LONG g_percent = 0;
static char g_label[128];
static CRITICAL_SECTION g_uiLock;
static int g_uiLockReady = 0;
static char g_gameDir[MAX_PATH];

static void EnsureParentDir(const char *filePath);

static void UiLockInit(void)
{
    if (!g_uiLockReady) {
        InitializeCriticalSection(&g_uiLock);
        g_uiLockReady = 1;
    }
}

static void SetUi(int active, int percent, const char *name)
{
    UiLockInit();
    InterlockedExchange(&g_active, active ? 1 : 0);
    if (percent < 0) {
        percent = 0;
    }
    if (percent > 100) {
        percent = 100;
    }
    InterlockedExchange(&g_percent, percent);
    EnterCriticalSection(&g_uiLock);
    if (name != NULL) {
        _snprintf(g_label, sizeof(g_label), "%s", name);
        g_label[sizeof(g_label) - 1] = '\0';
    }
    LeaveCriticalSection(&g_uiLock);
}

void Prefetch_GetUi(int *active, int *percent, char *name, size_t nameSize)
{
    UiLockInit();
    if (active != NULL) {
        *active = (int)InterlockedCompareExchange(&g_active, 0, 0);
    }
    if (percent != NULL) {
        *percent = (int)InterlockedCompareExchange(&g_percent, 0, 0);
    }
    if (name != NULL && nameSize > 0) {
        EnterCriticalSection(&g_uiLock);
        _snprintf(name, nameSize, "%s", g_label);
        name[nameSize - 1] = '\0';
        LeaveCriticalSection(&g_uiLock);
    }
}

static int FileExists(const char *path)
{
    DWORD attr = GetFileAttributesA(path);
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

static ULONGLONG FileSize(const char *path)
{
    WIN32_FILE_ATTRIBUTE_DATA info;
    ULARGE_INTEGER n;

    if (!GetFileAttributesExA(path, GetFileExInfoStandard, &info)) {
        return 0;
    }
    n.LowPart = info.nFileSizeLow;
    n.HighPart = info.nFileSizeHigh;
    return n.QuadPart;
}

static void Join(char *out, size_t outSize, const char *a, const char *b)
{
    size_t n = strlen(a);
    if (n > 0 && (a[n - 1] == '\\' || a[n - 1] == '/')) {
        _snprintf(out, outSize, "%s%s", a, b);
    } else {
        _snprintf(out, outSize, "%s\\%s", a, b);
    }
    out[outSize - 1] = '\0';
}

static void SlashToBack(char *path)
{
    char *p;
    for (p = path; *p != '\0'; p++) {
        if (*p == '/') {
            *p = '\\';
        }
    }
}

static void EnsureParentDir(const char *filePath)
{
    char dir[MAX_PATH];
    char *slash;

    _snprintf(dir, sizeof(dir), "%s", filePath);
    dir[sizeof(dir) - 1] = '\0';
    slash = strrchr(dir, '\\');
    if (slash == NULL) {
        return;
    }
    *slash = '\0';
    if (GetFileAttributesA(dir) == INVALID_FILE_ATTRIBUTES) {
        EnsureParentDir(dir);
        CreateDirectoryA(dir, NULL);
    }
}

static int HaveResource(const char *relPosix)
{
    char rel[MAX_PATH];
    char path[MAX_PATH];
    char cstrike[MAX_PATH];

    _snprintf(rel, sizeof(rel), "%s", relPosix);
    rel[sizeof(rel) - 1] = '\0';
    SlashToBack(rel);
    Join(cstrike, sizeof(cstrike), g_gameDir, "cstrike");
    Join(path, sizeof(path), cstrike, rel);
    if (FileExists(path)) {
        return 1;
    }
    _snprintf(path, sizeof(path), "%s\\cstrike_downloads\\%s", g_gameDir, rel);
    path[sizeof(path) - 1] = '\0';
    return FileExists(path);
}

static ULONGLONG CurlHeadLength(const char *url)
{
    char tmp[MAX_PATH];
    char cmd[2048];
    char line[512];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    FILE *f;
    ULONGLONG len = 0;

    _snprintf(tmp, sizeof(tmp), "%s\\cstrike_downloads\\_head.txt", g_gameDir);
    tmp[sizeof(tmp) - 1] = '\0';
    EnsureParentDir(tmp);
    _snprintf(cmd, sizeof(cmd),
              "\"%s\" -g -sI --max-time 8 --connect-timeout 5 -o \"%s\" \"%s\"",
              CURL_EXE, tmp, url);
    cmd[sizeof(cmd) - 1] = '\0';
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return 0;
    }
    WaitForSingleObject(pi.hProcess, 12000);
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (code != 0) {
        DeleteFileA(tmp);
        return 0;
    }
    f = fopen(tmp, "r");
    DeleteFileA(tmp);
    if (f == NULL) {
        return 0;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        if (_strnicmp(line, "Content-Length:", 15) == 0) {
            len = _strtoui64(line + 15, NULL, 10);
            break;
        }
    }
    fclose(f);
    return len;
}

static int RunCurl(const char *url, const char *dest, const char *label)
{
    char tmp[MAX_PATH];
    char cmd[2048];
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    DWORD code = 1;
    ULONGLONG total;
    DWORD wait;

    if (InterlockedCompareExchange(&g_stop, 0, 0) != 0) {
        return 0;
    }
    SetUi(1, 0, label);
    total = CurlHeadLength(url);
    _snprintf(tmp, sizeof(tmp), "%s.part", dest);
    tmp[sizeof(tmp) - 1] = '\0';
    EnsureParentDir(dest);
    DeleteFileA(tmp);

    _snprintf(cmd, sizeof(cmd),
              "\"%s\" -g -fsSL --max-time 180 --connect-timeout 8 -o \"%s\" \"%s\"",
              CURL_EXE, tmp, url);
    cmd[sizeof(cmd) - 1] = '\0';

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessA(NULL, cmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        return 0;
    }
    for (;;) {
        wait = WaitForSingleObject(pi.hProcess, 80);
        if (total > 0) {
            SetUi(1, (int)((FileSize(tmp) * 100) / total), label);
        }
        if (InterlockedCompareExchange(&g_stop, 0, 0) != 0) {
            TerminateProcess(pi.hProcess, 1);
            break;
        }
        if (wait != WAIT_TIMEOUT) {
            break;
        }
    }
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    if (code != 0 || !FileExists(tmp)) {
        DeleteFileA(tmp);
        return 0;
    }
    DeleteFileA(dest);
    if (!MoveFileA(tmp, dest)) {
        DeleteFileA(tmp);
        return 0;
    }
    SetUi(1, 100, label);
    return 1;
}

static int DownloadRel(const char *relPosix)
{
    char url[512];
    char dest[MAX_PATH];
    char rel[MAX_PATH];

    if (relPosix == NULL || relPosix[0] == '\0' || HaveResource(relPosix)) {
        return 1;
    }
    _snprintf(rel, sizeof(rel), "%s", relPosix);
    rel[sizeof(rel) - 1] = '\0';
    SlashToBack(rel);
    _snprintf(dest, sizeof(dest), "%s\\cstrike_downloads\\%s", g_gameDir, rel);
    dest[sizeof(dest) - 1] = '\0';
    _snprintf(url, sizeof(url), "%s%s", FASTDL_BASE, relPosix);
    url[sizeof(url) - 1] = '\0';
    return RunCurl(url, dest, relPosix);
}

static void Trim(char *line)
{
    size_t n;
    char *p = line;
    while (*p == ' ' || *p == '\t' || *p == '\r') {
        p++;
    }
    if (p != line) {
        memmove(line, p, strlen(p) + 1);
    }
    n = strlen(line);
    while (n > 0 && (line[n - 1] == ' ' || line[n - 1] == '\t' || line[n - 1] == '\r' || line[n - 1] == '\n')) {
        line[--n] = '\0';
    }
}

static void DownloadResList(const char *resPath)
{
    FILE *f;
    char line[512];

    f = fopen(resPath, "r");
    if (f == NULL) {
        return;
    }
    while (fgets(line, sizeof(line), f) != NULL) {
        Trim(line);
        if (line[0] == '\0' || line[0] == '/' || line[0] == '\\') {
            continue;
        }
        DownloadRel(line);
        if (InterlockedCompareExchange(&g_stop, 0, 0) != 0) {
            break;
        }
    }
    fclose(f);
}

static const char *SkipString(const char *p, const char *end)
{
    while (p < end && *p != '\0') {
        p++;
    }
    if (p < end) {
        p++;
    }
    return p;
}

static int QueryMapName(char *map, size_t mapSize)
{
    SOCKET sock;
    struct sockaddr_in addr;
    char req[64];
    char buf[2048];
    const char *p;
    const char *end;
    int n;
    int reqLen;

    map[0] = '\0';
    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET) {
        return 0;
    }
    n = 2500;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char *)&n, sizeof(n));
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(QUERY_PORT);
    inet_pton(AF_INET, QUERY_HOST, &addr.sin_addr);

    memcpy(req, "\xFF\xFF\xFF\xFFTSource Engine Query", 24);
    req[24] = 0;
    reqLen = 25;
    sendto(sock, req, reqLen, 0, (struct sockaddr *)&addr, sizeof(addr));
    n = recvfrom(sock, buf, sizeof(buf) - 1, 0, NULL, NULL);
    if (n >= 9 && (unsigned char)buf[4] == 0x41) {
        char req2[32];
        memcpy(req2, req, 25);
        memcpy(req2 + 25, buf + 5, 4);
        sendto(sock, req2, 29, 0, (struct sockaddr *)&addr, sizeof(addr));
        n = recvfrom(sock, buf, sizeof(buf) - 1, 0, NULL, NULL);
    }
    closesocket(sock);
    if (n < 8 || buf[4] != 'I') {
        return 0;
    }
    end = buf + n;
    p = SkipString(buf + 6, end);
    if (p >= end) {
        return 0;
    }
    _snprintf(map, mapSize, "%s", p);
    map[mapSize - 1] = '\0';
    return map[0] != '\0';
}

static DWORD WINAPI PrefetchThread(LPVOID unused)
{
    WSADATA wsa;
    char map[128];
    char rel[256];
    char resPath[MAX_PATH];
    char resRel[256];

    (void)unused;
    SetUi(1, 0, "Запрос карты...");
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        SetUi(0, 0, "");
        return 0;
    }
    if (!QueryMapName(map, sizeof(map))) {
        strncpy(map, "awp_india_winter", sizeof(map) - 1);
        map[sizeof(map) - 1] = '\0';
    }
    _snprintf(rel, sizeof(rel), "maps/%s.bsp", map);
    DownloadRel(rel);
    _snprintf(resRel, sizeof(resRel), "maps/%s.res", map);
    DownloadRel(resRel);
    _snprintf(resPath, sizeof(resPath), "%s\\cstrike_downloads\\maps\\%s.res", g_gameDir, map);
    resPath[sizeof(resPath) - 1] = '\0';
    if (!FileExists(resPath)) {
        _snprintf(resPath, sizeof(resPath), "%s\\cstrike\\maps\\%s.res", g_gameDir, map);
        resPath[sizeof(resPath) - 1] = '\0';
    }
    DownloadResList(resPath);
    WSACleanup();
    SetUi(0, 100, "");
    return 0;
}

void Prefetch_Start(const char *gameDir)
{
    if (g_thread != NULL) {
        return;
    }
    UiLockInit();
    _snprintf(g_gameDir, sizeof(g_gameDir), "%s", gameDir);
    g_gameDir[sizeof(g_gameDir) - 1] = '\0';
    InterlockedExchange(&g_stop, 0);
    SetUi(1, 0, "Загрузка...");
    g_thread = CreateThread(NULL, 0, PrefetchThread, NULL, 0, NULL);
}

void Prefetch_Stop(void)
{
    InterlockedExchange(&g_stop, 1);
    if (g_thread != NULL) {
        WaitForSingleObject(g_thread, 2000);
        CloseHandle(g_thread);
        g_thread = NULL;
    }
    SetUi(0, 0, "");
}
