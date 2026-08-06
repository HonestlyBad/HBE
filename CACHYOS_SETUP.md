# CachyOS Setup — HBE + Copilot CLI

Everything you need to go from a fresh CachyOS install to building & running HBE / MegaX / HBMapMaker in JetBrains CLion, plus running the same GitHub Copilot CLI you use on Windows.

CachyOS is Arch-based, so all commands here use `pacman` / `yay`.

---

## 1. One-time system setup

### 1.1 Base build tools

```bash
sudo pacman -Syu   # keep the system fresh first
sudo pacman -S --needed base-devel cmake ninja git clang lld pkgconf
```

### 1.2 HBE runtime dependencies

```bash
sudo pacman -S --needed sdl3 sdl3_ttf glm mesa libglvnd
```

### 1.3 SDL3_mixer (AUR)

SDL3_mixer is not yet in the official Arch repos. Install an AUR helper if you don't have one, then pull it:

```bash
# If you don't have an AUR helper yet:
sudo pacman -S --needed yay
# or:  sudo pacman -S --needed paru

yay -S sdl3_mixer
```

> If AUR fails or you don't want it, HBE will still build — the CMake config emits a warning and links against a stub. Audio-dependent code paths will link but be no-ops until SDL3_mixer is available.

### 1.4 Vendored header-only / generated dependencies

A few third-party sources are referenced by the CMake build but are **not**
checked into git and are **not** installable via `pacman` in the layout HBE
expects. You must vendor them once per clone before `cmake --preset
linux-clang` will fully configure and build:

```bash
# glad (classic 0.1.x, OpenGL 3.3 core loader) — HBE.Renderer.GL/external/glad
python3 -m venv /tmp/gladenv && /tmp/gladenv/bin/pip install glad
/tmp/gladenv/bin/glad --profile core --api gl=3.3 --generator c \
    --out-path HBE.Renderer.GL/external/glad --reproducible
rm -rf /tmp/gladenv

# nlohmann/json single header — external/nlohmann/json.hpp
mkdir -p external/nlohmann
curl -fsSL -o external/nlohmann/json.hpp \
    https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp

# stb single-header libs — external/stb/{stb_image.h,stb_truetype.h}
mkdir -p external/stb
curl -fsSL -o external/stb/stb_image.h \
    https://raw.githubusercontent.com/nothings/stb/master/stb_image.h
curl -fsSL -o external/stb/stb_truetype.h \
    https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h
```

This produces:

```
HBE.Renderer.GL/external/glad/src/glad.c
HBE.Renderer.GL/external/glad/include/glad/glad.h
HBE.Renderer.GL/external/glad/include/KHR/khrplatform.h
external/nlohmann/json.hpp
external/stb/stb_image.h
external/stb/stb_truetype.h
```

> `glm` does **not** need vendoring — the system package (`sdl3`/`glm` from
> step 1.2) provides `/usr/include/glm`, which CMake's `find_package`/include
> paths pick up directly.

### 1.5 (Recommended) JetBrains Toolbox → CLion

```bash
yay -S jetbrains-toolbox
```

Launch **JetBrains Toolbox**, sign in, and install **CLion** from the app list. CLion is the right JetBrains IDE for a native C++ CMake project on Linux (Rider is C#/.NET-first and can't build C++ vcxproj on Linux).

---

## 2. Pulling the project down

```bash
cd ~
mkdir -p Dev && cd Dev
git clone https://github.com/AlbertTuloIV/HBE.git
cd HBE

# Switch to the branch that has the CMake build
git fetch origin
git checkout linux/cmake-setup
```

Once `linux/cmake-setup` is merged into `main`, the last two lines become just `git checkout main`.

---

## 3. Building from the terminal (fastest smoke test)

```bash
cmake --preset linux-clang
cmake --build --preset linux-clang-debug
```

This produces:

```
build/linux-clang/bin/Debug/HBE.Sandbox
build/linux-clang/bin/Debug/MegaX
build/linux-clang/bin/Debug/HBMapMaker
build/linux-clang/bin/Debug/assets/...
```

Run any of them:

```bash
./build/linux-clang/bin/Debug/HBE.Sandbox
./build/linux-clang/bin/Debug/MegaX
./build/linux-clang/bin/Debug/HBMapMaker
```

### Build only one target

```bash
cmake --build --preset linux-clang-debug --target MegaX
```

### Release build

```bash
cmake --build --preset linux-clang-release
```

### Prefer gcc?

Swap `linux-clang` for `linux-gcc` in every command above.

### Clean rebuild

```bash
rm -rf build/linux-clang
cmake --preset linux-clang
cmake --build --preset linux-clang-debug
```

---

## 4. Opening the project in CLion

1. **File → Open…** → select the `HBE/` folder (not a single subproject).
2. CLion detects `CMakePresets.json` automatically and offers profiles:
   - **linux-clang** (recommended)
   - **linux-gcc**
   - `windows-msvc` (grayed out on Linux — ignore it)
3. Pick **linux-clang**, click **OK**. CLion will run CMake once — you should see the `==== HBE build config ====` block near the end of the CMake output tab.
4. In the **run configurations** dropdown (top-right), select `HBE.Sandbox`, `MegaX`, or `HBMapMaker` and hit the green ▶ / 🐞 buttons.

### If CLion says "no toolchain found"

**Settings → Build, Execution, Deployment → Toolchains → +** → **System**. Set:

- **C compiler:** `/usr/bin/clang`
- **C++ compiler:** `/usr/bin/clang++`
- **Debugger:** `/usr/bin/gdb` (or `/usr/bin/lldb`)

Then reload the CMake project (**File → Reload CMake Project**).

### Working directory for debugging

Assets are copied next to each executable as a post-build step, so the default working dir (the exe's folder) is already correct. If you edit assets under `HBE.Sandbox/assets/` you either need to rebuild (post-build re-copies) or set `HBE_ASSET_ROOT`:

```bash
export HBE_ASSET_ROOT="$HOME/Dev/HBE/HBE.Sandbox/assets"
```

In CLion: **Edit Configurations → Environment variables → `HBE_ASSET_ROOT=...`**.

---

## 5. Installing GitHub Copilot CLI on CachyOS

Two easy options — pick one.

### Option A: npm (matches how it works on Windows)

Requires Node.js 22+.

```bash
sudo pacman -S --needed nodejs npm
node --version    # confirm >= v22
npm install -g @github/copilot
```

If your `npm` global prefix isn't on `PATH`, add it:

```bash
npm config get prefix        # usually /usr or ~/.npm-global
echo 'export PATH="$(npm config get prefix)/bin:$PATH"' >> ~/.bashrc
# or ~/.zshrc if you use zsh
source ~/.bashrc
```

### Option B: Official install script

```bash
curl -fsSL https://gh.io/copilot-install | bash
```

This installs to `$HOME/.local/bin` by default. Make sure that's on your `PATH`:

```bash
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### Verify & log in

```bash
which copilot
copilot --version
cd ~/Dev/HBE
copilot
```

On first launch:

1. It'll ask whether it can operate on files in this folder — pick **Yes, and remember this folder for future sessions**.
2. Then run `/login` inside the CLI and follow the browser flow. Sign in with the same GitHub account that has your Copilot subscription.

### Updating later

```bash
# npm install:
npm install -g @github/copilot@latest

# or from inside the CLI:
/update
```

### Optional: enable multiline input in your terminal

Inside Copilot CLI: `/terminal-setup` — one-time setup so `Shift+Enter` inserts newlines.

---

## 6. Troubleshooting

| Symptom | Fix |
|---|---|
| `Cannot find source file: .../glad/src/glad.c` or `fatal error: 'json.hpp'/'stb_image.h'/'stb_truetype.h' file not found` | Vendored deps missing — run the commands in step 1.4 to generate/download `glad`, `nlohmann/json.hpp`, and `stb_image.h`/`stb_truetype.h`. |
| `Could not find SDL3` | `sudo pacman -S sdl3`. If already installed, ensure `pkgconf` is installed too. |
| `SDL3_mixer not found via find_package` (warning) | Install via AUR: `yay -S sdl3_mixer`. HBE will still build without it. |
| Runtime: `libSDL3.so.0: cannot open shared object` | Re-run `sudo pacman -S sdl3` — the pacman-provided `.so` goes in `/usr/lib/`, no `LD_LIBRARY_PATH` needed. |
| Black window / no OpenGL context | Install `mesa` + `libglvnd` (already in step 1.2). On NVIDIA, install the proprietary driver via `sudo pacman -S nvidia nvidia-utils` and reboot. |
| CLion doesn't see `linux-clang` preset | Reload CMake (**File → Reload CMake Project**) and make sure a Linux clang toolchain is registered. |
| `cmake: command not found` after install | `sudo pacman -S cmake`, then open a new shell. |
| Copilot CLI says "command not found" | The install directory isn't on `PATH`. See the `PATH` snippets in step 5. |
| Copilot CLI fails on login | Confirm your GitHub account has an active Copilot subscription and that org/enterprise policy allows Copilot CLI. |

---

## 7. Quick reference (copy-paste after every fresh clone)

```bash
git checkout linux/cmake-setup                 # or main once merged
cmake --preset linux-clang
cmake --build --preset linux-clang-debug
./build/linux-clang/bin/Debug/HBE.Sandbox
```

That's it — you're building HBE on Linux.
