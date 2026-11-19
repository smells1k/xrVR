# xrVR — VR-мод для S.T.A.L.K.E.R. Тень Чернобыля (OGSR Engine)

Стерео-рендер в шлеме, трекинг головы 6DoF, масштаб 1:1.
Написан с нуля, **не использует другие моды**.
Единственная внешняя зависимость — официальный заголовок Valve `openvr.h`.

Целевой шлем: **Pico Neo 3** (через SteamVR + Pico Connect / Business Streaming
или ALVR). Работает с любым шлемом, который видит SteamVR.

Управление: **голова = взгляд, мышь/клавиатура = тело и стрельба**.

---

## Установка в три команды

```bat
git clone https://github.com/smells1k/xrVR.git
cd xrVR
python tools\apply_patches.py  C:\path\to\OGSR-Engine
```

Скрипт сам пропатчит 11 файлов движка, скопирует модуль, поправит три
`.vcxproj` и прогонит 211 проверок. В конце должно быть:

```
ИТОГО: 118 проверок пройдено, 0 провалено
ИТОГО: 93 проверок пройдено, 0 провалено
РЕЗУЛЬТАТ: ВСЁ ПРИМЕНЕНО
```

Дальше — открыть `Engine.sln`, собрать `Release|x64`.

Полная инструкция: **[README.md](README.md)** (сборка, настройка, устранение
неполадок, архитектура).

---

## Структура репозитория

```
src/xrVR/            исходники модуля (10 файлов, ~60 КБ)
  xrVR.h             публичный API + Config
  xrVR_api.cpp       состояние, конфиг, покадровые хуки
  xrVR_math.h        вся математика, не зависит от движка
  xrVR_openvr.h      таблица указателей OpenVRImpl (мост движок <-> рендер)
  xrVR_openvr_r4.cpp РЕАЛЬНАЯ реализация OpenVR (живёт в xrRender_R4)
  xrVR_openvr_null.cpp заглушка для XR_3DA/xrGame
  xrVR_render.h/.cpp стерео-проход, применение камеры глаза, submit
  xrVR_console.h/.cpp команды консоли
tools/
  apply_patches.py   ОДНА команда: патчи + интеграция + проверки
  patch_01..06_*.py  правки конкретных файлов движка (идемпотентны)
  integrate_vcxproj.py копирование модуля + правка .vcxproj
  verify_api.py      118 проверок целостности модуля
  verify_integration.py 93 проверки интеграции в движок
tests/
  vr_math_test.cpp      36 проверок математики
  vr_math_test2.cpp     34 проверки (Valve Index / Pico / Rift / Vive)
  engine_convention_test.cpp 9 проверок соглашений с X-Ray
  win_compile_test.cpp  проверка сборки под MSVC-совместимым компилятором
third_party/openvr/openvr.h   официальный заголовок Valve
gamedata/configs/vr/vr_config.ltx  конфиг мода
```

---

## Что сделано

| Возможность | Статус |
|---|---|
| Стерео-рендер, асимметричная проекция из данных шлема | ✅ |
| Трекинг головы 6DoF (yaw/pitch/roll + смещение) | ✅ |
| Масштаб мира 1:1 | ✅ |
| Стрельба по направлению ствола | ✅ |
| Виртуальный экран для меню/ПДА (SteamVR overlay) | ✅ |
| Команды консоли `vr_on/vr_off/vr_status/vr_recenter/...` | ✅ |
| Мягкое отключение, если SteamVR/DLL нет | ✅ |
| Motion-контроллеры, оружие в руках | ❌ (вне скоупа v1) |
| 3D-HUD вместо плоского экрана | ❌ (позже) |

---

## Тесты

```bash
cd tests
g++ -std=c++17 -O2 -Wall -Wextra -I../src/xrVR vr_math_test.cpp -o t1 && ./t1
g++ -std=c++17 -O2 -Wall -Wextra -I../src/xrVR vr_math_test2.cpp -o t2 && ./t2
g++ -std=c++17 -O2 -Wall -Wextra -I../src/xrVR engine_convention_test.cpp -o t3 && ./t3
```

Ожидается: `36 checks, 0 failures` / `34 checks, 0 failures` / `9 checks, 0 failures`.

Проверка модуля целиком (без движка):

```bash
python tools/verify_api.py
```

---

## Лицензии

- Код мода — оригинальный, написан с нуля для этого репозитория.
- `third_party/openvr/openvr.h` — © Valve Corporation, BSD-3-Clause.
- OGSR Engine — отдельный проект, см. его лицензию.
- S.T.A.L.K.E.R. — © GSC Game World. Нужна легальная копия игры.

## Готовый движок (без Visual Studio)

Сборка делается автоматически на GitHub Actions (windows-2022, MSVC,
Release x64). Последняя успешная: `XR_3DA.vcxproj -> bin_x64\xrEngine.exe`.

Если не хочешь ставить Visual Studio и ждать 20 минут компиляции:

1. Скачай `xrVR-bin-v0.1.4.zip` со страницы релизов
2. Скопируй `xrEngine.exe` и `openvr_api.dll` в `bin_x64\` твоей OGSR
3. Скопируй `gamedata\` из `xrVR-kit-v0.1.4.zip` в корень игры
4. Запускай

Если хочешь собрать сам — `install.bat` делает всё то же самое,
включая `Update_Components.cmd` (12 сторонних репозиториев).

## Статус проверки

| Что | Как проверено |
|---|---|
| Компиляция MSVC | GitHub Actions, windows-2022, успешно |
| `xrEngine.exe` | собран, 31 МБ, внутри `vr_status`/`vr_recenter`/`xrVR` |
| `install.bat` | запущен в настоящем `cmd.exe` (wine), все 7 шагов |
| Патчи движка | применены к чистому клону, 142 + 91 проверка |
| Запуск на шлеме | **НЕ проверено** — Pico Neo 3 в песочнице нет |
| Игра целиком | **НЕ проверено** — нужен установленный S.T.A.L.K.E.R. |
