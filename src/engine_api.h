#ifndef HL_ENGINE_API_H
#define HL_ENGINE_API_H

/* Public GoldSrc launcher API: VENGINE_LAUNCHER_API_VERSION002. */

class IBaseInterface
{
public:
    virtual ~IBaseInterface() {}
};

typedef IBaseInterface *(*CreateInterfaceFn)(const char *name, int *returnCode);

enum EngineRunResult
{
    ENGRUN_QUITTING = 0,
    ENGRUN_CHANGED_VIDEOMODE = 1,
    ENGRUN_UNSUPPORTED_VIDEOMODE = 2
};

class IEngineAPI : public IBaseInterface
{
public:
    virtual EngineRunResult Run(void *instance, const char *basedir, const char *cmdline,
                                char *postRestartCmdLine, CreateInterfaceFn launcherFactory,
                                CreateInterfaceFn filesystemFactory) = 0;
};

#define VENGINE_LAUNCHER_API_VERSION "VENGINE_LAUNCHER_API_VERSION002"

#endif
