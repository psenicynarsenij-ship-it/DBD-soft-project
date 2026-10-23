# Проверки первой стадии

Проверено в Linux-среде; приложение предназначено для Windows x64.

| Проверка | Результат |
|---|---|
| CMake configure + Release-сборка portable core_tests, GCC 14.2 | PASS |
| CTest core_tests | PASS, 1/1; выполнены проверки проекции, clipping, camera transform, distances и демоснимка |
| config_tests, нативная сборка GCC 14.2 с JSON 3.11.3 | PASS; round-trip, перезапись, обработка ошибок и выбор провайдера |
| CMake Windows x64 Release, MinGW-w64 GCC 14 posix | PASS; VisualESPOverlay.exe собран и слинкован |
| Инспекция exe | PE32+ x86-64, Windows GUI; runtime MinGW статически слинкован |
| Windows-версии core_tests и config_tests | Собраны; запуск не выполнялся |
| Нативная сборка MSVC / Visual Studio 2022 | Не выполнялась в Linux; preset и команды включены |
| DWM/DX11 display, alpha, click-through, hotkeys, меню | Реализованы; визуальная проверка на Windows не выполнялась |
| Подключение к DBD 10.2.2 | Не реализовано; только DEMO, реальные адреса не заданы |

Графический ручной сценарий и команды воспроизведения находятся в README.md. Успешная кросс-сборка не заменяет запуск на Windows.
