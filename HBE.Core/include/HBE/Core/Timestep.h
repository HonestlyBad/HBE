#pragma once

namespace HBE::Core
{
    struct FixedTimestepConfig
    {
        float hz = 60.0f;
        int maxCatchUpSteps = 5;
        float maxFrameSeconds = 0.25f;
    };

    class FixedTimestep
    {
    public:
        void configure(const FixedTimestepConfig& config);

        const FixedTimestepConfig& config() const {return m_cfg;}

        float fixedDeltaSeconds() const {return m_fixedDt;}

        float alpha() const {return m_alpha;}

        int stepsLastFrame() const {return m_steps;}
        int droppedStepLastFrame() const {return m_dropped;}
        float accumulatorSeconds() const {return m_accumulator;}

        void beginFrame(float frameSeconds);

        bool consumeStep();

        void reset();

    private:
        FixedTimestepConfig m_cfg{};

        float m_fixedDt = 1.0f / 60.0f;
        float m_accumulator = 0.0f;
        float m_alpha = 0.0f;

        int m_steps = 0;
        int m_dropped = 0;
    };
}