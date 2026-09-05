# hl

`hl.exe` is a thin `WinMain` over `HlLauncher_Run` in `src/launcher.cpp` (mutex, filesystem, `hw.dll` / `sw.dll`, `IEngineAPI::Run`). Other programs may compile those two files without this repo knowing about them.

Does not load Vellum, LVGL, or Steam. For this No-Steam install start the game through `cstrike.exe` (revloader).

## Build

```bat
build.bat
```

Needs Visual Studio 2022 (vcvars32), CMake, and Ninja. The script copies `hl.exe` to the game root.

## License

MIT. See [LICENSE](LICENSE).
