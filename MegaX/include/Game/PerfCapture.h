#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace MegaX
{
    // per-frame budget. Two predfiened: the 60 FPS shipping target and 120 FPS aspirational target.
    struct PerfBudget
    {
        const char* name = "60fps";
        double frameMs = 16.67;
        double updateMs = 6.00;
        double renderMs = 4.00;
        double gpuMs = 8.00;
        int drawCalls = 64;
        int submittedQuads = 4000;
        int particles = 1500;
    };

    PerfBudget MakeBudget60();
    PerfBudget MakeBudget120();

    struct PerfSample
    {
        std::uint64_t frame = 0;
        double t = 0.0;

        double frameMs = 0.0;
        double updateMs = 0.0;
        double renderMs = 0.0;
        double gpuMs = -1.0; // -1 => GPU timing unsupported

        double sceneUpdateMs = 0.0;
        double physicsMs = 0.0;
        double combatMs = 0.0;
        double aiMs = 0.0;
        double particlesMs = 0.0;
        double tileRenderMs = 0.0;
        double spriteRenderMs = 0.0;

        int drawCalls = 0;
        int passes = 0;
        int submittedQuads = 0;
        int renderedQuads = 0;
        int culledSprites = 0;
        int materialChanges = 0;
        int textureChanges = 0;

        int entities = 0;
        int enemies = 0;
        int bullets = 0;
        int enemyBullets = 0;
        int particles = 0;
        int lights = 0;

        long rssKB = 0;
    };

    struct PerfCaptureRequest
    {
        bool enabled = false;
        double warmupSeconds = 3.0;
        double durationSeconds = 10.0;
        std::string label;
        std::string outPath;
        bool quitWhenDone = false;
        bool useBudget120 = false;
        bool disableVsync = false;
    };

    PerfCaptureRequest ParseCaptureArgs(int argc, char** argv);
    void PrintCaptureUsage();

    class PerfCapture
    {
    public:
        struct FrameCounts
        {
            int enemies = 0;
            int bullets = 0;
            int enemyBullets = 0;
            int particles = 0;
            const char* difficulty = "?";
        };

        struct Summary
        {
            std::size_t frames = 0;
            double durationSec = 0.0;
            double avgFps = 0.0;
            double frameAvgMs = 0.0;
            double frameMinMs = 0.0;
            double frameMaxMs = 0.0;
            double frameP50Ms = 0.0;
            double frameP95Ms = 0.0;
            double frameP99Ms = 0.0;
            double updateAvgMs = 0.0;
            double renderAvgMs = 0.0;
            double gpuAvgMs = -1.0;
            int maxDrawCalls = 0;
            int maxSubmittedQuads = 0;
            int maxParticles = 0;
            int maxEntities = 0;
            long maxRssKB = 0;
            std::size_t framesOverBudget = 0;
            double overBudgetPct = 0.0;
            bool pass = false;
        };

        static constexpr std::size_t kMaxSamples = 40000;

        void configure(const PerfCaptureRequest& req);
        void setSceneLabel(const std::string& scenePath);

        void tick(float dt, const FrameCounts& counts);

        void toggle();

        bool recording() const {return m_state == State::Recording;}
        bool shouldQuit() const {return m_quitRequested;}

        PerfBudget budget() const
        {
            return m_req.useBudget120 ? MakeBudget120() : MakeBudget60();
        }

        Summary summarize(const PerfBudget& budget) const;

        const std::string& lastCsvPath() const {return m_lastCsvPath;}

    private:
        enum class State { Idle, Warmup, Recording, Done};

        void start(const char* reason);
        void record(const FrameCounts& counts);
        void stopAndWrite();
        bool writeCsv(const std::string& path) const;
        bool writeMeta(const std::string& path, const Summary& sum) const;
        void logSummary(const Summary& sum) const;
        std::string buildOutputPath() const;

        PerfCaptureRequest m_req{};
        State m_state = State::Idle;
        double m_warmupClock = 0.0;
        double m_captureClock = 0.0;

        std::vector<PerfSample> m_samples;
        std::string m_sceneLabel = "unknown";
        std::string m_difficultyAtStart = "?";
        std::string m_startedBy = "?";
        std::string m_lastCsvPath;

        bool m_quitRequested = false;
        std::uint64_t m_lastFrameIndex = 0;
    };
}