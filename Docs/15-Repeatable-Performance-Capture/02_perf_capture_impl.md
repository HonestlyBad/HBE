# 02 — The capture implementation (`Game/PerfCapture.cpp`)

This doc creates `MegaX/src/Game/PerfCapture.cpp`, registers
it in `MegaX/CMakeLists.txt`, and adds one opt-in CMake
option. It depends on `PerfCapture.h` from doc `01` and on
nothing else in this item — after this doc `MegaX` compiles
and links again, even though nothing calls the new code yet.

The file is long (625 lines). §1 gives it in four
consecutive blocks so you can paste and check a section at a
time; **the four blocks concatenated, in order, are the
entire file** — there is nothing between them and nothing
after.

---

## 1. Create the file

**Full path:** `/home/atulo/Projects/HBE/MegaX/src/Game/PerfCapture.cpp`

**Status:** the file must not exist yet.

Indentation is **4 spaces**, matching every other file under
`MegaX/src/Game/`.

### 1.1 Block one — includes and file-local helpers

```cpp
#include "Game/PerfCapture.h"

#include "HBE/Core/AssetPaths.h"
#include "HBE/Core/Log.h"
#include "HBE/Core/Profiler.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <system_error>

#if defined(__linux__)
    #include <unistd.h>
#endif

using namespace HBE::Core;

namespace MegaX {

    namespace {

        // The CSV header. Must stay in lockstep with writeCsv()'s row
        // format and with PerfSample's field order.
        const char* kCsvHeader =
            "frame,t,frameMs,updateMs,renderMs,gpuMs,"
            "sceneUpdateMs,physicsMs,combatMs,aiMs,particlesMs,"
            "tileRenderMs,spriteRenderMs,"
            "drawCalls,passes,submittedQuads,renderedQuads,culledSprites,"
            "materialChanges,textureChanges,"
            "entities,enemies,bullets,enemyBullets,particles,lights,rssKB";

        // Profiler section names are `const char*` literals owned by the
        // translation unit that opened the scope, so pointer identity is
        // useless across HBE.Core and MegaX. Compare the text.
        double sectionMs(const Profiler::Snapshot& snap, const char* name) {
            for (const auto& s : snap.sections) {
                if (s.name && std::strcmp(s.name, name) == 0) {
                    return s.currentMs;
                }
            }
            return 0.0;
        }

        // Resident set size in KiB. Linux reads /proc/self/statm; every
        // other platform reports 0 and the column stays honest.
        long readResidentKB() {
#if defined(__linux__)
            std::FILE* f = std::fopen("/proc/self/statm", "r");
            if (!f) return 0;

            unsigned long totalPages = 0;
            unsigned long residentPages = 0;
            const int read = std::fscanf(f, "%lu %lu", &totalPages, &residentPages);
            std::fclose(f);
            if (read != 2) return 0;

            const long pageKB = static_cast<long>(sysconf(_SC_PAGESIZE)) / 1024;
            return static_cast<long>(residentPages) * pageKB;
#else
            return 0;
#endif
        }

        // Nearest-rank percentile over an already-sorted vector.
        double percentile(const std::vector<double>& sorted, double pct) {
            if (sorted.empty()) return 0.0;
            const double rank = (pct / 100.0) * static_cast<double>(sorted.size());
            std::size_t idx = static_cast<std::size_t>(rank);
            if (idx >= sorted.size()) idx = sorted.size() - 1;
            return sorted[idx];
        }

        std::string sanitizeForFilename(const std::string& in) {
            std::string out = in.empty() ? std::string("run") : in;
            for (char& c : out) {
                const bool ok = (c >= 'a' && c <= 'z')
                             || (c >= 'A' && c <= 'Z')
                             || (c >= '0' && c <= '9')
                             || c == '-' || c == '_';
                if (!ok) c = '-';
            }
            return out;
        }

        std::string timestampNow() {
            char stamp[32] = "00000000-000000";
            const std::time_t now = std::time(nullptr);
            std::tm tmv{};
#if defined(_WIN32)
            localtime_s(&tmv, &now);
#else
            localtime_r(&now, &tmv);
#endif
            std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &tmv);
            return std::string(stamp);
        }

        const char* buildConfigName() {
#if defined(NDEBUG)
            return "release";
#else
            return "debug";
#endif
        }

        // Accepts both "--flag value" and "--flag=value". Advances `i`
        // past the value when the separate form was used.
        bool matchOption(int argc, char** argv, int& i,
                         const char* flag, std::string& outValue)
        {
            const char* arg = argv[i];
            const std::size_t flagLen = std::strlen(flag);

            if (std::strncmp(arg, flag, flagLen) != 0) return false;

            if (arg[flagLen] == '=') {
                outValue = std::string(arg + flagLen + 1);
                return true;
            }
            if (arg[flagLen] != '\0') return false;   // e.g. --capture-out2

            if (i + 1 >= argc) {
                LogWarn(std::string("[PerfCapture] ") + flag + " needs a value — ignored.");
                return false;
            }
            outValue = std::string(argv[++i]);
            return true;
        }

        bool parsePositiveDouble(const std::string& text, const char* flag, double& out) {
            char* end = nullptr;
            const double v = std::strtod(text.c_str(), &end);
            if (end == text.c_str() || v <= 0.0) {
                LogWarn(std::string("[PerfCapture] bad value for ") + flag
                        + " ('" + text + "') — keeping the default.");
                return false;
            }
            out = v;
            return true;
        }
    }
```

> **PASTE NOTE.** The `#if defined(__linux__)` /
> `#if defined(_WIN32)` / `#if defined(NDEBUG)` directives
> are deliberately flush against the left margin inside
> indented functions. That is the C convention and matches
> how `Profiler.h` and `AssetPaths.cpp` already write theirs.
> Do not indent them to match the surrounding code.

> **WHY `long`, NOT `std::size_t`, FOR `rssKB`?** `sysconf`
> returns `long`, and the CSV writer prints the value with
> `%ld`. Keeping one type end to end avoids a
> `-Wformat` warning that would only appear on one platform.

### 1.2 Block two — the budgets

```cpp

    // ---------------------------------------------------------------
    // Budgets
    // ---------------------------------------------------------------

    PerfBudget MakeBudget60() {
        PerfBudget b{};
        b.name           = "60fps";
        b.frameMs        = 16.67;
        b.updateMs       = 6.00;
        b.renderMs       = 4.00;
        b.gpuMs          = 8.00;
        b.drawCalls      = 64;
        b.submittedQuads = 4000;
        b.particles      = 1500;
        return b;
    }

    PerfBudget MakeBudget120() {
        PerfBudget b{};
        b.name           = "120fps";
        b.frameMs        = 8.33;
        b.updateMs       = 3.00;
        b.renderMs       = 2.00;
        b.gpuMs          = 4.00;
        b.drawCalls      = 48;
        b.submittedQuads = 3000;
        b.particles      = 1000;
        return b;
    }
```

The 60 FPS values repeat the defaults already in
`PerfBudget`'s member initializers. That duplication is
intentional: the struct's defaults keep a
default-constructed `PerfBudget` meaningful, and the factory
is the thing you edit when re-baselining, so it states every
field explicitly rather than relying on which ones it left
alone.

### 1.3 Block three — the command line

```cpp

    // ---------------------------------------------------------------
    // Command line
    // ---------------------------------------------------------------

    void PrintCaptureUsage() {
        LogInfo("MegaX performance capture options:");
        LogInfo("  --capture                 start a capture automatically after the warmup");
        LogInfo("  --capture-seconds <N>     how long to record (default 10)");
        LogInfo("  --capture-warmup <N>      seconds to run before recording (default 3)");
        LogInfo("  --capture-label <text>    label folded into the output filename");
        LogInfo("  --capture-out <path>      explicit .csv path (default: user data captures/)");
        LogInfo("  --capture-120             score the run against the 120 FPS budget");
        LogInfo("  --capture-quit            quit once the capture is written");
        LogInfo("  --no-vsync                run uncapped so the capture measures real headroom");
        LogInfo("  --help                    print this list and exit");
        LogInfo("In-game, F9 starts/stops a capture with the same settings.");
    }

    PerfCaptureRequest ParseCaptureArgs(int argc, char** argv) {
        PerfCaptureRequest req{};
        if (argv == nullptr) return req;

        for (int i = 1; i < argc; ++i) {
            const char* arg = argv[i];
            if (arg == nullptr) continue;

            std::string value;

            if (std::strcmp(arg, "--capture") == 0) {
                req.enabled = true;
                continue;
            }
            if (std::strcmp(arg, "--capture-quit") == 0) {
                req.quitWhenDone = true;
                req.enabled = true;
                continue;
            }
            if (std::strcmp(arg, "--capture-120") == 0) {
                req.useBudget120 = true;
                continue;
            }
            if (std::strcmp(arg, "--no-vsync") == 0) {
                req.disableVsync = true;
                continue;
            }
            if (matchOption(argc, argv, i, "--capture-seconds", value)) {
                if (parsePositiveDouble(value, "--capture-seconds", req.durationSeconds)) {
                    req.enabled = true;
                }
                continue;
            }
            if (matchOption(argc, argv, i, "--capture-warmup", value)) {
                parsePositiveDouble(value, "--capture-warmup", req.warmupSeconds);
                continue;
            }
            if (matchOption(argc, argv, i, "--capture-label", value)) {
                req.label = value;
                continue;
            }
            if (matchOption(argc, argv, i, "--capture-out", value)) {
                req.outPath = value;
                continue;
            }
            if (std::strncmp(arg, "--capture", 9) == 0) {
                LogWarn(std::string("[PerfCapture] unknown option '") + arg + "' — ignored.");
            }
        }

        return req;
    }
```

Two behaviors worth knowing before you use it:

* `--capture-quit` and `--capture-seconds` **imply**
  `--capture`. Asking for a duration or an exit is asking
  for a capture, and having to write `--capture
  --capture-seconds 10` every time is friction with no
  benefit.
* `--capture-warmup`, `--capture-label`, `--capture-out`,
  `--capture-120` and `--no-vsync` do **not** imply it, so
  you can set them once in a shell alias and still press
  `F9` manually.
* The `--capture-seconds` / `--capture-warmup` order in the
  `if` chain matters only in that `matchOption` consumes the
  next argv entry when the separate form is used. That is
  why the flag comparisons come first: `--capture` must not
  swallow the `--capture-seconds` that follows it.

### 1.4 Block four — the recorder

```cpp

    // ---------------------------------------------------------------
    // PerfCapture
    // ---------------------------------------------------------------

    void PerfCapture::configure(const PerfCaptureRequest& req) {
        m_req = req;
        if (m_req.durationSeconds <= 0.0) m_req.durationSeconds = 10.0;
        if (m_req.warmupSeconds < 0.0)    m_req.warmupSeconds = 0.0;

        if (m_req.enabled) {
            m_state = State::Warmup;
            m_warmupClock = 0.0;

            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "[PerfCapture] armed: %.1f s warmup, then %.1f s of capture (%s budget).",
                m_req.warmupSeconds, m_req.durationSeconds,
                m_req.useBudget120 ? "120fps" : "60fps");
            LogInfo(buf);
        }
    }

    void PerfCapture::setSceneLabel(const std::string& scenePath) {
        m_sceneLabel = scenePath.empty() ? std::string("unknown") : scenePath;
    }

    void PerfCapture::tick(float dt, const FrameCounts& counts) {
        const double d = (dt > 0.0f) ? static_cast<double>(dt) : 0.0;

        if (m_state == State::Warmup) {
            m_warmupClock += d;
            if (m_warmupClock >= m_req.warmupSeconds) {
                m_startedBy = "command line";
                start("command line");
            }
            return;
        }

        if (m_state != State::Recording) return;

        record(counts);
        m_captureClock += d;

        if (m_captureClock >= m_req.durationSeconds) {
            stopAndWrite();
            return;
        }
        if (m_samples.size() >= kMaxSamples) {
            LogWarn("[PerfCapture] sample cap reached — stopping early.");
            stopAndWrite();
        }
    }

    void PerfCapture::toggle() {
        if (m_state == State::Recording) {
            LogInfo("[PerfCapture] stopped early (F9).");
            stopAndWrite();
            return;
        }
        m_startedBy = "F9";
        start("F9");
    }

    void PerfCapture::start(const char* reason) {
        m_state = State::Recording;
        m_captureClock = 0.0;
        m_lastFrameIndex = 0;
        m_samples.clear();

        // 1000 Hz of headroom: with vsync off a Debug frame can take well
        // under a millisecond, and a reallocation mid-capture would show up
        // as a memcpy spike in the very numbers we are measuring.
        std::size_t want = static_cast<std::size_t>(m_req.durationSeconds * 1000.0) + 512;
        if (want > kMaxSamples) want = kMaxSamples;
        m_samples.reserve(want);

        char buf[320];
        std::snprintf(buf, sizeof(buf),
            "[PerfCapture] recording %.1f s (%s) — scene '%s'.",
            m_req.durationSeconds, reason, m_sceneLabel.c_str());
        LogInfo(buf);
    }

    void PerfCapture::record(const FrameCounts& counts) {
        const Profiler::Snapshot& snap = Profiler::GetSnapshot();

        // EndFrame() publishes the snapshot, so the value we read at the
        // top of onUpdate belongs to the *previous* frame. Skip a repeat
        // index rather than writing the same frame twice.
        if (snap.frameIndex == m_lastFrameIndex) return;
        m_lastFrameIndex = snap.frameIndex;

        if (m_samples.empty()) {
            m_difficultyAtStart = counts.difficulty ? counts.difficulty : "?";
        }

        PerfSample s{};
        s.frame = snap.frameIndex;
        s.t     = m_captureClock;

        s.frameMs  = snap.frameMs;
        s.updateMs = sectionMs(snap, "ApplicationUpdate");
        s.renderMs = sectionMs(snap, "GameRender");
        s.gpuMs    = snap.gpu.supported ? snap.gpu.frameMs : -1.0;

        s.sceneUpdateMs  = sectionMs(snap, "SceneUpdate");
        s.physicsMs      = sectionMs(snap, "Physics");
        s.combatMs       = sectionMs(snap, "Combat");
        s.aiMs           = sectionMs(snap, "AI");
        s.particlesMs    = sectionMs(snap, "Particles");
        s.tileRenderMs   = sectionMs(snap, "TileRendering");
        s.spriteRenderMs = sectionMs(snap, "SpriteRendering");

        const Profiler::RendererStats& r = snap.renderer;
        s.drawCalls       = r.drawCalls;
        s.passes          = r.passes;
        s.submittedQuads  = r.submittedQuads;
        s.renderedQuads   = r.renderedQuads;
        s.culledSprites   = r.culledSprites;
        s.materialChanges = r.materialChanges;
        s.textureChanges  = r.textureChanges;

        s.enemies      = counts.enemies;
        s.bullets      = counts.bullets;
        s.enemyBullets = counts.enemyBullets;
        s.particles    = counts.particles;
        s.entities     = 1 + counts.enemies + counts.bullets + counts.enemyBullets;
        s.lights       = r.activeLights;

        s.rssKB = readResidentKB();

        m_samples.push_back(s);
    }

    PerfCapture::Summary PerfCapture::summarize(const PerfBudget& budget) const {
        Summary sum{};
        if (m_samples.empty()) return sum;

        sum.frames = m_samples.size();

        std::vector<double> frameTimes;
        frameTimes.reserve(m_samples.size());

        double frameSum = 0.0;
        double updateSum = 0.0;
        double renderSum = 0.0;
        double gpuSum = 0.0;
        std::size_t gpuCount = 0;

        for (const PerfSample& s : m_samples) {
            frameTimes.push_back(s.frameMs);
            frameSum  += s.frameMs;
            updateSum += s.updateMs;
            renderSum += s.renderMs;

            if (s.gpuMs >= 0.0) {
                gpuSum += s.gpuMs;
                ++gpuCount;
            }

            if (s.drawCalls      > sum.maxDrawCalls)      sum.maxDrawCalls = s.drawCalls;
            if (s.submittedQuads > sum.maxSubmittedQuads) sum.maxSubmittedQuads = s.submittedQuads;
            if (s.particles      > sum.maxParticles)      sum.maxParticles = s.particles;
            if (s.entities       > sum.maxEntities)       sum.maxEntities = s.entities;
            if (s.rssKB          > sum.maxRssKB)          sum.maxRssKB = s.rssKB;

            if (s.frameMs > budget.frameMs) ++sum.framesOverBudget;
        }

        const double n = static_cast<double>(sum.frames);
        sum.durationSec = m_samples.back().t;
        sum.frameAvgMs  = frameSum / n;
        sum.avgFps      = (sum.frameAvgMs > 0.0) ? (1000.0 / sum.frameAvgMs) : 0.0;
        sum.updateAvgMs = updateSum / n;
        sum.renderAvgMs = renderSum / n;
        sum.gpuAvgMs    = (gpuCount > 0) ? (gpuSum / static_cast<double>(gpuCount)) : -1.0;

        std::sort(frameTimes.begin(), frameTimes.end());
        sum.frameMinMs = frameTimes.front();
        sum.frameMaxMs = frameTimes.back();
        sum.frameP50Ms = percentile(frameTimes, 50.0);
        sum.frameP95Ms = percentile(frameTimes, 95.0);
        sum.frameP99Ms = percentile(frameTimes, 99.0);

        sum.overBudgetPct = 100.0 * static_cast<double>(sum.framesOverBudget) / n;

        sum.pass = (sum.frameP95Ms       <= budget.frameMs)
                && (sum.maxDrawCalls     <= budget.drawCalls)
                && (sum.maxSubmittedQuads <= budget.submittedQuads)
                && (sum.maxParticles     <= budget.particles);

        return sum;
    }

    void PerfCapture::stopAndWrite() {
        m_state = State::Done;

        if (m_samples.empty()) {
            LogWarn("[PerfCapture] nothing recorded — the capture ended before a "
                    "completed frame was published.");
            if (m_req.quitWhenDone) m_quitRequested = true;
            return;
        }

        const std::string csvPath = buildOutputPath();

        std::string metaPath = csvPath;
        const std::size_t dot = metaPath.find_last_of('.');
        const std::size_t slash = metaPath.find_last_of("/\\");
        if (dot != std::string::npos && (slash == std::string::npos || dot > slash)) {
            metaPath.erase(dot);
        }
        metaPath += ".meta.txt";

        std::error_code ec;
        const std::filesystem::path dir = std::filesystem::path(csvPath).parent_path();
        if (!dir.empty()) {
            std::filesystem::create_directories(dir, ec);
            if (ec) {
                LogError("[PerfCapture] could not create " + dir.string()
                         + ": " + ec.message());
            }
        }

        const PerfBudget b = budget();
        const Summary sum = summarize(b);

        if (writeCsv(csvPath)) {
            m_lastCsvPath = csvPath;
            char buf[512];
            std::snprintf(buf, sizeof(buf), "[PerfCapture] wrote %zu rows -> %s",
                          m_samples.size(), csvPath.c_str());
            LogInfo(buf);
        } else {
            LogError("[PerfCapture] FAILED to write " + csvPath);
        }

        if (writeMeta(metaPath, sum)) {
            LogInfo("[PerfCapture] wrote summary -> " + metaPath);
        }

        logSummary(sum);

        if (m_req.quitWhenDone) m_quitRequested = true;
    }

    std::string PerfCapture::buildOutputPath() const {
        if (!m_req.outPath.empty()) return m_req.outPath;

        char name[320];
        std::snprintf(name, sizeof(name), "captures/megax_perf_%s_%s_%s.csv",
                      buildConfigName(),
                      sanitizeForFilename(m_req.label).c_str(),
                      timestampNow().c_str());
        return AssetPaths::ResolveUser(name);
    }

    bool PerfCapture::writeCsv(const std::string& path) const {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return false;

        std::fprintf(f, "%s\n", kCsvHeader);

        for (const PerfSample& s : m_samples) {
            std::fprintf(f,
                "%llu,%.4f,"
                "%.4f,%.4f,%.4f,%.4f,"
                "%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,"
                "%d,%d,%d,%d,%d,%d,%d,"
                "%d,%d,%d,%d,%d,%d,%ld\n",
                static_cast<unsigned long long>(s.frame), s.t,
                s.frameMs, s.updateMs, s.renderMs, s.gpuMs,
                s.sceneUpdateMs, s.physicsMs, s.combatMs, s.aiMs,
                s.particlesMs, s.tileRenderMs, s.spriteRenderMs,
                s.drawCalls, s.passes, s.submittedQuads, s.renderedQuads,
                s.culledSprites, s.materialChanges, s.textureChanges,
                s.entities, s.enemies, s.bullets, s.enemyBullets,
                s.particles, s.lights, s.rssKB);
        }

        std::fclose(f);
        return true;
    }

    bool PerfCapture::writeMeta(const std::string& path, const Summary& sum) const {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return false;

        const PerfBudget b = budget();

        std::fprintf(f, "MegaX performance capture\n");
        std::fprintf(f, "=========================\n\n");
        std::fprintf(f, "started by      : %s\n", m_startedBy.c_str());
        std::fprintf(f, "build config    : %s\n", buildConfigName());
        std::fprintf(f, "megax scopes    : %s\n",
                     HBE_PROFILE_ENABLED ? "on" : "off (HBE_PROFILE_ENABLED=0)");
        std::fprintf(f, "vsync           : %s\n", m_req.disableVsync ? "off" : "on");
        std::fprintf(f, "scene           : %s\n", m_sceneLabel.c_str());
        std::fprintf(f, "difficulty      : %s\n", m_difficultyAtStart.c_str());
        std::fprintf(f, "label           : %s\n",
                     m_req.label.empty() ? "(none)" : m_req.label.c_str());
        std::fprintf(f, "timestamp       : %s\n", timestampNow().c_str());
        std::fprintf(f, "frames          : %zu over %.2f s (avg %.1f FPS)\n\n",
                     sum.frames, sum.durationSec, sum.avgFps);

        std::fprintf(f, "budget          : %s (frame %.2f ms, update %.2f, render %.2f, gpu %.2f)\n",
                     b.name, b.frameMs, b.updateMs, b.renderMs, b.gpuMs);
        std::fprintf(f, "                  drawCalls <= %d, submittedQuads <= %d, particles <= %d\n\n",
                     b.drawCalls, b.submittedQuads, b.particles);

        std::fprintf(f, "frame ms        : avg %.3f  min %.3f  max %.3f\n",
                     sum.frameAvgMs, sum.frameMinMs, sum.frameMaxMs);
        std::fprintf(f, "                  p50 %.3f  p95 %.3f  p99 %.3f\n",
                     sum.frameP50Ms, sum.frameP95Ms, sum.frameP99Ms);
        std::fprintf(f, "update ms       : avg %.3f  (budget %.2f)\n",
                     sum.updateAvgMs, b.updateMs);
        std::fprintf(f, "render ms       : avg %.3f  (budget %.2f)\n",
                     sum.renderAvgMs, b.renderMs);
        if (sum.gpuAvgMs >= 0.0) {
            std::fprintf(f, "gpu ms          : avg %.3f  (budget %.2f)\n",
                         sum.gpuAvgMs, b.gpuMs);
        } else {
            std::fprintf(f, "gpu ms          : unsupported on this driver\n");
        }
        std::fprintf(f, "peak counts     : drawCalls %d  submittedQuads %d  particles %d  entities %d\n",
                     sum.maxDrawCalls, sum.maxSubmittedQuads,
                     sum.maxParticles, sum.maxEntities);
        std::fprintf(f, "peak rss        : %ld KiB\n", sum.maxRssKB);
        std::fprintf(f, "over budget     : %zu frames (%.1f %%)\n",
                     sum.framesOverBudget, sum.overBudgetPct);
        std::fprintf(f, "\nverdict         : %s\n", sum.pass ? "PASS" : "OVER BUDGET");

        std::fclose(f);
        return true;
    }

    void PerfCapture::logSummary(const Summary& sum) const {
        const PerfBudget b = budget();
        char line[320];

        LogInfo("========== [PerfCapture summary] ==========");

        std::snprintf(line, sizeof(line),
            "frames %zu over %.2f s  (avg %.1f FPS, budget %s)",
            sum.frames, sum.durationSec, sum.avgFps, b.name);
        LogInfo(line);

        std::snprintf(line, sizeof(line),
            "frame ms  avg %.3f  p50 %.3f  p95 %.3f  p99 %.3f  max %.3f",
            sum.frameAvgMs, sum.frameP50Ms, sum.frameP95Ms,
            sum.frameP99Ms, sum.frameMaxMs);
        LogInfo(line);

        if (sum.gpuAvgMs >= 0.0) {
            std::snprintf(line, sizeof(line),
                "update avg %.3f  render avg %.3f  gpu avg %.3f",
                sum.updateAvgMs, sum.renderAvgMs, sum.gpuAvgMs);
        } else {
            std::snprintf(line, sizeof(line),
                "update avg %.3f  render avg %.3f  gpu unsupported",
                sum.updateAvgMs, sum.renderAvgMs);
        }
        LogInfo(line);

        std::snprintf(line, sizeof(line),
            "peak draws %d  quads %d  particles %d  entities %d  rss %ld KiB",
            sum.maxDrawCalls, sum.maxSubmittedQuads, sum.maxParticles,
            sum.maxEntities, sum.maxRssKB);
        LogInfo(line);

        std::snprintf(line, sizeof(line),
            "over budget %zu frames (%.1f %%)  ->  %s",
            sum.framesOverBudget, sum.overBudgetPct,
            sum.pass ? "PASS" : "OVER BUDGET");
        LogInfo(line);

        LogInfo("===========================================");
    }
}
```

> **PASTE NOTE.** The last line of the file is the single
> `}` that closes `namespace MegaX`, followed by one
> newline. The `namespace { ... }` opened in block one is
> closed at the end of block one — if your editor's brace
> matching disagrees, re-check that block one ends with the
> lone `    }` on its own line.

---

## 2. Register the source in `MegaX/CMakeLists.txt`

Open `/home/atulo/Projects/HBE/MegaX/CMakeLists.txt`.

The source list inside `add_executable(MegaX ...)` keeps
`src/main.cpp` first and is alphabetical after that. Insert
between `src/Game/GameLayer.cpp` (line 9) and
`src/Game/Player.cpp` (line 10):

```cmake
    src/Game/PerfCapture.cpp
```

The final block should read:

```cmake
# MegaX — shoot-em-up built on HBE
add_executable(MegaX
    src/main.cpp
    src/Game/Bullet.cpp
    src/Game/Effects.cpp
    src/Game/Enemy.cpp
    src/Game/EnemyBullet.cpp
    src/Game/EnemyManager.cpp
    src/Game/GameLayer.cpp
    src/Game/PerfCapture.cpp
    src/Game/Player.cpp
    src/World/World.cpp
)
```

`PerfCapture.h` needs no entry — headers resolve through
`target_include_directories`, which already adds
`${CMAKE_CURRENT_SOURCE_DIR}/include`.

---

## 3. Add the `MEGAX_PROFILE_IN_RELEASE` option

Still in `/home/atulo/Projects/HBE/MegaX/CMakeLists.txt`.

The `target_include_directories` block currently reads
(lines 14-16):

```cmake
target_include_directories(MegaX PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
```

Insert **right after** the closing `)` of that block (line
16, which is now line 17 after the source-list insertion in
§2) and **before** the blank line that precedes
`target_link_libraries(MegaX PRIVATE` :

```cmake

# Item 15: HBE_PROFILE_SCOPE compiles to ((void)0) when NDEBUG is set, so a
# Release capture normally has empty per-section columns. Turning this on
# keeps MegaX's own scopes (SceneUpdate, Physics, Combat, AI, Particles,
# GameRender, TileRendering, SpriteRendering) alive in Release. It cannot
# revive HBE.Core's own scopes (ApplicationUpdate, Audio) — those are
# compiled out inside the engine library.
option(MEGAX_PROFILE_IN_RELEASE
    "Keep MegaX's HBE_PROFILE_SCOPE timers alive in Release builds" OFF)

if(MEGAX_PROFILE_IN_RELEASE)
    target_compile_definitions(MegaX PRIVATE HBE_PROFILE_ENABLED=1)
endif()
```

Final region should read:

```cmake
target_include_directories(MegaX PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Item 15: HBE_PROFILE_SCOPE compiles to ((void)0) when NDEBUG is set, so a
# Release capture normally has empty per-section columns. Turning this on
# keeps MegaX's own scopes (SceneUpdate, Physics, Combat, AI, Particles,
# GameRender, TileRendering, SpriteRendering) alive in Release. It cannot
# revive HBE.Core's own scopes (ApplicationUpdate, Audio) — those are
# compiled out inside the engine library.
option(MEGAX_PROFILE_IN_RELEASE
    "Keep MegaX's HBE_PROFILE_SCOPE timers alive in Release builds" OFF)

if(MEGAX_PROFILE_IN_RELEASE)
    target_compile_definitions(MegaX PRIVATE HBE_PROFILE_ENABLED=1)
endif()

target_link_libraries(MegaX PRIVATE
    HBE.Core
    HBE.Platform.SDL
    HBE.Renderer.GL
    hbe::sdl3
    hbe::opengl
)
```

Nothing else in `MegaX/CMakeLists.txt` changes — the
`hbe::sdl3_mixer` block, `hbe_copy_runtime_deps`, the
`FOLDER` property and the `WIN32` block all stay as they
are.

> **WHY THIS WORKS.** `Profiler.h` wraps its default in
> `#ifndef HBE_PROFILE_ENABLED`, so a `-D` from the command
> line wins. Because the definition is `PRIVATE` to the
> `MegaX` target, it changes only MegaX's translation units;
> `HBE.Core` still compiles `ApplicationUpdate` and `Audio`
> out of a Release build. `Profiler::BeginScope` /
> `EndScope` are compiled into `HBE.Core` unconditionally, so
> MegaX's revived scopes link and work.

The option is **OFF by default**, so a plain
`cmake --preset linux-clang` behaves exactly as it did
before this item. Doc `06` §5 covers when to turn it on.

---

## 4. What NOT to touch

* Do **not** call `Profiler::Reset()` when a capture starts.
  It would wipe the 120-frame rolling window that item 13's
  1-Hz log block and the `F8` dump both rely on, and the
  capture reads `currentMs` per frame anyway — it does not
  need the rolling stats.
* Do **not** flush the CSV per frame. Opening the file once
  at `stopAndWrite()` is the reason the per-frame cost is a
  `push_back`; an `fflush` inside `record()` would put a
  syscall in the measured window.
* Do **not** add `HBE_PROFILE_SCOPE` inside `PerfCapture`.
  Measuring the measurement is a fine idea in general and a
  bad one here — the scope itself costs more than the
  `push_back` it would wrap.
* Do **not** widen `MEGAX_PROFILE_IN_RELEASE` into
  `target_compile_definitions(... PUBLIC ...)` or set it on
  an `HBE.*` target. Both would drag engine code into it,
  which is out of scope for a `[MEGAX]` item.
* Do **not** delete the `visibleTileChunks` /
  `postProcessPasses` fields from anything — they are not
  referenced by this item at all, and the reason is in
  `00_overview.md` §6.

---

## 5. Sanity check before doc 03

```fish
test -f /home/atulo/Projects/HBE/MegaX/src/Game/PerfCapture.cpp; echo $status
# -> 0

grep -c "PerfCapture.cpp" /home/atulo/Projects/HBE/MegaX/CMakeLists.txt
# -> 1

grep -c "MEGAX_PROFILE_IN_RELEASE" /home/atulo/Projects/HBE/MegaX/CMakeLists.txt
# -> 2   (the option() and the if())

grep -c "sectionMs(snap" /home/atulo/Projects/HBE/MegaX/src/Game/PerfCapture.cpp
# -> 9   (ApplicationUpdate, GameRender, SceneUpdate, Physics,
#         Combat, AI, Particles, TileRendering, SpriteRendering)
```

The project **should** build at this point — nothing calls
the new code yet, but it compiles and links on its own:

```fish
cd /home/atulo/Projects/HBE
cmake --build --preset linux-clang-debug --target MegaX
```

Expected: `PerfCapture.cpp.o` compiles, `MegaX` links, no
new warnings.

If it fails with
``fatal error: 'Game/PerfCapture.h' file not found``, the
header from doc `01` was not saved to
`MegaX/include/Game/PerfCapture.h`.

If it fails with
``error: use of undeclared identifier 'sysconf'``, the
`#include <unistd.h>` block at the top of §1.1 was dropped
or its `#if defined(__linux__)` guard was mistyped.

If `ninja` reports
``'src/Game/PerfCapture.cpp', needed by ..., missing``, the
CMake entry in §2 landed but the file did not — check the
path spelling.

Next: `03_gamelayer_wiring.md`.
