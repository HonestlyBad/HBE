//
// Created by atulo on 8/6/26.
//

#include "HBE/Renderer/GpuTimer.h"
#include "HBE/Core/Log.h"
#include "HBE/Core/Profiler.h"

#include <glad/glad.h>

#include <array>
#include <cstdlib>
#include <cstring>

namespace HBE::Renderer::GpuTimer
{
    namespace
    {
        constexpr std::size_t kRingSlots = kResultLatencyFrames + 1;

        struct Section
        {
            const char* name = nullptr;

            std::array<unsigned int, kRingSlots> startIds{};
            std::array<unsigned int, kRingSlots> endIds{};
            std::array<bool, kRingSlots> slotInFlight{};
            std::array<int, kRingSlots> slotDepth{};
        };

        struct StackEntry
        {
            int sectionIndex = -1;
            std::size_t writeSlot = 0;
        };

        struct State
        {
            bool initialized = false;
            bool supported = false;
            std::uint64_t frameIndex = 0;
            std::size_t writeSlot = 0;

            std::array<Section, kMaxGpuSections> sections{};
            std::size_t sectionCount = 0;

            std::array<StackEntry, kMaxGpuSections> stack{};
            std::size_t stackSize = 0;

            bool warnedStall = false;
        };

        State& S()
        {
            static State s;
            return s;
        }

        bool checkForceDisable()
        {
            const char* env = std::getenv("HBE_FORCE_NO_GPU_TIMER");
            return env && std::strcmp(env, "1") == 0;
        }

        int findOrAddSection(const char* name)
        {
            State& s = S();
            for (std::size_t i = 0; i < s.sectionCount; ++i)
            {
                if (s.sections[i].name == name) return static_cast<int>(i);
            }
            if (s.sectionCount >= kMaxGpuSections) return -1;

            Section& sec = s.sections[s.sectionCount];
            sec = Section{};
            sec.name = name;

            glGenQueries(static_cast<GLsizei>(kRingSlots), sec.startIds.data());
            glGenQueries(static_cast<GLsizei>(kRingSlots), sec.endIds.data());
            for (std::size_t i = 0; i < kRingSlots; ++i)
            {
                sec.slotInFlight[i] = false;
                sec.slotDepth[i] =0;
            }

            const int idx = static_cast<int>(s.sectionCount);
            ++s.sectionCount;
            return idx;
        }

        void drainReadySlot(Section& sec, std::size_t slot)
        {
            if (!sec.slotInFlight[slot]) return;
            GLint available = 0;
            glGetQueryObjectiv(sec.endIds[slot], GL_QUERY_RESULT_AVAILABLE, &available);
            if (!available) return;

            GLuint64 startNs = 0;
            GLuint64 endNs = 0;
            glGetQueryObjectui64v(sec.startIds[slot], GL_QUERY_RESULT, &startNs);
            glGetQueryObjectui64v(sec.endIds[slot], GL_QUERY_RESULT, &endNs);
            sec.slotInFlight[slot] = false;

            const std::uint64_t ns = (endNs >= startNs) ? (endNs - startNs) : 0;

            HBE::Core::Profiler::PublishGpuSection(sec.name, ns, sec.slotDepth[slot]);
        }
    }

    bool Initialize()
    {
        State& s = S();
        if (s.initialized) return s.supported;

        s.initialized = true;

        if (checkForceDisable())
        {
            HBE::Core::LogInfo("GpuTimer: disabled by HBE_FORCE_NO_GPU_TIMER=1.");
            s.supported = false;
            HBE::Core::Profiler::SetGpuSupported(false);
            return false;
        }

        const bool haveExt = (GLAD_GL_ARB_timer_query != 0) || (GLAD_GL_VERSION_3_3 != 0);
        const bool haveFuncs =
            (glGenQueries != nullptr) &&
            (glDeleteQueries != nullptr) &&
            (glQueryCounter != nullptr) &&
            (glGetQueryObjectiv != nullptr) &&
            (glGetQueryObjectui64v != nullptr);

        s.supported = haveExt && haveFuncs;

        if (!s.supported)
        {
            HBE::Core::LogWarn("GpuTimer: ARB_timer_query is not available. GPU timings disabled.");
        }

        HBE::Core::Profiler::SetGpuSupported(s.supported);
        return s.supported;
    }

    void Shutdown()
    {
        State& s = S();
        if (!s.supported)
        {
            s = State{};
            return;
        }

        for (std::size_t i = 0; i < s.sectionCount; ++i)
        {
            Section& sec = s.sections[i];
            glDeleteQueries(static_cast<GLsizei>(kRingSlots), sec.startIds.data());
            glDeleteQueries(static_cast<GLsizei>(kRingSlots), sec.endIds.data());
        }
        s = State{};
    }

    bool IsSupported() { return S().supported;}

    void NewFrame()
    {
        State& s = S();
        if (!s.supported) return;

        ++s.frameIndex;
        s.writeSlot = static_cast<std::size_t>(s.frameIndex % kRingSlots);
        s.stackSize = 0;

        const std::size_t readSlot = s.writeSlot;
        for (std::size_t i = 0; i < s.sectionCount; ++i)
        {
            drainReadySlot(s.sections[i], readSlot);
        }
    }

    int BeginScope(const char* name)
    {
        State& s = S();
        if (!s.supported || name == nullptr) return -1;
        if (s.stackSize >= s.stack.size()) return -1;

        const int sectionIdx = findOrAddSection(name);
        if (sectionIdx < 0) return -1;

        Section& sec = s.sections[sectionIdx];
        const std::size_t slot = s.writeSlot;

        if (sec.slotInFlight[slot])
        {
            if (!s.warnedStall)
            {
                s.warnedStall = true;
                HBE::Core::LogWarn("GpuTimer: query result still unavailabe after a full ring; sample dropped. Raise kResultLatencyFrames if this repeats every frame.");
            }
            return -1;
        }

        glQueryCounter(sec.startIds[slot], GL_TIMESTAMP);
        sec.slotDepth[slot] = static_cast<int>(s.stackSize);

        StackEntry& e = s.stack[s.stackSize];
        e.sectionIndex = sectionIdx;
        e.writeSlot = slot;
        ++s.stackSize;

        return static_cast<int>(s.stackSize) - 1;
    }

    void EndScope(int handle)
    {
        State& s = S();
        if (handle < 0 || !s.supported) return;
        if (handle >= static_cast<int>(s.stackSize)) return;

        while (static_cast<int>(s.stackSize) > handle)
        {
            StackEntry& e = s.stack[s.stackSize - 1];
            Section& sec = s.sections[e.sectionIndex];

            glQueryCounter(sec.endIds[e.writeSlot], GL_TIMESTAMP);
            sec.slotInFlight[e.writeSlot] = true;

            --s.stackSize;
        }
    }
}
