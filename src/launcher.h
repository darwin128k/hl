#ifndef HL_LAUNCHER_H
#define HL_LAUNCHER_H

#include <windows.h>

/* GoldSrc process entry used by hl.exe. cmdline is GetCommandLineA() or an
 * equivalent string the engine should parse (-game, -sw, ...).
 * With HL_LAUNCHER_DLLS, each -dll <name.dll> loads that basename from the
 * exe directory and calls Launcher_Init if the export exists. */
int HlLauncher_Run(HINSTANCE instance, const char *cmdline);

#endif
