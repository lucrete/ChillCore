# OpenXR loader 1.1.54 — how these binaries were produced

Source: `https://github.com/KhronosGroup/OpenXR-SDK`, tag `release-1.1.54`.

The static loader is built rather than downloaded because the engine links the
static CRT throughout and every published loader binary links the dynamic one.
A mismatch surfaces as link errors rather than a clear diagnostic.

## One patch is required

`src/loader/CMakeLists.txt` hard-codes dynamic CRT linkage for the static
library, which overrides `CMAKE_MSVC_RUNTIME_LIBRARY` on the command line.
In the `if(MSVC)` block, the non-`DYNAMIC_LOADER` branch reads:

```cmake
PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL"
```

Drop the trailing `DLL`:

```cmake
PROPERTIES MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
```

Re-apply this on every SDK bump.

## Configure and build

```
cmake -S <sdk> -B <build> -G "Visual Studio 18 2026" -A x64 \
      -DBUILD_WITH_SYSTEM_JSONCPP=OFF \
      -DDYNAMIC_LOADER=OFF \
      -DBUILD_TESTS=OFF -DBUILD_API_LAYERS=OFF -DBUILD_CONFORMANCE_TESTS=OFF

cmake --build <build> --config Debug   --target openxr_loader
cmake --build <build> --config Release --target openxr_loader
```

`BUILD_WITH_SYSTEM_JSONCPP=OFF` matters: the loader otherwise picks up any
jsoncpp installed on the machine instead of the copy it vendors.

## What was copied here

| From | To |
| --- | --- |
| `include/openxr/*.h` | `include/openxr/` |
| `src/loader/Release/openxr_loader.lib` | `lib-vc2022/` |
| `src/loader/Debug/openxr_loaderd.lib` | `lib-vc2022/` |
| `LICENSE` | `LICENSE` |

## Verifying the CRT

The static CRT is what makes these usable. Confirm before committing a rebuild:

```
strings -a openxr_loader.lib  | grep -o 'DEFAULTLIB:"LIBCMT'
strings -a openxr_loaderd.lib | grep -o 'DEFAULTLIB:"LIBCMTD'
```

Both must match. `MSVCRT` or `MSVCRTD` means the patch above was not applied.
