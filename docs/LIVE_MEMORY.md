# Контракт внешнего чтения памяти

Это универсальный read-only reader для Windows x64 и отдельно контролируемый стенд. Layout DBD 10.2.2 не известен и не придуман. `DBD: NOT CONNECTED` остаётся видимым, в том числе при успешном чтении стенда. Исследование поддерживаемых API из PR #3 зафиксировано без новых поисков.

## Разделение компонентов

- `ProcessReader`: RAII HANDLE, поиск по точному имени exe и необязательному PID, отказ при неоднозначности, проверка имени после открытия, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ. PID не заменяет handle идентичности процесса. Проверка x64/PE32+, получение module base/size/timestamp, безопасное разрешение data export по PE, запрет forwarded export. Никаких прав записи, привилегий debug, инъекций, драйверов или обходов защиты.
- `IMemoryReader`: read-exact uint64 адреса; отдельное исключение содержит адрес, требуемый размер, реально прочитанные байты и Win32 code. До RPM проверяются диапазон user-mode адресов, переполнение, размер ≤8 MiB, VirtualQueryEx, MEM_COMMIT, доступность каждой страницы и guard. Проверки не исключают изменение страницы между query и read: результат RPM всё равно проверяется.
- `MemoryLayout` / `GameStateDecoder`: JSON структуры, идентичность build, PE fingerprint, address chain и декодирование. Нет DirectX, ImGui или вызовов Demo. После декодирования применяется общий ValidateFrame.
- `LiveFrameSource`: один существующий worker RuntimeDataProvider. При reconnect повторно проверяет module fingerprint и разрешает корень с учётом ASLR. Layout перечитывается при изменении mtime. Снимки неизменяемые; только свежий sequence обновляет срок годности. Demo fallback запрещён для live.

Ошибки чтения и отсутствующий процесс дают DISCONNECTED/RECONNECTING с Win32 диагностикой, неверное содержимое присоединённого источника — INVALID DATA. Последний валидный кадр хранится не дольше stale_ms при потере источника; invalid кадр очищается сразу. Пустой валидный массив означает исчезновение всех объектов. Ошибка не подтверждает наличие античита и не вызывает попытку обхода.

## Layout schema 1

`memory.fixture.json` генерируется `fixture_layout_tool` по `sizeof/offsetof` опубликованного `tests/support/fixture_contract.hpp` и **реальному** PE-файлу EspMemoryFixture.exe. Это не пример DBD offsets. После пересборки exe JSON необходимо пересоздать; native CMake делает это автоматически. Для cross build запустите native fixture_layout_tool над полученным PE. Не переносите JSON от другого exe.

Обязательные поля: `coordinate_contract=row-major-column-vector-lh-y-up-dx01`, `verified_build`, `executable`, `module`, `image_size`, `pe_timestamp`, `machine=AMD64`, `max_entities`. `pid=0` либо отсутствует означает поиск по имени. Изменение PID в конфигурации требует Apply; автоматический restart работает при поиске по имени.

Корень: ровно один `root_export` (адрес экспортированной переменной) или `root_rva` (RVA от module base). `pointer_chain` — до восьми смещений: каждый шаг читает uint64 по `current + offset` и использует результат как следующий адрес. Адреса принимают беззнаковое JSON-число либо строку `0x...`. Fingerprint timestamp/size обнаруживает несовпадение сборки, но не является криптографической аутентификацией производителя.

`header`: `size`, `magic`/`magic_value` (uint64), `version`/`version_value` (uint32), `revision`, `sequence`, `timestamp_us` (uint64), `count` (uint32), `camera_position` (3 float32), `view_projection` (16 float32). Значения полей, кроме констант, — байтовые смещения. Память little-endian, стандартные IEEE754 float32.

`entities_offset`: смещение inline массива после header либо адреса uint64 в header, если `entities_pointer=true`. `entity`: `stride`, `id` uint64, `kind` uint32, `position`, `top`, `direction` (3 float32), `flags` uint32, `direction_mask`, `name` и `name_bytes` (UTF-8, обязательный NUL внутри fixed buffer ≤256), `kinds` из пяти уникальных чисел в порядке Survivor/Killer/Generator/Hook/ExitGate. Направление используется только при установленном флаге и должно быть нормализованным. Максимум 10000 объектов, stride ≤4096, общий массив ≤8 MiB. Снимок включает все объекты, не delta.

Контракт когерентности: writer увеличивает 64-битную выровненную revision до нечётного значения перед изменениями и до чётного после публикации с memory barrier. Decoder читает header, массив и повторную revision; нечётный/изменённый revision откладывает кадр. Sequence меняется с каждым свежим полным состоянием, timestamp_us задаётся производителем. Если источник не предоставляет revision/полный snapshot, нужен отдельный decoder для его подтверждённой модели согласованности; подстановка искусственных полей не выполняется.

Матрица: row-major storage, column vectors, left-handed, +Y up, clip z в [0,w]. Все позиции в одной системе единиц, согласованной с renderer (метры). Live не исправляет непроверенные соглашения автоматически. Viewport/aspect и FOV уже учитываются производителем в матрице; Demo settings не заменяют real camera.

## Отдельный процесс-стенд и проверки

EspMemoryFixture.exe сам выделяет свой State и два probe pages (вторая PAGE_NOACCESS), экспортирует указатели EspFixtureState / EspFixtureProbe. Пять категорий находятся в собственной памяти процесса. Стенд обновляет sequence, timestamp и направление killer. Тестовый контроллер через stdin меняет память **внутри стенда**, reader получает результат только через Windows read API.

`windows_memory_pipeline`: реальный child PID, module base/export/pointer traversal, декодирование, перемещение, поворот камеры, исчезновение ID, NaN matrix, invalid count, pause/stale/resume, выход и новый процесс/reconnect. Проверяет частичное чтение через границу страниц (8 из 16 байт), переполнение адреса, отсутствие Demo и отсутствие утверждения о DBD.

`windows_memory_overlay_pipeline`: те же проверки, дополнительно существующие DirectComposition Overlay + DX11 + RenderEntities, пять нарисованных категорий и пиксели survivor box после чтения/перемещения/поворота. BMP — backbuffer приложения перед Present; это не снимок всего desktop и не автоматическая проверка прохождения клика в чужое окно. Ручная проверка click-through и пользовательских hotkeys остаётся в README.

Portable memory_layout_and_decoder проверяет layout bounds, известный memory image через IMemoryReader, нечётный/изменённый revision, ID/type/NaN/normalization/name, пустой массив и невозможность Demo fallback. Этот тест не заменяет Windows cross-process verification.

## Что ещё нужно для DBD 10.2.2

Подтверждённый точный exe/module/build и его реальные адресные цепочки/структуры; реальная модель согласованного обхода объектов и жизненного цикла; подтверждённые ID/type/name/position/top/optional normalized direction, камера/VP и соглашения координат/единиц/viewport. Generic descriptor подходит только производителю, действительно реализующему описанный контракт. Если layout игры иной, потребуется реализация отдельного decoder на основании проверенных данных, а не переименование стенда. Наличие reader не делает эти поля известными. Доступ стандартного OpenProcess может завершиться отказом; программа выводит ошибку и не обходит защиту. DBD в этой среде не запущен, реальная игровая сессия не проверена.
