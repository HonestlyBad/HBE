# 04 — The command line (`MegaX/src/main.cpp`)

This doc edits one file. `main.cpp` is 36 lines and five of
them change, so it is given as a **full replacement** rather
than five hunks — replace the whole file and you are done.

It depends on docs `01`–`03`. After this doc every
`--capture*` switch works and the item's code is complete;
doc `05` documents the output and doc `06` builds and
verifies.

`main.cpp` is **tab-indented**. The block below preserves
that.

---

## 1. What changes and why

| Change | Why |
|---|---|
| `#include "Game/PerfCapture.h"` | `ParseCaptureArgs`, `PrintCaptureUsage`, `PerfCaptureRequest` |
| `#include <cstring>` | `std::strcmp` for the `--help` scan |
| `int main()` → `int main(int argc, char** argv)` | there is no other way to see the command line |
| the `--help` loop | prints the switches and exits **before** SDL initializes, so `--help` works headless and instantly |
| `ParseCaptureArgs(argc, argv)` | one call, result held `const` for the rest of `main` |
| `cfg.vsync = !captureRequest.disableVsync;` | `--no-vsync`; must happen before `app.initialize`, because that is when the GL swap interval is set |
| `pushLayer` split into three lines | the layer must be configured before it is pushed, and `pushLayer` takes ownership |

> **WHY CONFIGURE BEFORE PUSHING?**
> `Application::pushLayer(std::unique_ptr<Layer>)` takes the
> layer by value and returns `void` — once you have moved
> it in, there is no handle to reach it through. Building
> the `unique_ptr` first, calling `setCaptureRequest` on it,
> then `std::move`-ing it in is the whole trick.
> `pushLayer` also calls `onAttach` immediately, so the
> request must be in place before that runs: `configure()`
> is what puts the state machine into `Warmup`, and
> `onAttach` is where `setSceneLabel` lands.

> **WHY IS `--no-vsync` PARSED BY `ParseCaptureArgs`?** It
> is not a capture setting — `main` consumes it, not
> `PerfCapture`. But it rides in the same struct so the
> recorder can print `vsync : off` into the `.meta.txt`.
> Without that line, two captures with wildly different
> frame times and no visible reason are indistinguishable a
> week later.

---

## 2. Full replacement — `MegaX/src/main.cpp`

Open `/home/atulo/Projects/HBE/MegaX/src/main.cpp` and
replace lines **1-36 inclusive** — that is, the entire file
— with:

```cpp
#include "HBE/Core/Application.h"
#include "HBE/Core/Log.h"

#include "Game/GameLayer.h"
#include "Game/PerfCapture.h"

#include <cstring>
#include <memory>

using namespace HBE::Core;
using namespace HBE::Platform;

int main(int argc, char** argv) {
	SetLogLevel(LogLevel::Info);

	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
			MegaX::PrintCaptureUsage();
			return 0;
		}
	}

	const MegaX::PerfCaptureRequest captureRequest = MegaX::ParseCaptureArgs(argc, argv);

	WindowConfig cfg;
	cfg.title = "MegaX";
	cfg.width = 1280;
	cfg.height = 720;
	cfg.useOpenGL = true;
	cfg.mode = WindowMode::Windowed;
	cfg.vsync = !captureRequest.disableVsync;

	AssetPaths::Config assetCfg{};
	assetCfg.organization = "MegaX";
	assetCfg.application = "MegaX";
	assetCfg.siblingProjectNames = { "MegaX" };

	Application app;
	if (!app.initialize(cfg, assetCfg)) {
		return -1;
	}

	auto gameLayer = std::make_unique<MegaX::GameLayer>();
	gameLayer->setCaptureRequest(captureRequest);
	app.pushLayer(std::move(gameLayer));

	app.run();

	return 0;
}
```

The file goes from 36 to 50 lines. Everything not in the
table in §1 — the window title, size, mode, the
`AssetPaths::Config`, the `initialize` failure path, the
`return 0` — is byte-for-byte what it was.

> **PASTE NOTE.** Indentation inside `main` is a single tab
> per level, including the nested `if` inside the `--help`
> loop (two tabs) and its body (three tabs). If your editor
> is set to insert spaces, this file will look right and
> `git diff` will show every line as changed. Check with
> `grep -P "^\t" MegaX/src/main.cpp | wc -l` — it should
> report 28.

---

## 3. The switches, end to end

With docs `01`–`04` applied, this is the full surface:

```fish
cd /home/atulo/Projects/HBE

# What's available (exits immediately, no window)
./build/linux-clang/bin/Debug/MegaX --help

# Ten-second baseline, uncapped, exits by itself
./build/linux-clang/bin/Debug/MegaX --capture --capture-seconds 10 \
    --capture-warmup 3 --no-vsync --capture-label baseline --capture-quit

# Same, scored against the 120 FPS budget instead
./build/linux-clang/bin/Debug/MegaX --capture-seconds 10 --no-vsync \
    --capture-120 --capture-label baseline-120 --capture-quit

# Write somewhere specific instead of the user-data root
./build/linux-clang/bin/Debug/MegaX --capture-seconds 5 --no-vsync \
    --capture-out /tmp/megax-run.csv --capture-quit

# No switches at all: nothing changes until you press F9
./build/linux-clang/bin/Debug/MegaX
```

Both spellings work for every switch that takes a value —
`--capture-seconds 10` and `--capture-seconds=10` are the
same thing.

An unrecognized switch that starts with `--capture` warns
and is ignored:

```
[WARN][PerfCapture] unknown option '--capture-duration' — ignored.
```

Anything else on the command line is silently ignored, which
is deliberate: SDL, Mesa and the desktop all pass their own
arguments through on occasion, and MegaX has no business
rejecting them.

---

## 4. What NOT to touch

* Do **not** move the `--help` loop below
  `app.initialize(...)`. Half its value is that it answers
  without opening a window, which is what makes it usable
  over SSH and in a script.
* Do **not** set `cfg.vsync = false` unconditionally "for
  profiling". The default must stay `true` — that is how the
  game ships and how it is normally played. `--no-vsync` is
  opt-in per run.
* Do **not** call `app.platform().applyGraphicsSettings(...)`
  to change vsync after `initialize`. It works, but it is a
  second code path for the same decision, and the value is
  needed before the GL context exists anyway.
* Do **not** parse the capture switches inside `GameLayer`.
  `main` owns `argc`/`argv`; the layer receives a struct.
  Keeping it that way is what lets `F9` and the command line
  share one implementation.
* Do **not** add a `--map` or `--difficulty` switch here.
  Both are tempting for repeatability and both belong to
  item 20, which defines the golden room and its fixed
  spawns.

---

## 5. Sanity check before doc 05

```fish
grep -c "argc" /home/atulo/Projects/HBE/MegaX/src/main.cpp
# -> 3  (the signature, the --help loop, the ParseCaptureArgs call)

grep -c "captureRequest" /home/atulo/Projects/HBE/MegaX/src/main.cpp
# -> 3  (declaration, cfg.vsync, setCaptureRequest)

grep -n "pushLayer" /home/atulo/Projects/HBE/MegaX/src/main.cpp
# -> 1 match, and it must read: app.pushLayer(std::move(gameLayer));

grep -c "make_unique" /home/atulo/Projects/HBE/MegaX/src/main.cpp
# -> 1
```

The project should now build **and** the switches should
work:

```fish
cd /home/atulo/Projects/HBE
cmake --build --preset linux-clang-debug --target MegaX
./build/linux-clang/bin/Debug/MegaX --help
```

Expected: ten `[INFO]` lines and an immediate exit with
status 0, no window.

If it fails with
``error: too many arguments to function call, expected 0``
at `ParseCaptureArgs`, doc `01`'s declaration was mistyped.

If `--help` opens a window and starts the game, the loop
landed after `app.initialize` instead of before it.

If it fails with
``error: call to implicitly-deleted copy constructor of 'std::unique_ptr<...>'``,
the `std::move` in `pushLayer` was dropped.

Next: `05_capture_csv_format.md`.
