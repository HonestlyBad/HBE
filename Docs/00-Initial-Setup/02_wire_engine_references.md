# Doc 02 — Wire MegaX to the Engine

This is the heart of item 00: make MegaX **consume the engine as a package**.
You'll add three things and one build step:

1. **Project references** to the three engine projects (build order + link).
2. **Include directories** — the engine's public headers + the third-party
   headers the engine's public headers expose.
3. **Library directories + dependencies** — the engine `.lib`s and SDL/OpenGL.
4. **A post-build copy step** — drop `SDL3.dll`, `SDL3_mixer.dll`, and the
   game's `assets\` next to `MegaX.exe`.
5. **Debugger working directory** — so **F5** runs find the assets.

> Everything below matches how `HBE.Sandbox` links the engine — **minus any
> Sandbox reference**. MegaX links the same engine libs and SDL/OpenGL, nothing
> from `HBE.Sandbox`.

---

## Step 1 — Add project references to the engine (NOT the Sandbox)

Right-click **MegaX ▸ Add ▸ Reference…** (or **Project ▸ Add Project
Reference…**). Check **exactly these three**:

- ☑ `HBE.Core`
- ☑ `HBE.Platform.SDL`
- ☑ `HBE.Renderer.GL`
- ☐ `HBE.Sandbox`  ← **leave unchecked. Never reference it.**

This guarantees the engine libs build before MegaX and are linked in. In the
project file it looks like:

```xml
<ItemGroup>
  <ProjectReference Include="..\HBE.Core\HBE.Core.vcxproj" />
  <ProjectReference Include="..\HBE.Platform.SDL\HBE.Platform.SDL.vcxproj" />
  <ProjectReference Include="..\HBE.Renderer.GL\HBE.Renderer.GL.vcxproj" />
</ItemGroup>
```

> VS may add a `<Project>{guid}</Project>` child to each — that's fine. The
> GUIDs for the engine projects are in `HonestlyBadEngine.slnx` if you ever need
> them.

---

## Step 2 — Additional Include Directories (both configs)

**Properties ▸ Configuration: All Configurations ▸ C/C++ ▸ General ▸
Additional Include Directories.** Add these (order doesn't matter, but keep the
trailing `%(AdditionalIncludeDirectories)`):

```
$(SolutionDir)external\SDL3\include;
$(SolutionDir)external\SDL3_mixer\include;
$(SolutionDir)external\nlohmann;
$(SolutionDir)HBE.Core\include;
$(SolutionDir)HBE.Platform.SDL\include;
$(SolutionDir)HBE.Renderer.GL\include;
$(ProjectDir)include\;
%(AdditionalIncludeDirectories)
```

Why each one:

| Path | Needed because |
|---|---|
| `external\SDL3\include` | engine public headers include `<SDL3/SDL.h>` (e.g. `SDLPlatform.h`) |
| `external\SDL3_mixer\include` | `Audio.h` pulls in SDL3_mixer |
| `external\nlohmann` | engine serialization headers expose `nlohmann/json.hpp` |
| `HBE.Core\include` | `Application.h`, `Log.h`, `Layer.h`, `AssetPaths.h`, … |
| `HBE.Platform.SDL\include` | `SDLPlatform.h`, `Audio.h`, `Input.h`, … |
| `HBE.Renderer.GL\include` | `GLRenderer.h`, `Renderer2D.h`, `Camera2D.h`, … |
| `$(ProjectDir)include\` | **your** headers, e.g. `#include "Game/GameLayer.h"` |

> You do **not** need `glad`, `glm`, `stb`, or `SDL3_ttf` on the include path —
> they're compiled *inside* the engine libs and are not exposed by the engine's
> public headers (the Sandbox builds fine without them). Don't add them.

---

## Step 3 — Library Directories + Dependencies (both configs)

**C/C++** is done; now **Linker**.

**Linker ▸ General ▸ Additional Library Directories** — add:

```
$(SolutionDir)external\SDL3\lib\x64;
$(SolutionDir)external\SDL3_mixer\lib\x64;
$(SolutionDir)x64\$(Configuration)\;
%(AdditionalLibraryDirectories)
```

> `$(SolutionDir)x64\$(Configuration)\` is where the **engine `.lib`s** land
> (`HBE.Core.lib`, etc.). Note this is the *engine's* shared output folder — we
> only *read* the libs from there; MegaX's own output still goes to its
> isolated `bin\` from doc 01.

**Linker ▸ Input ▸ Additional Dependencies** — add:

```
HBE.Core.lib;HBE.Platform.SDL.lib;HBE.Renderer.GL.lib;SDL3.lib;SDL3_mixer.lib;OpenGL32.lib;%(AdditionalDependencies)
```

| Lib | Source |
|---|---|
| `HBE.Core.lib`, `HBE.Platform.SDL.lib`, `HBE.Renderer.GL.lib` | the engine (built via the project references) |
| `SDL3.lib`, `SDL3_mixer.lib` | import libs from `external\...\lib\x64` |
| `OpenGL32.lib` | Windows system OpenGL |

> **No `SDL3_ttf.lib`** — text rendering uses stb_truetype inside the engine;
> SDL3_ttf is not used.

---

## Step 4 — Copy runtime deps next to the exe (post-build)

The engine needs `SDL3.dll` + `SDL3_mixer.dll` at runtime, and the game needs
its `assets\` folder discoverable next to the exe. Add a copy target that runs
after every build.

**How:** right-click `MegaX.vcxproj` in Solution Explorer → **Unload Project**
→ right-click → **Edit MegaX.vcxproj**. Paste this block **just before** the
final `<Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />` line, then
reload:

```xml
<!-- Runtime deps copied next to MegaX.exe for every config. -->
<ItemGroup>
  <MegaXRuntimeDll Include="$(SolutionDir)external\SDL3\lib\x64\SDL3.dll" />
  <MegaXRuntimeDll Include="$(SolutionDir)external\SDL3_mixer\lib\x64\SDL3_mixer.dll" />
  <MegaXAsset Include="$(ProjectDir)assets\**\*" />
</ItemGroup>
<Target Name="MegaXCopyRuntime" AfterTargets="Build">
  <Message Importance="high" Text="MegaX: copying runtime deps to $(TargetDir)" />
  <Copy SourceFiles="@(MegaXRuntimeDll)" DestinationFolder="$(TargetDir)"
        SkipUnchangedFiles="true" Retries="3" />
  <Copy SourceFiles="@(MegaXAsset)"
        DestinationFiles="@(MegaXAsset->'$(TargetDir)assets\%(RecursiveDir)%(Filename)%(Extension)')"
        SkipUnchangedFiles="true" Retries="3" />
</Target>
```

After a build you should see `MegaX: copying runtime deps to …` in the output,
and `bin\x64\<Config>\` will contain `MegaX.exe`, `SDL3.dll`,
`SDL3_mixer.dll`, and an `assets\` mirror.

> This is the same mechanism the Sandbox uses (`HBECopyRuntime`), renamed for
> MegaX and pointed at MegaX's own assets. It does **not** touch the Sandbox.

---

## Step 5 — Debugger working directory (both configs)

So **F5** runs with the working directory set to the folder that has the DLLs
and `assets\`.

**Properties ▸ Configuration: All Configurations ▸ Debugging ▸ Working
Directory:**

```
$(TargetDir)
```

This writes `MegaX.vcxproj.user`:

```xml
<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
  <LocalDebuggerWorkingDirectory>$(TargetDir)</LocalDebuggerWorkingDirectory>
  <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
</PropertyGroup>
<PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
  <LocalDebuggerWorkingDirectory>$(TargetDir)</LocalDebuggerWorkingDirectory>
  <DebuggerFlavor>WindowsLocalDebugger</DebuggerFlavor>
</PropertyGroup>
```

---

## Reference — complete `MegaX.vcxproj`

If you'd rather verify against a finished file, this is what MegaX should look
like after docs 01–02 (source files from doc 04 are included here so it's
build-ready). You can diff your generated file against this.

```xml
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
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
  <ItemGroup>
    <ClCompile Include="src\main.cpp" />
    <ClCompile Include="src\Game\GameLayer.cpp" />
  </ItemGroup>
  <ItemGroup>
    <ClInclude Include="include\Game\GameLayer.h" />
  </ItemGroup>
  <ItemGroup>
    <ProjectReference Include="..\HBE.Core\HBE.Core.vcxproj" />
    <ProjectReference Include="..\HBE.Platform.SDL\HBE.Platform.SDL.vcxproj" />
    <ProjectReference Include="..\HBE.Renderer.GL\HBE.Renderer.GL.vcxproj" />
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <VCProjectVersion>18.0</VCProjectVersion>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>MegaX</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v145</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v145</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <OutDir>$(ProjectDir)bin\$(Platform)\$(Configuration)\</OutDir>
    <IntDir>$(ProjectDir)obj\$(Platform)\$(Configuration)\</IntDir>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <OutDir>$(ProjectDir)bin\$(Platform)\$(Configuration)\</OutDir>
    <IntDir>$(ProjectDir)obj\$(Platform)\$(Configuration)\</IntDir>
  </PropertyGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <SDLCheck>true</SDLCheck>
      <PreprocessorDefinitions>_DEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <ConformanceMode>true</ConformanceMode>
      <LanguageStandard>stdcpp20</LanguageStandard>
      <AdditionalIncludeDirectories>$(SolutionDir)external\SDL3\include;$(SolutionDir)external\SDL3_mixer\include;$(SolutionDir)external\nlohmann;$(SolutionDir)HBE.Core\include;$(SolutionDir)HBE.Platform.SDL\include;$(SolutionDir)HBE.Renderer.GL\include;$(ProjectDir)include\;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <AdditionalLibraryDirectories>$(SolutionDir)external\SDL3\lib\x64;$(SolutionDir)external\SDL3_mixer\lib\x64;$(SolutionDir)x64\$(Configuration)\;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
      <AdditionalDependencies>HBE.Core.lib;HBE.Platform.SDL.lib;HBE.Renderer.GL.lib;SDL3.lib;SDL3_mixer.lib;OpenGL32.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <FunctionLevelLinking>true</FunctionLevelLinking>
      <IntrinsicFunctions>true</IntrinsicFunctions>
      <SDLCheck>true</SDLCheck>
      <PreprocessorDefinitions>NDEBUG;_CONSOLE;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <ConformanceMode>true</ConformanceMode>
      <LanguageStandard>stdcpp20</LanguageStandard>
      <AdditionalIncludeDirectories>$(SolutionDir)external\SDL3\include;$(SolutionDir)external\SDL3_mixer\include;$(SolutionDir)external\nlohmann;$(SolutionDir)HBE.Core\include;$(SolutionDir)HBE.Platform.SDL\include;$(SolutionDir)HBE.Renderer.GL\include;$(ProjectDir)include\;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <AdditionalLibraryDirectories>$(SolutionDir)external\SDL3\lib\x64;$(SolutionDir)external\SDL3_mixer\lib\x64;$(SolutionDir)x64\$(Configuration)\;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
      <AdditionalDependencies>HBE.Core.lib;HBE.Platform.SDL.lib;HBE.Renderer.GL.lib;SDL3.lib;SDL3_mixer.lib;OpenGL32.lib;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
    <MegaXRuntimeDll Include="$(SolutionDir)external\SDL3\lib\x64\SDL3.dll" />
    <MegaXRuntimeDll Include="$(SolutionDir)external\SDL3_mixer\lib\x64\SDL3_mixer.dll" />
    <MegaXAsset Include="$(ProjectDir)assets\**\*" />
  </ItemGroup>
  <Target Name="MegaXCopyRuntime" AfterTargets="Build">
    <Message Importance="high" Text="MegaX: copying runtime deps to $(TargetDir)" />
    <Copy SourceFiles="@(MegaXRuntimeDll)" DestinationFolder="$(TargetDir)"
          SkipUnchangedFiles="true" Retries="3" />
    <Copy SourceFiles="@(MegaXAsset)"
          DestinationFiles="@(MegaXAsset->'$(TargetDir)assets\%(RecursiveDir)%(Filename)%(Extension)')"
          SkipUnchangedFiles="true" Retries="3" />
  </Target>
  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
```

> This reference file omits the `Microsoft.Cpp.$(Platform).user.props`
> `ImportGroup`s VS normally emits — harmless if yours has them. The important
> parts are the toolset, standard, include/lib dirs, dependencies, output dirs,
> and the copy target.

---

## Verify (doc 02)

- [ ] MegaX references `HBE.Core`, `HBE.Platform.SDL`, `HBE.Renderer.GL` — and
      **not** `HBE.Sandbox`.
- [ ] Include dirs list the 3 engine `include\` folders + SDL3 + SDL3_mixer +
      nlohmann + `$(ProjectDir)include\`.
- [ ] Linker deps include the 3 `HBE.*.lib` + `SDL3.lib` + `SDL3_mixer.lib` +
      `OpenGL32.lib` (no `SDL3_ttf`).
- [ ] The `MegaXCopyRuntime` target is present.
- [ ] Debugger working directory = `$(TargetDir)`.

Next: **`03_folder_structure_and_assets.md`**.
