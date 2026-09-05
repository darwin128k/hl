# hl

32-bit GoldSrc launcher stub (`hl.exe`): mutex, `FileSystem_Stdio.dll`, then `hw.dll` / `sw.dll` and `IEngineAPI::Run`.

Does not load Vellum, LVGL, or Steam. For this No-Steam install start the game through `cstrike.exe` (revloader).

## Build

```bat
build.bat
```

Needs Visual Studio 2022 (vcvars32), CMake, and Ninja. The script copies `hl.exe` to the game root.

## License

MIT. See [LICENSE](LICENSE).
