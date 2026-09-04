#include "module.h"

#include <windows.h>
#include <stdio.h>

int Module_LoadVellum(const char *gameDir)
{
    char path[MAX_PATH];
    HMODULE module;
    void (WINAPI *init)(void);

    _snprintf(path, sizeof(path), "%s\\vellum.dll", gameDir);
    path[sizeof(path) - 1] = '\0';

    module = LoadLibraryA(path);
    if (module == NULL) {
        return 0;
    }
    init = (void (WINAPI *)(void))GetProcAddress(module, "Vellum_Init");
    if (init != NULL) {
        init();
    }
    return 1;
}
