#ifndef VELLUM_SHELL_H
#define VELLUM_SHELL_H

#include <windows.h>

int Shell_Create(HINSTANCE instance, const char *gameDir);
/* 1 = connect (address written), 0 = quit without starting the game. */
int Shell_Wait(char *address, size_t addressSize);
void Shell_GetClientSize(int *width, int *height);
void Shell_EmbedGame(void);
void Shell_Destroy(void);

#endif
