#ifndef VELLUM_MODULE_H
#define VELLUM_MODULE_H

/* Load local vellum.dll (built from this repo) next to hl.exe. */
int Module_LoadVellum(const char *gameDir);

#endif
