# hl

`hl.exe` для этой сборки CS 1.6: окно Vellum (LVGL), Steam/RevEmu, затем `FileSystem_Stdio.dll` + `hw.dll` / `sw.dll` и `IEngineAPI::Run`.

Запуск из корня игры: `hl.exe`.

## Сборка

32-bit MSVC.

```bat
build.bat
```

Нужны Visual Studio 2022 (vcvars32), CMake, Ninja и junction `lvgl` → `H:\Vellum\engine\lvgl`. Скрипт кладёт `hl.exe` в корень игры.

## Лицензия

MIT. См. [LICENSE](LICENSE).
