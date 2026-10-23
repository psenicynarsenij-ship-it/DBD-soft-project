# Проверки версии 0.3

PR #1 проверен исходными двумя CTest и слит в work. Ветка feature/runtime-frame-sources основана на merge-коммите dfdad591ecada6cc9e3907351e399e80a43870e5.

| Проверка | Результат |
|---|---|
| Native GCC 14.2 Release, CTest | PASS: 9/9 |
| core_tests | Проекция, clipping, камера/FOV, направления и сектор demo killer |
| config_tests | Все настройки, runtime options, обратная совместимость config.json, ошибки и диапазоны |
| frame_serialization | Полный round-trip Frame, optional direction, UTF-8, uint64, пустая сцена |
| frame_validation | Координаты/NaN/Inf, вырожденные матрицы, ID/дубликаты, типы, направления, строки, размер/глубина JSON |
| frame_concurrency | Четыре одновременных читателя и worker, отсутствие разорванных снимков, неизменяемость удержанного Frame, concurrent Stop |
| frame_recovery | Исчезновение объектов, stale timeout, удаление/повреждение файла и восстановление, явный fallback; expiry при задержке worker |
| frame_recording | Timestamp scheduling, append-сессии, loop/EOF, некорректная запись и восстановление, автоматическая запись runtime, отдельная ошибка записи |
| frame_scale | Сериализация и чтение 10000 объектов; повторное чтение неизменённого файла пропускается |
| render_budget_and_killer_view | CPU ImGui: 10000 кандидатов/25 отрисованных, переключатели, геометрия стрелки/сектора, отсутствие направления |
| CLI smoke | generate/validate, publish 4 кадров по 100 объектов, record/validate 12 кадров по 11 объектов — PASS |
| Windows x64 Release MinGW-w64 | VisualESPOverlay.exe, frame_tool.exe и четыре test exe собраны и слинкованы |
| Инспекция поставляемых exe | PE32+ x86-64; только системные DLL, runtime MinGW статически включён |
| Запуск GUI Windows / нативный MSVC | Не выполнялся в Linux; ручной сценарий и команды VS2022 в README |
| Подключение к процессу DBD | Не реализовано и не заявляется; источник JSON непроверенного происхождения явно маркируется |

Native-сборка использовала ESP_BUILD_OVERLAY=OFF, ESP_BUILD_CONFIG_TESTS=ON, ESP_BUILD_RENDER_TESTS=ON, ESP_BUILD_TOOLS=ON, Release и существующие закреплённые зависимости через FETCHCONTENT_SOURCE_DIR_JSON/IMGUI. Windows-сборка использовала тот же код и MinGW x64 toolchain. Windows test exe собраны, но не запускались. CPU ImGui тест не проверяет DWM, alpha, click-through или аппаратный Present.

## Измерение обработки большого снимка

`frame_tool benchmark 10000 10`, Linux Release, без параллельной компиляции:

```text
Objects=10000 bytes=1597390 encode+decode+validation mean_ms=101.732 iterations=10
```

Это время сериализации + десериализации + проверок, а не render FPS и не гарантия частоты внешнего источника. JSON-worker может обновляться реже renderer; Latest передаёт только последний снимок и не создаёт очередь. Нагрузка отрисовки отдельно ограничивается Draw budget.

Архив Windows содержит overlay, frame_tool, примеры, offsets.json и лицензии; исходный ZIP исключает build и downloads. При упаковке проверяются CRC всех записей и побайтовое соответствие исходникам и собранным exe.
