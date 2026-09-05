#include "launcher.h"

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prev, LPSTR cmd, int show)
{
    (void)prev;
    (void)cmd;
    (void)show;
    return HlLauncher_Run(instance, GetCommandLineA());
}
