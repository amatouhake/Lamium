# Third-party notices

## LeviLamina mod template

The build and packaging scaffolding is adapted from the
[LeviLamina mod template](https://github.com/LiteLDev/levilamina-mod-template),
licensed under CC0-1.0. Its dedication does not require Lamium's original code to
use CC0. Lamium is licensed under LGPL-3.0-only.

## LaminaView

Camera integration uses implementation lessons from
[LaminaView](https://github.com/amatouhake/LaminaView), an earlier mod by
Lamium's author, copyright 2026 amatouhake.

## LaminaPeek

Item-content readers, hover integration, and preview rendering are adapted from
[LaminaPeek](https://github.com/amatouhake/LaminaPeek), an earlier mod by
Lamium's author, copyright 2026 amatouhake.

## LaminaSort

Inventory planning, item classification, and container transfer integration are
adapted from [LaminaSort](https://github.com/amatouhake/LaminaSort), an earlier
mod by Lamium's author, copyright 2026 amatouhake.

## JSON for Modern C++

Settings serialization uses [nlohmann/json](https://github.com/nlohmann/json)
v3.12.0, MIT, copyright 2013-2025 Niels Lohmann. Its license is included in
[licenses/nlohmann-json-MIT.txt](licenses/nlohmann-json-MIT.txt).

## LeviLamina

[LeviLamina](https://github.com/LiteLDev/LeviLamina) is a separately installed
runtime and SDK licensed under LGPL-3.0. Its license remains applicable to that
dependency. Lamium does not bundle Minecraft or the LeviLamina runtime.
The SDK's [GPL text](licenses/LeviLamina-GPL-3.0.txt) and
[LGPL supplement](licenses/LeviLamina-LGPL-3.0.txt) are included together.
Lamium uses the separately replaceable runtime through its SDK; these notices
do not change the LGPL-3.0-only license of Lamium's original code.

## SDK dependencies

The following notices cover the libraries made available by the locked SDK
build, including header-only libraries. Inclusion here does not imply that all
of a library's implementation is present in every resulting DLL. License files
are retained verbatim from the indicated version's upstream source.

| Component | Version | Upstream source | Included notice |
| --- | --- | --- | --- |
| {fmt} | 11.2.0 | [fmtlib/fmt](https://github.com/fmtlib/fmt/tree/11.2.0) | [MIT with optional exception](licenses/fmt.txt) |
| EnTT | v4.0.0 | [skypjack/entt](https://github.com/skypjack/entt/tree/v4.0.0) | [MIT](licenses/entt.txt) |
| Microsoft GSL | v4.2.0 | [microsoft/GSL](https://github.com/microsoft/GSL/tree/v4.2.0) | [MIT](licenses/gsl.txt) |
| expected-lite | v0.8.0 | [nonstd-lite/expected-lite](https://github.com/nonstd-lite/expected-lite/tree/v0.8.0) | [Boost](licenses/expected-lite.txt) |
| GLM | 1.0.1 | [g-truc/glm](https://github.com/g-truc/glm/tree/1.0.1) | [Dual license; MIT option used](licenses/glm.txt) |
| LevelDB | 1.23 | [google/leveldb](https://github.com/google/leveldb/tree/1.23) | [BSD](licenses/leveldb.txt) |
| Snappy | 1.2.2 | [google/snappy](https://github.com/google/snappy/tree/1.2.2) | [BSD and upstream test-data notices](licenses/snappy.txt) |
| magic_enum | v0.9.7 | [Neargye/magic_enum](https://github.com/Neargye/magic_enum/tree/v0.9.7) | [MIT](licenses/magic-enum.txt) |
| RapidJSON | 2025.02.05 | [Tencent/rapidjson](https://github.com/Tencent/rapidjson) | [MIT and upstream third-party notices](licenses/rapidjson.txt) |
| type_safe | v0.2.4 | [foonathan/type_safe](https://github.com/foonathan/type_safe/tree/v0.2.4) | [MIT](licenses/type-safe.txt) |
| debug_assert | v1.3.4 | [foonathan/debug_assert](https://github.com/foonathan/debug_assert/tree/v1.3.4) | [zlib](licenses/debug-assert.txt) |
| PCG C++ | v1.0.0 | [imneme/pcg-cpp](https://github.com/imneme/pcg-cpp/tree/v1.0.0) | [MIT option](licenses/pcg-cpp-MIT.txt) |
| Boost.PFR | 2.1.1 | [boostorg/pfr](https://github.com/boostorg/pfr/tree/2.1.1) | [Boost](licenses/boost-pfr.txt) |
| parallel-hashmap | v1.3.12 | [greg7mdp/parallel-hashmap](https://github.com/greg7mdp/parallel-hashmap/tree/v1.3.12) | [Apache-2.0](licenses/parallel-hashmap.txt) |
| concurrentqueue | v1.0.4 | [cameron314/concurrentqueue](https://github.com/cameron314/concurrentqueue/tree/v1.0.4) | [BSD/Boost](licenses/concurrentqueue.txt), [semaphore zlib notice](licenses/concurrentqueue-semaphore.txt) |
| stb | 2025.03.14 | [nothings/stb](https://github.com/nothings/stb) | [MIT/public-domain alternatives](licenses/stb.txt) |
| SymbolProvider | v1.3.0 | [LiteLDev/SymbolProvider](https://github.com/LiteLDev/SymbolProvider/tree/6c93ec45c8455992ee726d92df60316c8e731c44) | [Source-file notice](licenses/symbolprovider.txt) |

Snappy test data and RapidJSON's jsonchecker executable are not shipped with
Lamium. Their mentions above are preserved as part of the upstream notice files.
SymbolProvider has a public-domain notice in its source file rather than a root
LICENSE file. The referenced [MinGW disclaimer](licenses/mingw-w64-DISCLAIMER.PD.txt)
is retained from [mingw-w64 at commit 57b5950](https://github.com/mingw-w64/mingw-w64/blob/57b595039040eaa15bece85b7cc71d952281b269/DISCLAIMER.PD).
Build-generated runtime import libraries remain part of the outstanding
distribution review.
