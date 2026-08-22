# Doc 01 — Create the MegaX Project + Configuration

**You'll do this in Visual Studio Community 2026.** By the end of this doc the
empty MegaX project exists in the solution, targets **x64 Debug/Release only**,
compiles as **C++20**, and writes its build output to an **isolated folder** so
it never collides with the Sandbox.

> The engine projects and the Sandbox share `x64\<Config>\` as their output
> folder. If MegaX wrote there too, its assets and exe would mix with the
> Sandbox's. MegaX gets its **own** `bin\` / `obj\` under the project.

---

## Step 1 — Add a new project to the solution

1. Open `G:\Dev\HBE\HonestlyBadEngine.slnx` in Visual Studio 2026.
2. In **Solution Explorer**, right-click the **Solution** node →
   **Add ▸ New Project…**
3. Search for and pick **Empty Project** (C++ / Windows / Console). *Do not*
   pick a template that generates precompiled headers or sample files.
4. Configure:
   - **Project name:** `MegaX`
   - **Location:** `G:\Dev\HBE\`  → the wizard will create `G:\Dev\HBE\MegaX\`.
   - **Solution:** *Add to solution* (the existing one).
5. Click **Create**. You now have `G:\Dev\HBE\MegaX\MegaX.vcxproj`.

> If VS starts the project with a `PrecompiledHeader` setting, you'll disable it
> in Step 4. If it created any `.cpp`/`.h` stub files, delete them — we add our
> own in doc 04.

---

## Step 2 — Remove the Win32 platform (x64 only)

The whole solution is **x64-only**. A brand-new project usually comes with
`Win32` and `x64` configurations — remove `Win32`.

1. **Build ▸ Configuration Manager…**
2. In **Active solution platform**, open the drop-down → **Edit…** → select
   **Win32** → **Remove**. (If asked, also remove the project's `Win32`
   platform.)
3. Confirm MegaX now lists exactly **Debug|x64** and **Release|x64** and no
   `Win32`/`x86` anywhere.

You should end up with only these in `MegaX.vcxproj`:

```xml
<ItemGroup Label="ProjectConfigurations">
  <ProjectConfiguration Include="Debug|x64">
    <Configuration>Debug</Configuration>
    <Platform>x64</Platform>
  </ProjectConfiguration>
  <ProjectConfiguration Include="Release|x64">
    <Configuration>Release</Configuration>
    <Platform>x64</Platform>
  </ProjectConfiguration>
</ItemGroup>
```

---

## Step 3 — General configuration (both Debug|x64 and Release|x64)

Right-click **MegaX ▸ Properties**. At the top set **Configuration:**
`All Configurations` and **Platform:** `x64`, then under
**Configuration Properties ▸ General**:

| Property | Value |
|---|---|
| Configuration Type | **Application (.exe)** |
| Platform Toolset | **v145** *(matches the engine — must be identical)* |
| C++ Language Standard | **ISO C++20 Standard (/std:c++20)** |
| Character Set | **Use Unicode Character Set** |
| Windows SDK Version | **10.0 (latest installed)** |

> **Platform Toolset must match the engine (`v145`).** Linking static libs
> built with a different toolset causes cryptic link/runtime errors.

---

## Step 4 — C/C++ and linker basics (both configs)

Still in **Properties**, **Configuration: All Configurations**:

**C/C++ ▸ General**
- **Warning Level:** `Level3 (/W3)`
- **SDL checks:** `Yes (/sdl)`  *(the engine builds with this on)*

**C/C++ ▸ Language**
- **Conformance mode:** `Yes (/permissive-)`
- **C++ Language Standard:** `ISO C++20 (/std:c++20)` *(confirm it stuck)*

**C/C++ ▸ Precompiled Headers**
- **Precompiled Header:** `Not Using Precompiled Headers`
  *(MegaX has no `pch.h`; leaving this on `Use` breaks the build.)*

**Linker ▸ System**
- **SubSystem:** `Console (/SUBSYSTEM:CONSOLE)`
  *(our `main()` is a console `int main()`, and engine logs go to the console.)*

**Linker ▸ Debugging**
- **Generate Debug Info:** `Generate Debug Information (/DEBUG)`

---

## Step 5 — Preprocessor + optimization (per-config)

Switch **Configuration** to each value and set:

**Debug|x64** — C/C++ ▸ Preprocessor ▸ **Preprocessor Definitions**:
```
_DEBUG;_CONSOLE;%(PreprocessorDefinitions)
```
General ▸ **Use Debug Libraries:** `Yes`.

**Release|x64** — C/C++ ▸ Preprocessor ▸ **Preprocessor Definitions**:
```
NDEBUG;_CONSOLE;%(PreprocessorDefinitions)
```
- General ▸ **Use Debug Libraries:** `No`
- General ▸ **Whole Program Optimization:** `Yes`
- C/C++ ▸ Optimization ▸ **Function-Level Linking:** `Yes (/Gy)`
- C/C++ ▸ Optimization ▸ **Enable Intrinsic Functions:** `Yes (/Oi)`

These mirror the engine/Sandbox exactly, so all objects link cleanly.

---

## Step 6 — Isolated output & intermediate directories (both configs)

**Configuration: All Configurations**, **Configuration Properties ▸ General**:

| Property | Value |
|---|---|
| **Output Directory** | `$(ProjectDir)bin\$(Platform)\$(Configuration)\` |
| **Intermediate Directory** | `$(ProjectDir)obj\$(Platform)\$(Configuration)\` |

This is the key isolation step: `MegaX.exe`, its DLLs, and its `assets\` land
in `G:\Dev\HBE\MegaX\bin\x64\<Config>\`, completely separate from the engine
libs (which stay in `G:\Dev\HBE\x64\<Config>\`) and from the Sandbox output.

> In `MegaX.vcxproj` this appears as, inside each config's `<PropertyGroup>`:
> ```xml
> <OutDir>$(ProjectDir)bin\$(Platform)\$(Configuration)\</OutDir>
> <IntDir>$(ProjectDir)obj\$(Platform)\$(Configuration)\</IntDir>
> ```

---

## Verify (doc 01)

- [ ] Solution Explorer shows **MegaX** alongside the three `HBE.*` projects.
- [ ] MegaX has **Debug|x64** and **Release|x64** only — no `Win32`/`x86`.
- [ ] Configuration Type = **Application**, Toolset = **v145**, Standard =
      **C++20**, Character Set = **Unicode**, SubSystem = **Console**.
- [ ] Output/Intermediate dirs point into `$(ProjectDir)bin\...` /
      `$(ProjectDir)obj\...`.
- [ ] The project still has **no source files** (we add them in doc 04) — that's
      expected; it won't link yet.

Next: **`02_wire_engine_references.md`**.
