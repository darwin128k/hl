# hl

Тонкая замена `hl.exe` для GoldSrc / CS 1.6. Это процесс игры: `FileSystem_Stdio.dll`, опциональный `vellum.dll` (`Vellum_Init`), затем `hw.dll` / `sw.dll` и `IEngineAPI::Run`.

Steam/RevEmu сюда не входят. Их поднимает отдельный лоадер (`cstrike.exe` / [revloader](https://github.com/darwin128k/revloader)) и уже он запускает этот `hl.exe`. Прямой запуск без лоадера обычно падает на `SteamAPI_Init`.

## Сборка

32-bit MSVC (движок x86).

```bat
build.bat
```

Нужны Visual Studio 2022 (vcvars32), CMake и Ninja. Скрипт кладёт `hl.exe` в корень игры (`hw.dll` на уровень выше или на два, если репозиторий лежит в `vellum\host`) и один раз копирует прежний exe в `orig\`.

Сборка руками:

```bat
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -B build
cmake --build build
```

## Запуск

Из корня игры, через лоадер:

```bat
cstrike.exe
```

## Лицензия

MIT. См. [LICENSE](LICENSE).
