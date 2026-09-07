# hl

`hl.exe` is a thin `WinMain` over `HlLauncher_Run` in `src/launcher.cpp` (mutex, filesystem, `hw.dll` / `sw.dll`, `IEngineAPI::Run`). Other programs may compile those two files without this repo knowing about them.

`hl.exe` is a thin `WinMain` over `HlLauncher_Run` in `src/launcher.cpp` (mutex, filesystem, `hw.dll` / `sw.dll`, `IEngineAPI::Run`). Other programs may compile those two files without this repo knowing about them.

Does not load extra mods, LVGL, or Steam. Optional `-dll name.dll` (compile out with `-DHL_LAUNCHER_DLLS=OFF`). Optional MetaHookSv embed (`build.bat metahook`, `-DHL_METAHHOOK=ON`) — run `prepare-metahook.bat` first. For this No-Steam install start the game through `cstrike.exe` (revloader).

## Build

```bat
build.bat
```

Needs Visual Studio 2022 (vcvars32), CMake, and Ninja. The script copies `hl.exe` to the game root.

## License

MIT. See [LICENSE](LICENSE).
