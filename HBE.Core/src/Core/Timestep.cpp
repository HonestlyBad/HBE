#include "HBE/Core/Timestep.h"

namespace HBE::Core
{
    static constexpr float kMinHz = 1.0f;
    static constexpr float kMaxHz = 1000.0f;

    void FixedTimestep::configure(const FixedTimestepConfig& cfg)
    {
        m_cfg = cfg;

        if (!(m_cfg.hz > 0.0f)) m_cfg.hz = 60.0f;
        if (m_cfg.hz < kMinHz) m_cfg.hz = kMinHz;
        if (m_cfg.hz > kMaxHz) m_cfg.hz = kMaxHz;

        if (m_cfg.maxCatchUpSteps < 1) m_cfg.maxCatchUpSteps = 1;

        m_fixedDt = 1.0f / m_cfg.hz;

        if (!(m_cfg.maxFrameSeconds > 0.0f)) m_cfg.maxFrameSeconds = 0.25f;
        if (m_cfg.maxFrameSeconds < m_fixedDt) m_cfg.maxFrameSeconds = m_fixedDt;

        reset();
    }

    void FixedTimestep::reset()
    {
        m_accumulator = 0.0f;
        m_alpha = 0.0f;
        m_steps = 0;
        m_dropped = 0;
    }

    void FixedTimestep::beginFrame(float frameSeconds)
    {
        m_steps = 0;
        m_dropped = 0;

        if (!(frameSeconds > 0.0f)) frameSeconds = 0.0f;

        if (frameSeconds > m_cfg.maxFrameSeconds)
        {
            frameSeconds = m_cfg.maxFrameSeconds;
        }

        m_accumulator += frameSeconds;
    }

    bool FixedTimestep::consumeStep()
    {
        if (m_accumulator >= m_fixedDt && m_steps < m_cfg.maxCatchUpSteps)
        {
            m_accumulator -= m_fixedDt;
            ++m_steps;
            return true;
        }

        while (m_accumulator >= m_fixedDt)
        {
            m_accumulator -= m_fixedDt;
            ++m_dropped;
        }

        m_alpha = m_accumulator / m_fixedDt;
        if (!(m_alpha > 0.0f)) m_alpha = 0.0f;
        if (m_alpha > 1.0f) m_alpha = 1.0f;

        return false;
    }
}