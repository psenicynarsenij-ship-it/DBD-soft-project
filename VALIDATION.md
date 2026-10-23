# Проверки demo 0.2: Camera FOV и направление взгляда

Проверено в Linux-среде; приложение предназначено для Windows x64.

| Проверка | Результат |
|---|---|
| CMake configure + Release-сборка core_tests и config_tests, GCC 14.2 | PASS |
| CTest | PASS, 2/2: core_tests и config_tests |
| Математика направления/FOV | PASS: градусы/радианы, единичный yaw, плавность поворота, границы сектора 30/90/120°, длина, проекция при FOV 60/90/120°, передача настройки в DemoProvider |
| Конфигурация JSON 3.11.3 | PASS: все новые поля, старые config.json, границы FOV/угла, NaN, некорректный цвет и сохранность текущих настроек при ошибках |
| CMake Windows x64 Release, MinGW-w64 GCC 14 posix | PASS; VisualESPOverlay.exe собран и слинкован |
| Инспекция exe | PE32+ x86-64, Windows GUI; runtime MinGW статически слинкован |
| Windows-версии core_tests и config_tests | Собраны; запуск не выполнялся |
| Нативная сборка MSVC / Visual Studio 2022 | Не выполнялась в Linux; preset и команды включены |
| DWM/DX11 display, alpha, click-through, hotkeys, меню, стрелка и сектор | Реализованы; визуальная проверка на Windows не выполнялась |
| Подключение к DBD 10.2.2 | Не реализовано; только DEMO, реальные адреса не заданы |

Графический ручной сценарий и команды воспроизведения находятся в README.md. Успешная кросс-сборка не заменяет запуск на Windows.

При native-конфигурировании использована ранее полученная закреплённая JSON-зависимость через `FETCHCONTENT_SOURCE_DIR_JSON=build/windows/_deps/json-src` (абсолютный путь в текущей машине). Это позволяет выполнить оба CTest без повторной загрузки. Использовались `ESP_BUILD_OVERLAY=OFF`, `ESP_BUILD_CONFIG_TESTS=ON`, Release. Windows-сборка использовала существующий MinGW-w64 toolchain; оба тестовых exe также пересобраны, но не запускались.
