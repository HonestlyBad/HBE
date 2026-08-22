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

namespace MegaX
{
    namespace
    {
        const char* kCsvHeader =
            "frame,t,frameMs,updateMs,renderMs,gpuMs,"
            "sceneUpdateMs,physicsMs,combatMs,aiMs,particlesMs,"
            "tileRenderMs,spriteRenderMs,"
            "drawCalls,passes,submittedQuads,renderedQuads,culledSprites,"
            "materialChanges,textureChanges,"
            "entities,enemies,bullets,enemyBullets,particles,lights,rssKB";

        double sectionMs(const Profiler::Snapshot& snap, const char* name)
        {
            for (const auto& s : snap.sections)
            {
                if (s.name && std::strcmp(s.name, name) == 0)
                {
                    return s.currentMs;
                }
            }
            return 0.0;
        }

        long readResidentKB()
        {
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

        double percentile(const std::vector<double>& sorted, double pct)
        {
            if (sorted.empty()) return 0.0;
            const double rank = (pct / 100.0) * static_cast<double>(sorted.size());
            std::size_t idx = static_cast<std::size_t>(rank);
            if (idx >= sorted.size()) idx = sorted.size() - 1;
            return sorted[idx];
        }

        std::string sanitizeForFilename(const std::string& in)
        {
            std::string out = in.empty() ? std::string("run") : in;
            for (char& c : out)
            {
                const bool ok = (c >= 'a' && c <= 'z')
                            || (c >= 'A' && c <= 'Z')
                            || (c >= '0' && c <= '9')
                            || c == '_' || c == '-';
                if (!ok) c = '-';
            }
            return out;
        }

        std::string timestampNow()
        {
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

        const char* buildConfigName()
        {
#if defined(NDEBUG)
            return "release";
#else
            return "debug";
#endif
        }

        bool matchOption(int argc, char** argv, int& i, const char* flag, std::string& outValue)
        {
            const char* arg = argv[i];
            const std::size_t flagLen = std::strlen(flag);

            if (std::strncmp(arg, flag, flagLen) != 0) return false;

            if (arg[flagLen] == '=')
            {
                outValue = std::string(arg + flagLen + 1);
                return true;
            }
            if (arg[flagLen] != '\0') return false;

            if (i + 1 >= argc)
            {
                LogWarn(std::string("[PerfCapture] ") + flag + " needs a value - ignored.");
                return false;
            }
            outValue = std::string(argv[++i]);
            return true;
        }

        bool parsePositiveDouble(const std::string& text, const char* flag, double& out)
        {
            char* end = nullptr;
            const double v = std::strtod(text.c_str(), &end);
            if (end == text.c_str() || v <= 0.0)
            {
                LogWarn(std::string("[PerfCapture] bad value for ") + flag + " ('" + text + "') - keeping the default.");
                return false;
            }
            out = v;
            return true;
        }
    }

    PerfBudget MakeBudget60()
    {
        PerfBudget b{};
        b.name = "60fps";
        b.frameMs = 16.67;
        b.updateMs = 6.00;
        b.renderMs = 4.00;
        b.gpuMs = 8.00;
        b.drawCalls = 64;
        b.submittedQuads = 4000;
        b.particles = 1500;
        return b;
    }

    PerfBudget MakeBudget120()
    {
        PerfBudget b{};
        b.name = "120fps";
        b.frameMs = 8.33;
        b.updateMs = 3.00;
        b.renderMs = 2.00;
        b.gpuMs = 4.00;
        b.drawCalls = 48;
        b.submittedQuads = 3000;
        b.particles = 1000;
        return b;
    }

    void PrintCaptureUsage()
    {
        LogInfo("MegaX performance capture options:");
        LogInfo("  --capture                start a capture automatically after the warmup");
        LogInfo("  --capture-seconds <N>    how long to record (default 10)");
        LogInfo("  --capture-warmup <N>     seconds to run before recording (default 3)");
        LogInfo("  --capture-label <text>   label folded into the output filename");
        LogInfo("  --capture-out <path>     explicit .csv path (default: user data captures/)");
        LogInfo("  --capture-120            score the run against the 120 FPS budget");
        LogInfo("  --capture-quit           quit once the capture is written.");
        LogInfo("  --no-vsync               run uncapped so the capture measures real headroom");
        LogInfo("  --fixed-hz <N>           gameplay simulation rate (default 60)");
        LogInfo("  --fixed-catchup <N>      max simulation steps per rendered frame (default 5)");
        LogInfo("  --render-hz <N>          cap the render rate (default uncapped)");
        LogInfo("  --help                   print this list and exit");
        LogInfo("In-game, F9 starts/stops a capture with the same settings.");
    }

    PerfCaptureRequest ParseCaptureArgs(int argc, char** argv)
    {
        PerfCaptureRequest req{};
        if (argv == nullptr) return req;

        for (int i = 1; i < argc; ++i)
        {
            const char* arg = argv[i];
            if (arg == nullptr) continue;

            std::string value;

            if (std::strcmp(arg, "--capture") == 0)
            {
                req.enabled = true;
                continue;
            }
            if (std::strcmp(arg,"--capture-quit") == 0)
            {
                req.quitWhenDone = true;
                req.enabled = true;
                continue;
            }
            if (std::strcmp(arg, "--capture-120") == 0)
            {
                req.useBudget120 = true;
                continue;
            }
            if (std::strcmp(arg, "--no-vsync") == 0)
            {
                req.disableVsync = true;
                continue;
            }
            if (matchOption(argc, argv, i, "--capture-seconds", value))
            {
                if (parsePositiveDouble(value, "--capture-seconds", req.durationSeconds))
                {
                    req.enabled = true;
                }
                continue;
            }
            if (matchOption(argc, argv, i, "--capture-warmup", value))
            {
                parsePositiveDouble(value, "--capture-warmup", req.warmupSeconds);
                continue;
            }
            if (matchOption(argc, argv, i, "--capture-label", value))
            {
                req.label = value;
                continue;
            }
            if (matchOption(argc, argv, i, "--capture-out", value))
            {
                req.outPath = value;
                continue;
            }
            if (std::strncmp(arg, "--capture", 9) == 0)
            {
                LogWarn(std::string("[PerfCapture] unknown option '") + arg + "' - ignored.");
            }
        }
        return req;
    }

    void PerfCapture::configure(const PerfCaptureRequest& req)
    {
        m_req = req;
        if (m_req.durationSeconds <= 0.0) m_req.durationSeconds = 10.0;
        if (m_req.warmupSeconds < 0.0) m_req.warmupSeconds = 0.0;

        if (m_req.enabled)
        {
            m_state = State::Warmup;
            m_warmupClock = 0.0;

            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "[PerfCapture] armed: %.1f s warmup, then %.1f s of capture (%s budget).",
                m_req.warmupSeconds,m_req.durationSeconds,
                m_req.useBudget120 ? "120fps" : "60fps");
            LogInfo(buf);
        }
    }

    void PerfCapture::setSceneLabel(const std::string& scenePath)
    {
        m_sceneLabel = scenePath.empty() ? std::string("unknown") : scenePath;
    }

    void PerfCapture::tick(float dt, const FrameCounts& counts)
    {
        const double d = (dt > 0.0f) ? static_cast<double>(dt) : 0.0;

        if (m_state == State::Warmup)
        {
            m_warmupClock += d;
            if (m_warmupClock >= m_req.warmupSeconds)
            {
                m_startedBy = "command line";
                start("command line");
            }
            return;
        }
        if (m_state != State::Recording) return;

        record(counts);
        m_captureClock += d;

        if (m_captureClock >= m_req.durationSeconds)
        {
            stopAndWrite();
            return;
        }
        if (m_samples.size() >= kMaxSamples)
        {
            LogWarn("[PerfCapture] sample cap reached - stopping early.");
            stopAndWrite();
        }
    }

    void PerfCapture::toggle()
    {
        if (m_state == State::Recording)
        {
            LogInfo("[PerfCapture] stopped early (F9).");
            stopAndWrite();
            return;
        }
        m_startedBy = "F9";
        start("F9");
    }

    void PerfCapture::start(const char* reason)
    {
        m_state = State::Recording;
        m_captureClock = 0.0;
        m_lastFrameIndex = 0;
        m_samples.clear();

        std::size_t want = static_cast<std::size_t>(m_req.durationSeconds * 1000.0) + 512;
        if (want > kMaxSamples) want = kMaxSamples;
        m_samples.reserve(want);

        char buf[320];
        std::snprintf(buf, sizeof(buf),
            "[PerfCapture] recording %.1f s (%s) - scene '%s'.",
            m_req.durationSeconds, reason, m_sceneLabel.c_str());
        LogInfo(buf);
    }

    void PerfCapture::record(const FrameCounts& counts)
    {
        const Profiler::Snapshot& snap = Profiler::GetSnapshot();

        if (snap.frameIndex == m_lastFrameIndex) return;
        m_lastFrameIndex = snap.frameIndex;

        if (m_samples.empty())
        {
            m_difficultyAtStart = counts.difficulty ? counts.difficulty : "?";
        }

        PerfSample s{};
        s.frame = snap.frameIndex;
        s.t = m_captureClock;

        s.frameMs = snap.frameMs;
        s.updateMs = sectionMs(snap, "ApplicationUpdate");
        s.renderMs = sectionMs(snap, "GameRender");
        s.gpuMs = snap.gpu.supported ? snap.gpu.frameMs : -1.0;

        s.sceneUpdateMs = sectionMs(snap, "SceneUpdate");
        s.physicsMs = sectionMs(snap, "Physics");
        s.combatMs = sectionMs(snap, "Combat");
        s.aiMs = sectionMs(snap, "AI");
        s.particlesMs = sectionMs(snap, "Particles");
        s.tileRenderMs = sectionMs(snap, "TileRendering");
        s.spriteRenderMs = sectionMs(snap, "SpriteRendering");

        const Profiler::RendererStats& r = snap.renderer;
        s.drawCalls = r.drawCalls;
        s.passes = r.passes;
        s.submittedQuads = r.submittedQuads;
        s.renderedQuads = r.renderedQuads;
        s.culledSprites = r.culledSprites;
        s.materialChanges = r.materialChanges;
        s.textureChanges = r.textureChanges;

        s.enemies = counts.enemies;
        s.bullets = counts.bullets;
        s.enemyBullets = counts.enemyBullets;
        s.particles = counts.particles;
        s.entities = 1 + counts.enemies + counts.bullets + counts.enemyBullets;
        s.lights = r.activeLights;

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

    void PerfCapture::stopAndWrite()
    {
        m_state = State::Done;

        if (m_samples.empty())
        {
            LogWarn("[PerfCapture] nothing recorded - the capture ended before a "
                    "completed frame was published!");
            if (m_req.quitWhenDone) m_quitRequested = true;
            return;
        }

        const std::string csvPath = buildOutputPath();

        std::string metaPath = csvPath;
        const std::size_t dot = metaPath.find_last_of('.');
        const std::size_t slash = metaPath.find_last_of("/\\");
        if (dot != std::string::npos && (slash == std::string::npos || dot > slash))
        {
            metaPath.erase(dot);
        }
        metaPath += ".meta.txt";

        std::error_code ec;
        const std::filesystem::path dir = std::filesystem::path(csvPath).parent_path();
        if (!dir.empty())
        {
            std::filesystem::create_directories(dir, ec);
            if (ec)
            {
                LogError("[PerfCapture] could not create " + dir.string() + ": " + ec.message());
            }
        }

        const PerfBudget b = budget();
        const Summary sum = summarize(b);

        if (writeCsv(csvPath))
        {
            m_lastCsvPath = csvPath;
            char buf[512];
            std::snprintf(buf, sizeof(buf), "[PerfCapture] wrote %zu rows -> %s", m_samples.size(), csvPath.c_str());
            LogInfo(buf);
        }else
        {
            LogError("[PerfCapture] FAILED to write " + csvPath);
        }

        if (writeMeta(metaPath, sum))
        {
            LogInfo("[PerfCapture] wrote summary -> " + metaPath);
        }

        logSummary(sum);

        if (m_req.quitWhenDone) m_quitRequested = true;
    }

    std::string PerfCapture::buildOutputPath() const
    {
        if (!m_req.outPath.empty()) return m_req.outPath;

        char name[320];
        std::snprintf(name, sizeof(name), "captures/megax_perf_%s_%s_%s.csv", buildConfigName(), sanitizeForFilename(m_req.label).c_str(), timestampNow().c_str());
        return AssetPaths::ResolveUser(name);
    }

    bool PerfCapture::writeCsv(const std::string& path) const
    {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return false;

        std::fprintf(f, "%s\n", kCsvHeader);

        for (const PerfSample& s : m_samples)
        {
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

    bool PerfCapture::writeMeta(const std::string& path, const Summary& sum) const
    {
        std::FILE* f = std::fopen(path.c_str(), "wb");
        if (!f) return false;

        const PerfBudget b = budget();
        std::fprintf(f, "MegaX performance capture\n");
        std::fprintf(f, "========================\n\n");
        std::fprintf(f, "started by     : %s\n", m_startedBy.c_str());
        std::fprintf(f, "build config   : %s\n", buildConfigName());
        std::fprintf(f, "megax scopes   : %s\n", HBE_PROFILE_ENABLED ? "on" : "off (HBE_PROFILE_ENABLED=0)");
        std::fprintf(f, "vsync          : %s\n", m_req.disableVsync ? "off" : "on");
        std::fprintf(f, "scene          : %s\n", m_sceneLabel.c_str());
        std::fprintf(f, "difficulty     : %s\n", m_difficultyAtStart.c_str());
        std::fprintf(f, "label          : %s\n", m_req.label.empty() ? "(none)" : m_req.label.c_str());
        std::fprintf(f, "timestamp      : %s\n", timestampNow().c_str());
        std::fprintf(f, "frames         : %zu over %.2f s (avg %.1f FPS)\n\n", sum.frames, sum.durationSec, sum.avgFps);
        std::fprintf(f, "budget         : %s (frame %.2f ms, update %.2f, render %.2f, gpu %.2f)\n", b.name, b.frameMs, b.updateMs, b.renderMs, b.gpuMs);
        std::fprintf(f, "                 drawCalls <= %d, submittedQuads <= %d, particles <= %d\n\n", b.drawCalls, b.submittedQuads, b.particles);
        std::fprintf(f, "frame ms       : avg %.3f min %.3f max %.3f\n", sum.frameAvgMs, sum.frameMinMs, sum.frameMaxMs);
        std::fprintf(f, "                 p50 %.3f p95 %.3f p99 %.3f\n", sum.frameP50Ms, sum.frameP95Ms, sum.frameP99Ms);
        std::fprintf(f, "update ms      : avg %.3f (budget %.2f)\n", sum.updateAvgMs, b.updateMs);
        std::fprintf(f, "render ms      : avg %.3f (budget %.2f)\n", sum.renderAvgMs, b.renderMs);
        if (sum.gpuAvgMs >= 0.0)
        {
            std::fprintf(f, "gpu ms         : avg %.3f (budget %.2f)\n", sum.gpuAvgMs, b.gpuMs);
        }else
        {
            std::fprintf(f, "gpu ms         : unsupported on this driver\n");
        }
        std::fprintf(f, "peak counts    : drawCalls %d submittedQuads %d particles %d entities %d\n", sum.maxDrawCalls, sum.maxSubmittedQuads, sum.maxParticles, sum.maxEntities);
        std::fprintf(f, "peak rss       : %ld KiB\n", sum.maxRssKB);
        std::fprintf(f, "over budget    : %zu frames (%.1f %%)\n", sum.framesOverBudget, sum.overBudgetPct);
        std::fprintf(f, "\nverdict        : %s\n", sum.pass ? "PASS" : "OVER BUDGET");

        std::fclose(f);
        return true;
    }

    void PerfCapture::logSummary(const Summary& sum) const
    {
        const PerfBudget b = budget();
        char line[320];

        LogInfo("========== [PerfCapture summary] ==========");

        std::snprintf(line, sizeof(line), "frame %zu over %.2f s ( avg %.1f FPS, budget %s)", sum.frames, sum.durationSec, sum.avgFps, b.name);
        LogInfo(line);

        std::snprintf(line, sizeof(line), "frame ms avg %.3f p50 %.3f p95 %.3f p99 %.3f max %.3f", sum.frameAvgMs, sum.frameP50Ms, sum.frameP95Ms, sum.frameP99Ms, sum.frameMaxMs);
        LogInfo(line);

        if (sum.gpuAvgMs >= 0.0)
        {
            std::snprintf(line, sizeof(line),"update avg %.3f render avg %.3f gpu avg %.3f", sum.updateAvgMs, sum.renderAvgMs, sum.gpuAvgMs);
        }else
        {
            std::snprintf(line, sizeof(line), "update avg %.3f render avg %.3f gpu unsupported", sum.updateAvgMs, sum.renderAvgMs);
        }
        LogInfo(line);

        std::snprintf(line, sizeof(line), "peak draws %d quads %d particles %d entities %d rss %ld KiB", sum.maxDrawCalls, sum.maxSubmittedQuads, sum.maxParticles, sum.maxEntities, sum.maxRssKB);
        LogInfo(line);

        std::snprintf(line, sizeof(line), "over budget %zu frames (%.1f %%) -> %s", sum.framesOverBudget, sum.overBudgetPct, sum.pass ? "PASS" : "OVER BUDGET");
        LogInfo(line);

        LogInfo("===========================================");
    }
}
