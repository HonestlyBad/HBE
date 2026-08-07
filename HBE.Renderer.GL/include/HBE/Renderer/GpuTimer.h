//
// Created by atulo on 8/6/26.
//
#pragma once

#include <cstdint>
#include <cstddef>

#ifndef HONESTLYBADENGINE_GPUTIMER_H
#define HONESTLYBADENGINE_GPUTIMER_H

namespace HBE::Renderer::GpuTimer
{
    static constexpr std::size_t kResultLatencyFrames = 3;
    static constexpr std::size_t kMaxGpuSections = 32;

    bool Initialize();
    void Shutdown();

    bool IsSupported();

    void NewFrame();

    int BeginScope(const char* name);
    void EndScope(int handle);

    class Scope
    {
    public:
        explicit Scope(const char* name) noexcept : m_handle(BeginScope(name)){}
        ~Scope() noexcept { EndScope(m_handle); }

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        Scope(Scope&&) = delete;
        Scope& operator=(Scope&&) = delete;
    private:
        int m_handle;
    };
} // HBE::Renderer::GpuTimer

#define HBE_GPU_SCOPE_CONCAT_INNER(a, b) a##b
#define HBE_GPU_SCOPE_CONCAT(a, b) HBE_GPU_SCOPE_CONCAT_INNER(a, b)
#define HBE_GPU_SCOPE(NAME) ::HBE::Renderer::GpuTimer::Scope HBE_GPU_SCOPE_CONCAT(_hbe_gpu_, __LINE__)(NAME)

#endif //HONESTLYBADENGINE_GPUTIMER_H
