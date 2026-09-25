# BOM Analyzer

A C++20 and Qt 6 application for BOM data: CSV import, editing, analytics and JSON export.

## Build and run

Tested with Windows x64, Visual Studio Community 2026, MSVC v145, Qt 6.8.3 (`msvc2022_64`) and CMake 4.3.1. You need the C++ development tools, Windows SDK and Qt Core/Widgets/Test.

Run the commands from the project root in PowerShell with `cmake` and `ctest` available. Set your own Qt path. This creates a local Debug build for a development machine.

```powershell
$taskQtPath = 'C:/Qt/6.8.3/msvc2022_64'

cmake -S . -B build/local-x64 -G "Visual Studio 18 2026" -A x64 -T "v145,host=x64" "-DCMAKE_PREFIX_PATH=$taskQtPath" -DBUILD_TESTING=ON
cmake --build build/local-x64 --config Debug --parallel 1
ctest --test-dir build/local-x64 -C Debug --output-on-failure

& "$taskQtPath/bin/windeployqt.exe" --debug --no-translations --no-system-d3d-compiler --no-opengl-sw --no-compiler-runtime build/local-x64/bin/Debug/BomAnalyzer.exe
& './build/local-x64/bin/Debug/BomAnalyzer.exe'
```

Open the [sample CSV](tests/data/bom_sample.csv) with **Open CSV** (`Ctrl+O`). The total cost should be **1560.00**. Double-click a cell, press `F2` or use the context menu to edit it. `Enter` confirms the change, `Escape` cancels it. **Save report** (`Ctrl+S`) exports the current analytics.

For a Release build, replace `Debug` with `Release` and `--debug` with `--release` in these commands.

## Data and calculations

Required CSV header:

```csv
BuildingID,ProfileName,Length_mm,Cost,Quantity
```

`Cost`: the price of one whole piece. `Length_mm`: its length. No currency is specified.

| Value | Formula |
| --- | --- |
| Item cost | `Cost * Quantity` |
| Total item length, mm | `Length_mm * Quantity` |
| Cost per metre | `Cost / Length_mm * 1000` |
| Average cost per metre for a profile | `sum(Cost * Quantity) / sum(Length_mm * Quantity) * 1000` |

The average is weighted by length. Profiles are grouped by exact name, including case and spaces. The top three list ranks individual items. Items with equal costs keep their document order.

Length must be positive, cost must be non-negative, and quantity must be a positive `int`. Empty names, non-finite numbers and overflow are rejected.

JSON contains the overall cost, building costs and profile summaries: average cost per metre, length in millimetres and cost. Buildings are sorted by ID. Exported numbers are not rounded. The UI shows two decimal places, with the precise value in a tooltip.

## Architecture and decisions

The code is split into data and validation (`domain`), calculations (`core`), CSV/JSON (`io`), the tree model (`models`), formatting (`presentation`) and widgets (`ui`). Core and I/O depend only on Qt Core. I kept `QString` and `QHash` in these layers to avoid extra conversions in a Qt application.

- **Full recalculation.** After loading or a confirmed edit, the report is calculated in one pass, with expected O(N) time. No state is kept between calls, so I used free functions. Partial updates would need intermediate totals and dependency tracking, which I considered too much for this project.
- **Stored metrics.** Cells read values from the report without repeating calculations.
- **Links through IDs.** Lookup by `BuildingID` and `ItemId` does not depend on item order. Item IDs are unique within a document and stay the same after edits. A new import creates its own IDs.
- **Declarative fields.** Validation, the model, editors and the context menu share numeric field descriptions. Decimal values are edited as text to keep `double` precision.
- **Errors and saving.** Calculations and I/O return `Result<T>`, and the UI displays errors. The document and report are replaced only after validation and calculation succeed. JSON is written atomically with `QSaveFile`.

## CSV

I kept own parser for this fixed format: fewer dependencies, but parsing and its tests need maintenance. A library would be worth considering for more formats or streaming input.

The parser supports UTF-8, comma separators, quoted fields with commas and line breaks, and escaped quotes (`""`). CRLF and CR are converted to LF. The header must match the one above exactly. Numbers use a decimal point regardless of locale; length and cost also accept exponent notation. Blank records are rejected, but a file with only the header is valid.

Rows with the same `BuildingID` form one building. Buildings keep their first-seen order, and rows keep their order within each building. Duplicate items are kept.

## Tests

All six Qt Test suites passed in Win64 Release: calculations, CSV, JSON, the model and window, profile analysis and the DLL call. UI tests use the offscreen plugin. The Delphi demo was checked with both Debug and Release DLLs.

## Bonus task

**Analyze profile** calls `ProfileAnalyzer.dll` for the selected item and shows its type, nominal height and estimated weight per metre. This result is not included in JSON. The analyzer in `src/profile_analyzer` does not depend on Qt.

### Weight estimate

The name contains a type in Latin letters, a height and an optional thickness (`IPE200`, `HEA160`, `L150*5`). Both letter cases, spaces between parts and separators `*`, `x`, `X`, `×` are accepted. Dimensions must be positive numbers with a decimal point, without exponent notation.

All types use one formula: `weight_kg_per_m = height_mm * thickness_mm * factor * 0.00785`.

| Type | Factor | Default thickness, mm |
| --- | --- | --- |
| IPE | 2 | 7 |
| HEA | 3 | 8 |
| L | 2 | 5 |

The second number replaces the table thickness. The multiplier `0.00785` comes from a steel density of 7850 kg/m³ with area in mm². The factors and thicknesses were chosen for demonstration, so the result is not a catalog weight. A new type only needs another table entry.

### C++ and Delphi boundary

The assignment describes migration from Delphi, so I first wrote the calculation in C++ and then added a C ABI wrapper. This keeps buffer and calling rules outside the calculation code.

The function signature from the assignment is preserved:

```cpp
extern "C" int __cdecl AnalyzeProfile(const wchar_t* profileName,
    double* nominalHeight, double* weightPerMeter, wchar_t* profileType) noexcept;
```

- **Call.** The `.def` file sets the exact export name `AnalyzeProfile`, with `cdecl` as the calling convention. The ABI passes pointers and numbers, without Qt/STL/Delphi objects.
- **Buffer.** The input string is null-terminated. For the returned type, the caller allocates 32 `wchar_t`/`WideChar` elements: room for 31 characters and a terminator. This is an agreed capacity, since the signature does not pass a buffer size and string length does not tell us that size. A new API could accept the capacity as a separate argument.
- **Errors.** Codes: `0` (success), `1` (null pointer), `2` (invalid format or dimensions), `3` (unsupported type), `4` (internal error). Outputs are cleared on failure, and exceptions are caught inside the DLL.
- **Declarations in two languages.** The signature, codes and buffer capacity are repeated in C++ and Delphi. For one interface, I kept them in sync by hand. If it grows, declarations could be generated from one shared ABI description.

[ProfileAnalyzerWrapper.pas](delphi/ProfileAnalyzerWrapper.pas) keeps the assignment's `Boolean` / `out TProfileInfo` interface and adds an overload with an error message. It loads the DLL beside the EXE, finds the function through `GetProcAddress` and releases the library in `finally`.

### Delphi demo

The compiled [ProfileAnalyzer.dll](delphi/ProfileAnalyzer.dll) is included beside the Delphi sources (Windows x64, Release). It needs the Microsoft Visual C++ x64 runtime, but no Qt libraries. Copy it beside the demo EXE if the IDE puts the EXE in another folder.

Open [ProfileAnalyzerDemo.dproj](delphi/ProfileAnalyzerDemo.dproj), select **Windows 64-bit / Debug** and use **Project → Build ProfileAnalyzerDemo**. I used Delphi 13 Community Edition.

The EXE and DLL must be in the same folder and have matching bitness. The tested build is x64; a 32-bit process needs a separate x86 DLL. The Debug DLL needs the MSVC Debug runtime, while the Release DLL uses the standard runtime.

If the IDE placed the EXE in `delphi`, copy it beside the DLL. Run these commands from the project root:

```powershell
Copy-Item -LiteralPath 'delphi/ProfileAnalyzerDemo.exe' -Destination 'build/local-x64/bin/Debug/ProfileAnalyzerDemo.exe'

& './build/local-x64/bin/Debug/ProfileAnalyzerDemo.exe'
& './build/local-x64/bin/Debug/ProfileAnalyzerDemo.exe' 'IPE200*2.5' 'L150*5'
& './build/local-x64/bin/Debug/ProfileAnalyzerDemo.exe' 'XYZ200'
$LASTEXITCODE
```

Without arguments: `IPE200` → **21.98**, `HEA160` → **30.144**, `L150*5` → **11.775 kg/m**. For `IPE200*2.5`, expect **7.85 kg/m**. For `XYZ200`, expect an error and exit code `1`. A successful run returns `0`. The demo calls the Delphi wrapper and exits after printing the results. Profile names are passed as command-line arguments.

## Development notes

I mainly focused on making the code easier to change later. I came back to working code few times: I wanted the connections between parts to be clear without keeping all of them in my head.

The most interesting parts for me were declarative fields and Delphi integration. With fields, I had to think about data, the model and the UI together. With Delphi, even this small calculation was a useful way to learn more about type compatibility.
