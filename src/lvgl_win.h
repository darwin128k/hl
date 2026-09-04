#ifndef VELLUM_HOST_LVGL_WIN_H
#define VELLUM_HOST_LVGL_WIN_H

#include <windows.h>

typedef void (*UiConnectFn)(void);
typedef void (*UiQuitFn)(void);

int Ui_Init(HWND hwnd, const char *gameDir);
void Ui_SetHandlers(UiConnectFn onConnect, UiQuitFn onQuit);
void Ui_Resize(int width, int height);
void Ui_Tick(void);
void Ui_Paint(HDC hdc, int width, int height);
void Ui_OnMouse(int x, int y, int pressed);
void Ui_Shutdown(void);

#endif
