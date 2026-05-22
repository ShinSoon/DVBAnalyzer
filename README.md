# DVBAnalyzer
# DVB SI Parser Simulator

## Description / Purpose

This is a command-line C++ application designed to simulate the parsing and handling of basic DVB Service Information (SI) data, specifically focusing on Service Description Tables (SDT) and Event Information Tables (EIT). It serves as a learning tool and a potential test harness for developing or testing higher-level TV middleware components like Electronic Program Guides (EPGs) or channel management logic.

The primary goal is to demonstrate understanding of DVB SI data structures and provide a practical example of C++ parsing, data management using STL containers, time handling, and basic command-line application structure, relevant to the TV software/middleware domain. It processes a simplified text-based representation of SI data, mimicking how real middleware would handle data extracted from a DVB transport stream (like one delivered via DVB-T2).

## Features Implemented

*   **Data Structures:** Defines `struct TransportStreamId`, `struct ServiceInfo`, and `struct EventInfo` to model key DVB SI entities.
    *   `ServiceInfo` stores Service ID, names, provider, type, running status, and flags derived from SDT.
    *   `EventInfo` stores Event ID, name, description, start time, and duration derived from EIT.
*   **Parsing Logic:** Implements a `DvbSiParser` class responsible for reading simulated SI data.
    *   Parses a custom pipe-delimited (|) text format with record type prefixes (SDT, EIT) from an input file.
    *   Handles Service Description Table records (`SDT|ONID|TSID|ServiceID|EIT_Schedule_flag|EIT_PF_flag|RunStatus|FreeCA|ServiceType|ProviderName|ServiceName`).
    *   Handles Event Information Table records (`EIT|ONID|TSID|ServiceID|EventID|StartTime|DurationSec|EventName|EventDesc|Type(PF/SCHED)`).
    *   Stores parsed services in a `std::map<TransportStreamId, std::vector<ServiceInfo>>`.
    *   Stores parsed events (separated by P/F and Schedule) in nested maps: `std::map<TransportStreamId, std::map<uint16_t, std::vector<EventInfo>>>`.
    *   Includes warning messages for malformed or incomplete records and handles basic exceptions during number conversion.
*   **File Input:** Reads simulated data from a text file specified via a command-line argument.
*   **SI/EPG Queries:** Implements essential query functionalities:
    *   `getServices(tsid)`: Returns all services for a given Transport Stream ID / Original Network ID.
    *   `getServiceInfo(tsid, serviceId, outInfo)`: Retrieves details for a specific service.
    *   `getPresentFollowingEvents(tsid, serviceId)`: Returns EIT Present/Following events for a service.
    *   `getScheduledEvents(tsid, serviceId)`: Returns EIT Schedule events for a service (the main EPG data).
    *   `getEventsForTimeRange(tsid, serviceId, startMillis, endMillis)`: Finds all events (P/F and Schedule) for a service overlapping a given time range.
*   **Time Handling:**
    *   Uses `<chrono>` for time representations.
    *   Stores event start times as epoch milliseconds (`long long`) for easier calculations.
    *   Includes helper functions (`parseDateTimeString`, `formatTime`) to convert between string representations ("YYYY-MM-DD HH:MM:SS") and epoch milliseconds.
    *   Uses thread-safe `localtime_s` (Windows) or `localtime_r` (POSIX) for converting epoch time back to human-readable format for display.
*   **Build System:** Portable **CMake** build (app + CTest-driven GoogleTest suite) plus a Visual Studio solution. Targets C++17.
*   **Command-Line Arguments & Interactive Shell:** Accepts the input data file (text or `*.ts`), plus `--gen-ts` and `--demo` modes. After parsing it enters a small REPL (`streams`, `services`, `epg`, `pf`, `range`, …) so the data can be explored live; the prompt is TTY-aware so commands can also be piped in.
*   **C++ Best Practices:** Uses header guards, separates declaration (`.h` in `include/`) from implementation (`.cpp` in `src/`), utilizes STL containers effectively, implements basic exception handling, and demonstrates class design (`DvbSiParser`).

## Binary Transport Stream Parsing (DVB SI over MPEG-2 TS)

In addition to the simplified text format, the analyzer now ingests **real binary
DVB Service Information carried inside an MPEG-2 Transport Stream** — i.e. the
actual on-air data path that TV middleware deals with, not a stand-in text file.
Both ingest paths populate the **same** internal stores and feed the **same**
query API, and they now produce byte-identical results (all times in UTC).

### Pipeline

```
.ts file ──▶ TsDemux ──▶ SectionAssembler ──▶ CRC-32 check ──▶ SDT / EIT section parser
   (188-byte packets,     (reassemble            (MPEG-2)        (+ descriptor loops,
    PID 0x11 / 0x12,       sections across                         MJD+BCD time decode)
    PUSI, pointer_field)   TS packets)                                     │
                                                                          ▼
                                       fills services_by_ts_ / events_*_by_service_ ──▶ queries
```

### What the binary path implements

*   **TS packet layer:** 188-byte packets, sync byte `0x47`, 13-bit PID,
    `payload_unit_start_indicator`, adaptation-field skip, `continuity_counter`.
*   **Section reassembly:** PSI/SI sections rebuilt across multiple TS packets,
    honouring the `pointer_field` after PUSI and `0xFF` inter-section stuffing.
*   **CRC-32:** every section is validated with the MPEG-2/DVB CRC
    (poly `0x04C11DB7`); sections that fail are dropped.
*   **SDT** (`table_id` 0x42/0x46 on PID 0x11): service loop with
    `EIT_schedule_flag`, `EIT_present_following_flag`, `running_status`,
    `free_CA_mode`, plus the **`service_descriptor` (0x48)** for service type,
    provider name and service name.
*   **EIT** (`0x4E`/`0x4F` present/following, `0x50`–`0x6F` schedule on PID 0x12):
    event loop with **MJD + BCD UTC** `start_time` and BCD `duration` decoding
    (ETSI EN 300 468 Annex C), plus the **`short_event_descriptor` (0x4D)** for
    event name and description.
*   **TS generator (`--gen-ts`):** a bundled encoder that builds a valid stream
    (real section framing, CRC-32, MJD/BCD times and descriptor loops) mirroring
    `simulated_dvb_si.txt`, so the parser can be exercised against genuine binary
    data with no external tooling.

### New source files

| File | Responsibility |
|------|----------------|
| `dvb_crc32.{h,cpp}`        | MPEG-2/DVB section CRC-32 |
| `bit_reader.h`            | Big-endian sub-byte field reader |
| `dvb_time.h`              | UTC-correct epoch helpers + MJD/BCD codec |
| `ts_demux.{h,cpp}`        | TS packet parsing + section reassembly |
| `dvb_descriptors.{h,cpp}` | `service_descriptor` / `short_event_descriptor` |
| `dvb_section_parser.{h,cpp}` | SDT/EIT section decoding (with CRC validation) |
| `ts_generator.{h,cpp}`    | Sample binary transport-stream encoder |

> Note: the previous text path called `mktime`/`localtime`, which silently
> shifted every timestamp by the host timezone. Time handling is now centralised
> in `dvb_time.h` and is pure UTC, so the text and binary paths agree.

## Technologies Used

*   **Language:** C++17
*   **Core Libraries:** C++ Standard Library (STL - `<vector>`, `<map>`, `<string>`, `<sstream>`, `<fstream>`, `<iostream>`, `<stdexcept>`, `<chrono>`, `<ctime>`, `<iomanip>`)
*   **Build System:** CMake (cross-platform) or the Visual Studio solution; also compiles directly with g++/Clang/MSVC.
*   **Compiler:** g++, Clang, or MSVC.

## Setup / Prerequisites

*   A C++17 compliant compiler (g++, Clang, MSVC).
*   CMake ≥ 3.14 (recommended build path).
*   Git (for cloning).

## How to Build

**Option 1: CMake (recommended, cross-platform)**

Builds the `DVBAnalyzer` app and the `DVBAnalyzerTests` suite on Windows (MSVC),
Linux (g++/clang) and macOS from one configuration step:

```bash
cmake -S . -B build                            # configure
cmake --build build --config Debug             # build app + tests
ctest --test-dir build -C Debug --output-on-failure   # run the 28 unit tests
```

The app lands at `build/DVBAnalyzer` (single-config generators) or
`build/Debug/DVBAnalyzer.exe` (the multi-config Visual Studio generator). Pass
`-DBUILD_TESTING=OFF` to build only the app and skip GoogleTest.

**Option 2: Visual Studio solution**

Open `DVBAnalyzer.sln` and build (Debug|x64). It contains two projects:
`DVBAnalyzer` (the app) and `DVBAnalyzerTests` (the GoogleTest suite). The
debugger is preconfigured to parse the bundled `simulated_dvb.ts`.

**Option 3: Single-command compile (no build system)**

```bash
g++ -std=c++17 -Wall -Wextra -o dvb_parser_sim dvb_*.cpp ts_*.cpp main.cpp
```
(Use `clang++`, or `cl /EHsc /std:c++17 dvb_*.cpp ts_*.cpp main.cpp` with MSVC.)

## How to Run / Use

1.  **Prepare Data File:** Ensure the `simulated_dvb_si.txt` file (or your own version) exists in the project's root directory (or provide the correct path when running). The file should contain data lines using the specified format:
    *   `SDT|ONID|TSID|ServiceID|EIT_Schedule_flag|EIT_PF_flag|RunStatus|FreeCA|ServiceType|ProviderName|ServiceName`
    *   `EIT|ONID|TSID|ServiceID|EventID|StartTime|DurationSec|EventName|EventDesc|Type(PF/SCHED)`
    *   **Time Format:** The parser expects `StartTime` in `"YYYY-MM-DD HH:MM:SS"` format, interpreted as UTC (via `makeUtcEpochMillis` in `dvb_time.h`).

2.  **Execute:** the executable selects its mode from the arguments:
    *   **Text format:** `DVBAnalyzer simulated_dvb_si.txt`
    *   **Binary transport stream:** `DVBAnalyzer some_stream.ts` (any `*.ts` path)
    *   **Generate a sample stream:** `DVBAnalyzer --gen-ts simulated_dvb.ts`
        then parse it with `DVBAnalyzer simulated_dvb.ts`.
    *   **Full non-interactive dump:** append `--demo`, e.g. `DVBAnalyzer simulated_dvb.ts --demo`.

3.  **Explore interactively:** after parsing, the program prints the transport
    streams found and drops into a small command shell:

    ```text
    dvb> streams                       list transport streams (ONID/TSID)
    dvb> summary                       streams and their services
    dvb> services <onid> <tsid>        services in a transport stream
    dvb> service  <onid> <tsid> <sid>  one service's details
    dvb> epg      <onid> <tsid> <sid>  scheduled events (EPG) for a service
    dvb> pf       <onid> <tsid> <sid>  present/following events
    dvb> range    <onid> <tsid> <sid> <start> <end>   events in a time window
                                       (times as YYYY-MM-DDTHH:MM:SS, UTC)
    dvb> help                          show commands
    dvb> quit                          exit
    ```

    Example: `epg 10 1001 101`. Commands can also be piped in
    (`echo summary | DVBAnalyzer simulated_dvb.ts`); the prompt is suppressed when
    input is not a terminal.

## Testing

Unit and integration tests use **GoogleTest**, vendored under
`third_party/googletest` (committed, so the suite builds offline with no package
manager). They live in a second project, `DVBAnalyzerTests`, in the same solution.

**28 tests across 6 suites** focus on the parts where bugs actually hide:

| Suite | What it pins down |
|-------|-------------------|
| `Crc32`        | known-answer check value `0x0376E6E7`, zero-remainder property, single-bit error detection |
| `DvbTime`      | UTC epoch anchors (1970, Y2K), leap day, MJD `0x9E8B` encoding, MJD/BCD and duration round-trips |
| `DvbText`      | DVB character-table control-byte stripping |
| `Descriptors`  | `service_descriptor` / `short_event_descriptor` field extraction; unknown tags skipped by length |
| `SdtSection`   | valid section decode; **rejection** of bad CRC, wrong `table_id`, and too-short input |
| `EndToEnd`     | generate a real `.ts` → demux → CRC → parse → query, asserting services, P/F vs schedule split, decoded times and the time-range overlap |

**Run from Visual Studio:** open the solution, set *DVBAnalyzerTests* as startup
project and press F5 (or use Test Explorer with the *Test Adapter for Google
Test*).

**Run from the command line** (Developer Prompt, from the project folder):

```bat
cl /nologo /EHsc /std:c++17 /D_CRT_SECURE_NO_WARNINGS ^
   /I. /Ithird_party\googletest\googletest\include /Ithird_party\googletest\googletest ^
   dvb_crc32.cpp dvb_descriptors.cpp dvb_eit.cpp dvb_sdt.cpp dvb_section_parser.cpp ^
   dvb_si_parser.cpp ts_demux.cpp ts_generator.cpp tests\*.cpp ^
   third_party\googletest\googletest\src\gtest-all.cc ^
   third_party\googletest\googletest\src\gtest_main.cc /Fe:run_tests.exe
run_tests.exe
```

## Known Limitations / Future Improvements

*   **Input Format:** Two paths are supported — a simplified custom text format and **real binary DVB SI sections inside MPEG-2 Transport Stream packets** (PID demux, section reassembly, CRC-32, descriptor loops, MJD/BCD time). The binary path covers the SI layer; it does not yet read raw `.ts` *recordings* with PCR/PTS or scrambled payloads.
*   **Physical Layer:** Does not simulate any DVB-T/T2/S/S2/C physical layer reception or demodulation. It assumes the transport stream (SI sections) has already been received/extracted.
*   **SI Table Completeness:** Implements SDT and EIT. A full implementation would still need NIT, PAT, PMT, TDT, TOT, CAT, etc., for complete receiver functionality.
*   **Descriptor Parsing:** Parses the `service_descriptor` (0x48) and `short_event_descriptor` (0x4D). Real SI carries many more descriptors (extended event, parental rating, content, component, …) and full DVB character-set tables, which are not yet modelled.
*   **Parsing Robustness:** Basic error handling. Assumes largely correct input format. Doesn't handle character encoding complexities explicitly (relies on system defaults).
*   **Time Zone Handling:** All times are handled as UTC (matching DVB), centralised in `dvb_time.h` (`makeUtcEpochMillis`/`formatUtcTime`). Leap seconds are not modelled.
*   **Testing:** A **GoogleTest** suite (28 tests, see *Testing* below) covers CRC-32, MJD/BCD time, descriptors, the section parser and the full pipeline. Broader fuzz/property testing of malformed streams would be a useful next step.
*   **Concurrency:** All operations are single-threaded. Real middleware often needs to handle SI updates asynchronously.

---

# DVB SI 解析器模拟器 (中文说明)

## 描述 / 目的

这是一个命令行 C++ 应用程序，旨在模拟解析和处理基本的 DVB 服务信息 (SI) 数据，特别关注服务描述表 (SDT) 和事件信息表 (EIT)。它可作为一个学习工具，以及一个潜在的测试工具，用于开发或测试更高级别的电视中间件组件（如电子节目指南 EPG）或频道管理逻辑。

主要目标是展示对 DVB SI 数据结构的理解，并提供一个关于 C++ 解析、使用 STL 容器进行数据管理、时间处理以及基本命令行应用程序结构的实践示例，这些都与电视软件/中间件领域相关。它处理 SI 数据的简化文本表示，模仿真实中间件处理从 DVB 传输流（例如通过 DVB-T2 传输的流）中提取的数据的方式。

## 已实现功能

*   **数据结构:** 定义了 `struct TransportStreamId`、`struct ServiceInfo` 和 `struct EventInfo` 来模拟关键的 DVB SI 实体。
    *   `ServiceInfo` 存储从 SDT 派生的服务 ID、名称、提供商、类型、运行状态和标志。
    *   `EventInfo` 存储从 EIT 派生的事件 ID、名称、描述、开始时间和持续时间。
*   **解析逻辑:** 实现了一个 `DvbSiParser` 类，负责读取模拟的 SI 数据。
    *   解析来自输入文件的自定义管道符分隔 (|) 文本格式，该格式带有记录类型前缀 (SDT, EIT)。
    *   处理服务描述表记录 (`SDT|ONID|TSID|ServiceID|EIT_Schedule_flag|EIT_PF_flag|RunStatus|FreeCA|Type|ProviderName|ServiceName`)。
    *   处理事件信息表记录 (`EIT|ONID|TSID|ServiceID|EventID|StartTime|DurationSec|EventName|EventDesc|Type(PF/SCHED)`)。
    *   将解析的服务存储在 `std::map<TransportStreamId, std::vector<ServiceInfo>>` 中。
    *   将解析的事件（按 P/F 和 Schedule 分开）存储在嵌套的 map 中：`std::map<TransportStreamId, std::map<uint16_t, std::vector<EventInfo>>>`。
    *   包含针对格式错误或不完整记录的警告消息，并处理数字转换期间的基本异常。
*   **文件输入:** 从命令行参数指定的文本文件中读取模拟数据。
*   **SI/EPG 查询:** 实现了必要的查询功能：
    *   `getServices(tsid)`: 返回给定传输流 ID / 原始网络 ID 的所有服务。
    *   `getServiceInfo(tsid, serviceId, outInfo)`: 检索特定服务的详细信息。
    *   `getPresentFollowingEvents(tsid, serviceId)`: 返回服务的 EIT 当前/后续 (Present/Following) 事件。
    *   `getScheduledEvents(tsid, serviceId)`: 返回服务的 EIT 计划 (Schedule) 事件（主要的 EPG 数据）。
    *   `getEventsForTimeRange(tsid, serviceId, startMillis, endMillis)`: 查找在给定时间范围内与服务重叠的所有事件（P/F 和 Schedule）。
*   **时间处理:**
    *   使用 `<chrono>` 进行时间表示。
    *   将事件开始时间存储为 epoch 毫秒 (`long long`) 以便于计算。
    *   包含辅助函数 (`parseDateTimeString`, `formatTime`) 以在字符串表示 ("YYYY-MM-DD HH:MM:SS") 和 epoch 毫秒之间进行转换。
    *   使用线程安全的 `localtime_s` (Windows) 或 `localtime_r` (POSIX) 将 epoch 时间转换回人类可读的格式进行显示。
*   **构建系统:** 可移植的 **CMake** 构建（应用 + 由 CTest 驱动的 GoogleTest 套件），另附 Visual Studio 解决方案。目标 C++17。
*   **命令行参数与交互式命令行:** 接受输入数据文件（文本或 `*.ts`），并支持 `--gen-ts` 与 `--demo` 模式。解析完成后进入一个小型 REPL（`streams`、`services`、`epg`、`pf`、`range` 等）以便实时浏览数据；提示符可感知终端，因此命令也可通过管道输入。
*   **C++ 最佳实践:** 使用头文件保护符，将声明 (`.h` 在 `include/` 中) 与实现 (`.cpp` 在 `src/` 中) 分离，有效利用 STL 容器，实现基本的异常处理，并演示了类设计 (`DvbSiParser`)。

## 二进制传输流解析 (MPEG-2 TS 上的 DVB SI)

除了简化的文本格式外，本工具现在还能解析**承载于 MPEG-2 传输流 (TS) 中的真实二进制 DVB SI 数据**——即电视中间件实际处理的空中数据路径，而非文本替身。两条解析路径填充**同一套**内部存储、共用**同一组**查询接口，且结果完全一致（时间统一为 UTC）。

**处理流水线：** `.ts 文件` → `TsDemux`（188 字节 TS 包、PID 0x11/0x12、PUSI、pointer_field）→ `SectionAssembler`（跨 TS 包重组 section）→ `CRC-32 校验`（MPEG-2，多项式 0x04C11DB7）→ `SDT/EIT section 解析`（描述符循环 + MJD/BCD 时间解码）→ 填充存储 → 查询。

**已实现：**
*   **TS 包层：** 同步字节 `0x47`、13 位 PID、`payload_unit_start_indicator`、适配字段跳过、`continuity_counter`。
*   **Section 重组：** 跨多个 TS 包重建 PSI/SI section，处理 `pointer_field` 与 `0xFF` 填充。
*   **CRC-32：** 校验每个 section，丢弃校验失败者。
*   **SDT** (`table_id` 0x42/0x46，PID 0x11)：服务循环 + `service_descriptor` (0x48)。
*   **EIT** (`0x4E`/`0x4F` P/F，`0x50`–`0x6F` schedule，PID 0x12)：事件循环 + **MJD+BCD UTC** 时间解码 (ETSI EN 300 468 附录 C) + `short_event_descriptor` (0x4D)。
*   **TS 生成器 (`--gen-ts`)：** 内置编码器，生成带真实 CRC-32、MJD/BCD 时间与描述符循环的合法码流，用于测试，无需外部工具。

> 注：原文本路径使用 `mktime`/`localtime`，会按主机时区悄悄偏移所有时间戳。现在时间处理统一集中在 `dvb_time.h` 中并采用纯 UTC，因此文本与二进制两条路径结果一致。

## 使用的技术

*   **语言:** C++17
*   **核心库:** C++ 标准库 (STL - `<vector>`, `<map>`, `<string>`, `<sstream>`, `<fstream>`, `<iostream>`, `<stdexcept>`, `<chrono>`, `<ctime>`, `<iomanip>`)
*   **构建系统:** 使用 CMake（跨平台）或 Visual Studio 解决方案；也可直接用 g++/Clang/MSVC 编译。
*   **编译器:** g++, Clang, 或 MSVC。

## 设置 / 先决条件

*   符合 C++17 标准的编译器 (g++, Clang, MSVC)。
*   CMake ≥ 3.14（推荐的构建方式）。
*   Git (用于克隆)。

## 如何构建

**选项 1: CMake (推荐，跨平台)**

一次配置即可在 Windows (MSVC)、Linux (g++/clang) 与 macOS 上构建 `DVBAnalyzer` 应用与 `DVBAnalyzerTests` 测试套件：

```bash
cmake -S . -B build                            # 配置
cmake --build build --config Debug             # 构建 应用 + 测试
ctest --test-dir build -C Debug --output-on-failure   # 运行 28 个单元测试
```

应用生成于 `build/DVBAnalyzer`（单配置生成器）或 `build/Debug/DVBAnalyzer.exe`（Visual Studio 多配置生成器）。传入 `-DBUILD_TESTING=OFF` 可仅构建应用并跳过 GoogleTest。

**选项 2: Visual Studio 解决方案**

打开 `DVBAnalyzer.sln` 并构建 (Debug|x64)。其中包含两个项目：`DVBAnalyzer`（应用）与 `DVBAnalyzerTests`（GoogleTest 测试套件）。调试器已预配置为解析自带的 `simulated_dvb.ts`。

**选项 3: 单命令编译 (无需构建系统)**

```bash
g++ -std=c++17 -Wall -Wextra -o dvb_parser_sim dvb_*.cpp ts_*.cpp main.cpp
```
（也可使用 `clang++`，或在 MSVC 下使用 `cl /EHsc /std:c++17 dvb_*.cpp ts_*.cpp main.cpp`。）

## 如何运行 / 使用

1.  **准备数据文件:** 确保 `simulated_dvb_si.txt` 文件（或您自己的版本）存在于项目的根目录中（或在运行时提供正确的路径）。该文件应包含使用指定格式的数据行：
    *   `SDT|ONID|TSID|ServiceID|EIT_Schedule_flag|EIT_PF_flag|RunStatus|FreeCA|ServiceType|ProviderName|ServiceName`
    *   `EIT|ONID|TSID|ServiceID|EventID|StartTime|DurationSec|EventName|EventDesc|Type(PF/SCHED)`
    *   **时间格式:** 解析器期望 `StartTime` 为 `"YYYY-MM-DD HH:MM:SS"` 格式，按 UTC 解释（统一通过 `dvb_time.h` 中的 `makeUtcEpochMillis` 处理）。

2.  **执行:** 可执行文件根据参数选择模式：
    *   **文本格式:** `DVBAnalyzer simulated_dvb_si.txt`
    *   **二进制传输流:** `DVBAnalyzer some_stream.ts`（任意 `*.ts` 路径）
    *   **生成示例码流:** `DVBAnalyzer --gen-ts simulated_dvb.ts`，随后用 `DVBAnalyzer simulated_dvb.ts` 解析。
    *   如果数据文件名不同，请替换为您的数据文件的实际路径。

3.  **交互式浏览:** 解析完成后，程序会打印发现的传输流，并进入一个小型命令行：

    ```text
    dvb> streams                       列出传输流 (ONID/TSID)
    dvb> summary                       传输流及其服务
    dvb> services <onid> <tsid>        某传输流中的服务
    dvb> service  <onid> <tsid> <sid>  单个服务的详情
    dvb> epg      <onid> <tsid> <sid>  某服务的预定事件 (EPG)
    dvb> pf       <onid> <tsid> <sid>  当前/后续事件
    dvb> range    <onid> <tsid> <sid> <start> <end>   时间窗口内的事件
                                       （时间格式 YYYY-MM-DDTHH:MM:SS，UTC）
    dvb> help                          显示命令
    dvb> quit                          退出
    ```

    例如：`epg 10 1001 101`。命令也可通过管道输入（`echo summary | DVBAnalyzer simulated_dvb.ts`）；当输入非终端时提示符会被抑制。追加 `--demo` 可进行非交互式完整转储。

## 测试

单元测试与集成测试使用 **GoogleTest**，以源码形式置于 `third_party/googletest`（随项目提交，因此无需任何包管理器即可离线构建）。测试位于同一解决方案中的第二个项目 `DVBAnalyzerTests`。

**共 28 个测试、6 个测试套件**，聚焦于真正容易出错的部分：

| 套件 | 验证内容 |
|------|----------|
| `Crc32`        | 已知校验值 `0x0376E6E7`、余数为零特性、单比特错误检测 |
| `DvbTime`      | UTC 纪元锚点（1970、Y2K）、闰日、MJD `0x9E8B` 编码、MJD/BCD 与时长往返 |
| `DvbText`      | DVB 字符表控制字节剥离 |
| `Descriptors`  | `service_descriptor` / `short_event_descriptor` 字段提取；按长度跳过未知 tag |
| `SdtSection`   | 合法 section 解析；**拒绝** 错误 CRC、错误 `table_id` 及过短输入 |
| `EndToEnd`     | 生成真实 `.ts` → 解复用 → CRC → 解析 → 查询，断言服务、P/F 与 schedule 分流、解码时间及时间范围重叠 |

**在 Visual Studio 中运行：** 打开解决方案，将 *DVBAnalyzerTests* 设为启动项目并按 F5（或通过 *Test Adapter for Google Test* 使用测试资源管理器）。命令行构建方式见英文 *Testing* 一节。

## 已知限制 / 未来改进

*   **输入格式:** 支持两种路径——简化的自定义文本格式，以及 **MPEG-2 传输流数据包内的真实二进制 DVB SI section**（PID 解复用、section 重组、CRC-32、描述符循环、MJD/BCD 时间）。二进制路径覆盖了 SI 层；尚不解析含 PCR/PTS 或加扰负载的原始 `.ts` 录像。
*   **物理层:** 不模拟任何 DVB-T/T2/S/S2/C 物理层的接收或解调。它假定传输流（SI section）已被接收/提取。
*   **SI 表完整性:** 已实现 SDT 和 EIT。完整的实现仍需 NIT、PAT、PMT、TDT、TOT、CAT 等才能实现完整的接收器功能。
*   **描述符解析:** 解析 `service_descriptor` (0x48) 与 `short_event_descriptor` (0x4D)。真实 SI 还包含许多其他描述符（扩展事件、家长分级、内容、组件……）及完整的 DVB 字符集表，此处尚未建模。
*   **解析健壮性:** 基本的错误处理。假设输入格式基本正确。未明确处理字符编码复杂性（依赖于系统默认值）。
*   **时区处理:** 所有时间均按 UTC 处理（符合 DVB 规范），统一集中在 `dvb_time.h`（`makeUtcEpochMillis`/`formatUtcTime`）。未建模闰秒。
*   **测试:** 已包含 **GoogleTest** 测试套件（28 个测试，见下文 *测试* 一节），覆盖 CRC-32、MJD/BCD 时间、描述符、section 解析器及完整流水线。对畸形码流的模糊/属性测试是有价值的后续工作。
*   **并发性:** 所有操作都是单线程的。真实的中间件通常需要异步处理 SI 更新。
