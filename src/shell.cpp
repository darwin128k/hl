#include "shell.h"
#include "lvgl_win.h"

#include <windows.h>
#include <objidl.h>
#include <gdiplus.h>

#include <stdio.h>
#include <string.h>

#define FRAME_CLASS L"VellumFrame"
#define GAME_SERVER "37.230.210.218:27015"
#define UI_TIMER 1

static HINSTANCE g_instance = NULL;
static char g_gameDir[MAX_PATH];
static HWND g_frame = NULL;
static ULONG_PTR g_gdiplus = 0;
static HANDLE g_thread = NULL;
static HANDLE g_ready = NULL;
static HANDLE g_choice = NULL;
static DWORD g_threadId = 0;
static volatile LONG g_result = 0;
static volatile LONG g_embed = 0;
static char g_address[256];
static HWND g_sdl = NULL;

static void Finish(LONG result)
{
    InterlockedExchange(&g_result, result);
    if (g_choice != NULL) {
        SetEvent(g_choice);
    }
}

static void OnConnect(void)
{
    _snprintf(g_address, sizeof(g_address), "%s", GAME_SERVER);
    g_address[sizeof(g_address) - 1] = '\0';
    Finish(1);
}

static BOOL CALLBACK FindSdlProc(HWND hwnd, LPARAM lParam)
{
    char className[64];
    DWORD pid = 0;
    RECT rc;

    if (hwnd == g_frame) {
        return TRUE;
    }
    GetWindowThreadProcessId(hwnd, &pid);
    if (pid != GetCurrentProcessId()) {
        return TRUE;
    }
    className[0] = '\0';
    GetClassNameA(hwnd, className, sizeof(className));
    if (lstrcmpiA(className, "SDL_app") != 0) {
        return TRUE;
    }
    memset(&rc, 0, sizeof(rc));
    GetClientRect(hwnd, &rc);
    if (rc.right <= 0 || rc.bottom <= 0) {
        return TRUE;
    }
    *(HWND *)lParam = hwnd;
    return FALSE;
}

static HWND FindSdl(void)
{
    HWND found = NULL;
    EnumWindows(FindSdlProc, (LPARAM)&found);
    if (found == NULL && g_frame != NULL) {
        EnumChildWindows(g_frame, FindSdlProc, (LPARAM)&found);
    }
    return found;
}

static void LayoutGame(void)
{
    RECT rc;

    if (g_frame == NULL || g_sdl == NULL || !IsWindow(g_sdl)) {
        return;
    }
    GetClientRect(g_frame, &rc);
    MoveWindow(g_sdl, 0, 0, rc.right, rc.bottom, TRUE);
}

static void FitSdl(void)
{
    HWND sdl;
    LONG style;
    LONG ex;

    if (InterlockedCompareExchange(&g_embed, 0, 0) == 0 || g_frame == NULL) {
        return;
    }
    sdl = FindSdl();
    if (sdl == NULL) {
        return;
    }
    if (g_sdl != sdl) {
        style = GetWindowLongA(sdl, GWL_STYLE);
        style &= ~(WS_POPUP | WS_CAPTION | WS_THICKFRAME | WS_SYSMENU |
                   WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_OVERLAPPEDWINDOW);
        style |= WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
        SetWindowLongA(sdl, GWL_STYLE, style);
        ex = GetWindowLongA(sdl, GWL_EXSTYLE);
        ex &= ~(WS_EX_APPWINDOW | WS_EX_TOOLWINDOW);
        SetWindowLongA(sdl, GWL_EXSTYLE, ex);
        SetParent(sdl, g_frame);
        SetWindowPos(sdl, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        g_sdl = sdl;
    }
    ShowWindow(sdl, SW_SHOW);
    LayoutGame();
}

static void OnQuit(void)
{
    if (g_frame != NULL) {
        PostMessageW(g_frame, WM_CLOSE, 0, 0);
    }
}

static LRESULT CALLBACK FrameProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_CREATE: {
        RECT rc;
        GetClientRect(hwnd, &rc);
        Ui_Init(hwnd, g_gameDir);
        Ui_SetHandlers(OnConnect, OnQuit);
        Ui_Resize(rc.right > 0 ? rc.right : GetSystemMetrics(SM_CXSCREEN),
                  rc.bottom > 0 ? rc.bottom : GetSystemMetrics(SM_CYSCREEN));
        SetTimer(hwnd, UI_TIMER, 16, NULL);
        return 0;
    }
    case WM_SIZE:
        if (wParam != SIZE_MINIMIZED) {
            if (InterlockedCompareExchange(&g_embed, 0, 0) == 0) {
                Ui_Resize(LOWORD(lParam), HIWORD(lParam));
            } else {
                LayoutGame();
            }
        }
        return 0;
    case WM_TIMER:
        if (wParam == UI_TIMER) {
            if (InterlockedCompareExchange(&g_embed, 0, 0) == 0) {
                Ui_Tick();
            } else {
                FitSdl();
            }
        }
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        RECT rc;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (InterlockedCompareExchange(&g_embed, 0, 0) == 0) {
            GetClientRect(hwnd, &rc);
            Ui_Paint(hdc, rc.right, rc.bottom);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_MOUSEMOVE:
        Ui_OnMouse((short)LOWORD(lParam), (short)HIWORD(lParam), -1);
        return 0;
    case WM_LBUTTONDOWN:
        SetCapture(hwnd);
        Ui_OnMouse((short)LOWORD(lParam), (short)HIWORD(lParam), 1);
        return 0;
    case WM_LBUTTONUP:
        Ui_OnMouse((short)LOWORD(lParam), (short)HIWORD(lParam), 0);
        ReleaseCapture();
        return 0;
    case WM_CLOSE:
        if (g_sdl != NULL && IsWindow(g_sdl)) {
            PostMessageA(g_sdl, WM_CLOSE, 0, 0);
        }
        DestroyWindow(hwnd);
        return 0;
    case WM_DESTROY:
        KillTimer(hwnd, UI_TIMER);
        Ui_Shutdown();
        g_frame = NULL;
        Finish(0);
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

static DWORD WINAPI ShellThread(LPVOID unused)
{
    WNDCLASSW wc;
    Gdiplus::GdiplusStartupInput gdipIn;
    MSG msg;

    (void)unused;
    Gdiplus::GdiplusStartup(&g_gdiplus, &gdipIn, NULL);

    memset(&wc, 0, sizeof(wc));
    wc.lpfnWndProc = FrameProc;
    wc.hInstance = g_instance;
    wc.lpszClassName = FRAME_CLASS;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassW(&wc);

    {
        int cx = GetSystemMetrics(SM_CXSCREEN);
        int cy = GetSystemMetrics(SM_CYSCREEN);
        if (cx < 640) {
            cx = 640;
        }
        if (cy < 480) {
            cy = 480;
        }
        g_frame = CreateWindowExW(WS_EX_APPWINDOW, FRAME_CLASS, L"Vellum",
                                  WS_POPUP | WS_VISIBLE | WS_CLIPCHILDREN,
                                  0, 0, cx, cy,
                                  NULL, NULL, g_instance, NULL);
    }
    if (g_frame == NULL) {
        SetEvent(g_ready);
        Finish(0);
        return 0;
    }

    ShowWindow(g_frame, SW_SHOW);
    UpdateWindow(g_frame);
    SetEvent(g_ready);

    while (GetMessageW(&msg, NULL, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return 0;
}

int Shell_Create(HINSTANCE instance, const char *gameDir)
{
    g_instance = instance;
    _snprintf(g_gameDir, sizeof(g_gameDir), "%s", gameDir);
    g_gameDir[sizeof(g_gameDir) - 1] = '\0';
    g_address[0] = '\0';
    InterlockedExchange(&g_result, 0);
    InterlockedExchange(&g_embed, 0);
    g_sdl = NULL;

    g_ready = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_choice = CreateEventW(NULL, TRUE, FALSE, NULL);
    g_thread = CreateThread(NULL, 0, ShellThread, NULL, 0, &g_threadId);
    if (g_thread == NULL) {
        return 0;
    }
    WaitForSingleObject(g_ready, 5000);
    return g_frame != NULL;
}

int Shell_Wait(char *address, size_t addressSize)
{
    if (g_choice == NULL) {
        return 0;
    }
    WaitForSingleObject(g_choice, INFINITE);
    if (InterlockedCompareExchange(&g_result, 0, 0) != 1) {
        return 0;
    }
    if (address != NULL && addressSize > 0) {
        _snprintf(address, addressSize, "%s", g_address);
        address[addressSize - 1] = '\0';
    }
    return 1;
}

void Shell_GetClientSize(int *width, int *height)
{
    RECT rc;
    int cx;
    int cy;

    cx = GetSystemMetrics(SM_CXSCREEN);
    cy = GetSystemMetrics(SM_CYSCREEN);
    if (g_frame != NULL && IsWindow(g_frame)) {
        GetClientRect(g_frame, &rc);
        if (rc.right > 0 && rc.bottom > 0) {
            cx = rc.right;
            cy = rc.bottom;
        }
    }
    if (width != NULL) {
        *width = cx;
    }
    if (height != NULL) {
        *height = cy;
    }
}

void Shell_EmbedGame(void)
{
    HWND frame = g_frame;
    LONG style;

    InterlockedExchange(&g_embed, 1);
    if (frame != NULL && IsWindow(frame)) {
        style = GetWindowLongA(frame, GWL_STYLE);
        style |= WS_CLIPCHILDREN;
        SetWindowLongA(frame, GWL_STYLE, style);
        ShowWindow(frame, SW_SHOW);
    }
}

void Shell_Destroy(void)
{
    if (g_threadId != 0) {
        PostThreadMessageW(g_threadId, WM_QUIT, 0, 0);
    }
    if (g_frame != NULL && IsWindow(g_frame)) {
        PostMessageW(g_frame, WM_CLOSE, 0, 0);
    }
    if (g_thread != NULL) {
        WaitForSingleObject(g_thread, 3000);
        CloseHandle(g_thread);
        g_thread = NULL;
    }
    if (g_ready != NULL) {
        CloseHandle(g_ready);
        g_ready = NULL;
    }
    if (g_choice != NULL) {
        CloseHandle(g_choice);
        g_choice = NULL;
    }
    g_frame = NULL;
    if (g_gdiplus != 0) {
        Gdiplus::GdiplusShutdown(g_gdiplus);
        g_gdiplus = 0;
    }
}
