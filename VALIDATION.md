# Проверки версии 0.3.1

Основа — слитый PR #2, merge-коммит b6db33aa74435f8bec5aacab7d96dcca2b832f06. Текущая ветка: feature/dbd-supported-source-assessment. Исходные функции версии 0.3 сохранены.

| Проверка | Результат |
|---|---|
| Native GCC 14.2 Release, CTest | PASS: 11/11 |
| core_tests | Проекция, clipping, камера/FOV, направления и сектор demo killer |
| config_tests | Все настройки, runtime options, обратная совместимость config.json, ошибки и диапазоны |
| frame_serialization | Полный round-trip Frame, optional direction, UTF-8, uint64, пустая сцена |
| frame_validation | Координаты/NaN/Inf, вырожденные матрицы, ID/дубликаты, типы, направления, строки, размер/глубина JSON |
| frame_concurrency | Четыре одновременных читателя и worker, отсутствие разорванных снимков, неизменяемость удержанного Frame, concurrent Stop |
| frame_recovery | Исчезновение объектов, stale timeout, удаление/повреждение файла и восстановление, явный fallback; expiry при задержке worker |
| frame_recording | Timestamp scheduling, append-сессии, loop/EOF, некорректная запись и восстановление, автоматическая запись runtime, отдельная ошибка записи |
| frame_scale | Сериализация и чтение 10000 объектов; повторное чтение неизменённого файла пропускается |
| render_budget_and_killer_view | CPU ImGui: 10000 кандидатов/25 отрисованных, переключатели, геометрия стрелки/сектора, отсутствие направления |
| dbd_transport_is_not_game_connection | PASS: все транспорты не подтверждают DBD; JSON с ложными source/connected/dbd.verified, потеря файла, Stop и Demo fallback сохраняют NOT CONNECTED |
| dbd_cli_expected_unavailable | PASS: проверены точный exit code 3, scope binary_capabilities и ложные connected/adapter/session flags |
| CLI smoke 0.3.1 | validate примера (11 объектов), record/validate 3 синтетических кадров, dbd-status с точным exit 3 — PASS |
| Windows x64 Release MinGW-w64 | VisualESPOverlay.exe, frame_tool.exe и пять test exe собраны и слинкованы |
| Инспекция поставляемых exe | PE32+ x86-64; только системные DLL, runtime MinGW статически включён |
| Запуск GUI Windows / нативный MSVC | Не выполнялся в Linux; ручной сценарий и команды VS2022 в README |
| Подключение к процессу DBD | Не реализовано и не заявляется; источник JSON непроверенного происхождения явно маркируется |

Native-сборка использовала ESP_BUILD_OVERLAY=OFF, ESP_BUILD_CONFIG_TESTS=ON, ESP_BUILD_RENDER_TESTS=ON, ESP_BUILD_TOOLS=ON, Release и существующие закреплённые зависимости через FETCHCONTENT_SOURCE_DIR_JSON/IMGUI. Windows-сборка использовала тот же код и MinGW x64 toolchain. Windows test exe собраны, но не запускались. CPU ImGui тест не проверяет DWM, alpha, click-through или аппаратный Present.

## Источники реальных данных

Подходящий документированный spatial feed в просмотренных материалах не найден. Актуальные страницы BHVR/Steamworks/Overwolf не прочитаны из-за proxy CONNECT 403; это не ответ origin и не доказательство отсутствия всех API. Прочитаны архивные официальные Overwolf docs (2024), Valve SDK declarations в community mirror и сторонние API README. Ни текущая совместимость с 10.2.2, ни реальные позиции/камера/направление не подтверждены. Полный отчёт: docs/DBD_DATA_SOURCES.md; точные URLs/SHA/ошибки: docs/dbd-source-evidence.json.

Реальное подключение к DBD не тестировалось. Выполнены только перечисленные offline/transport проверки и кросс-сборка. Меню, категории, FOV, стрелка/сектор, hotkeys и click-through не перепроектировались; в HUD добавлена строка NOT CONNECTED.

## Ранее измеренная обработка большого снимка

`frame_tool benchmark 10000 10`, результат версии 0.3 из предыдущего PR, Linux Release, без параллельной компиляции (в 0.3.1 benchmark не повторялся):

```text
Objects=10000 bytes=1597390 encode+decode+validation mean_ms=101.732 iterations=10
```

Это время сериализации + десериализации + проверок, а не render FPS и не гарантия частоты внешнего источника. JSON-worker может обновляться реже renderer; Latest передаёт только последний снимок и не создаёт очередь. Нагрузка отрисовки отдельно ограничивается Draw budget.

Архив Windows содержит overlay, frame_tool, примеры, offsets.json, docs с отчётом исследования и лицензии; исходный ZIP исключает build и downloads. При упаковке проверяются CRC всех записей и побайтовое соответствие исходникам и собранным exe.
