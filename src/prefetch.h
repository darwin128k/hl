#ifndef VELLUM_HOST_PREFETCH_H
#define VELLUM_HOST_PREFETCH_H

#include <stddef.h>

void Prefetch_Start(const char *gameDir);
void Prefetch_Stop(void);
void Prefetch_GetUi(int *active, int *percent, char *name, size_t nameSize);

#endif
