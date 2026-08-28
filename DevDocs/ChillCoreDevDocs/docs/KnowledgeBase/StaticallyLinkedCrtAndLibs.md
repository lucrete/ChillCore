## MSVC C Runtime Selection: `/MT` (Static Linking)

This project uses the **`/MT`** (Multi-threaded) compiler switch to statically link the C Runtime Library (CRT). This ensures a self-contained binary with no external dependencies on the Visual C++ Redistributable.

### **Core Concepts**

*   **CRT Linking vs. Library Linking:** 
    *   **Library Linking:** Refers to how dependencies like **GLFW** and **zlib** (Static `.lib` vs. Dynamic `.dll`) are linked.
    *   **CRT Linking:** Refers to how those libraries and the application EXE consume the **C Standard Library** (Static `/MT` vs. Dynamic `/MD`).
*   **The Unified Heap:** By using `/MT` across all components, the CRT code is physically merged into the final binary. The application code and all static libraries share a **single instance** of the memory manager. All calls to `malloc` and `free` reference the same heap logic, ensuring safety even if memory pointers are passed between modules.

To maintain stability and avoid linker errors (e.g., `LNK2005`), all components **must** be compiled with the `/MT` flag (or `/MTd` for debug builds).

#### **GLFW**
Official pre-compiled binaries are available at the [GLFW Release Page](https://github.com/glfw/glfw/releases). 
*   **Linker Warning Management:** Linking a Release-built `/MT` library into a Debug-built `/MTd` project causes a "conflicting CRT" warning. This is generally harmless. To suppress it, `LIBCMT` is added to **Ignore Specific Default Libraries** in the Debug configuration. 
*   *Note:* This suppression does **not** mask `/MT` vs `/MD` conflicts, which involve different library names (e.g., `MSVCRT`) and will still cause errors.

#### **zlib**
Zlib is compiled from source as there are no precompiled binaries available. While the [official zlib source](https://github.com/madler/zlib/releases) provides the core logic, it lacks modern Visual Studio 2022 project files for static `/MT` builds.
*   This repo is used [kiyolee/zlib-win-build](https://github.com/kiyolee/zlib-win-build/tree/main), which provides updated VS2022 configurations with statically linked CRT.

### **Pros & Cons**

*   **Pros:** Simplified deployment ("Zero-dependency" `.exe`); eliminates "Missing VCRUNTIME140.dll" errors on client machines.
*   **Cons:** Increased binary size; requires manual verification that all third-party dependencies are compiled with the `/MT` flag.
