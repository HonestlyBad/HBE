#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

// NDEBUG is the portable debug/release signal: CMake adds it to Release,
// RelWithDebInfo and MinSizeRel on every compiler, and never to Debug. MSVC's
// _DEBUG is not available on GCC/Clang, so gating on it compiled the whole
// profiler out of Linux Debug builds.
#ifndef HBE_PROFILE_ENABLED
	#if defined(NDEBUG)
		#define HBE_PROFILE_ENABLED 0
	#else
		#define HBE_PROFILE_ENABLED 1
	#endif
#endif

namespace HBE::Core::Profiler {
	static constexpr std::size_t kRollingWindowFrames = 120;
	static constexpr std::size_t kMaxSections = 64;
	static constexpr std::size_t kMaxScopesPerFrame = 512;

	struct Sample {
		const char* name = nullptr;
		int depth = 0;
		double currentMs = 0.0f;
		double avgMs = 0.0;
		double minMs = 0.0;
		double maxMs = 0.0;
		std::size_t sampleCount = 0;
		std::size_t opensThisFrame = 0;
	};

	struct RendererStats
	{
		int drawCalls = 0;
		int passes = 0;
		int submittedQuads = 0;
		int renderedQuads = 0;
		int culledSprites = 0;
		int materialChanges = 0;
		int textureChanges = 0;
		int visibleTileChunks = 0;
		int postProcessPasses = 0;
		int activeLights = 0;
		int shadowCastingLights = 0;
		int liveParticles = 0;
	};

	struct GpuTimings
	{
		bool supported = false;
		double frameMs = 0.0;
		double frameAvgMs = 0.0;
		double frameMinMs = 0.0;
		double frameMaxMs = 0.0;
		std::size_t frameSampleCount = 0;
		std::vector<Sample> sections;
	};

	struct Snapshot {
		double frameMs = 0.0;
		double frameAvgMs = 0.0;
		double frameMinMs = 0.0;
		double frameMaxMs = 0.0;
		std::size_t frameSampleCount = 0;
		std::uint64_t frameIndex = 0;
		std::vector<Sample> sections;

		GpuTimings gpu;
		RendererStats renderer;
	};

	void BeginFrame();
	void EndFrame();

	void SetEnabled(bool on);
	bool IsEnabled();
	
	void Reset();

	const Snapshot& GetSnapshot();

	std::uint64_t NowNs();

	int BeginScope(const char* name);
	void EndScope(int index);

	void SetGpuSupported(bool supported);
	void PublishGpuSection(const char* name, std::uint64_t ns, int depth);
	void PublishGpuFrame(std::uint64_t ns);
	void PublishRendererStats(const RendererStats& stats);
	void PublishLightStats(int activeLights, int shadowCastingLights);
	void PublishParticleStats(int liveParticles);

	class ScopeTimer {
	public:
		explicit ScopeTimer(const char* name) noexcept : m_index(BeginScope(name)) {}
		~ScopeTimer() noexcept { EndScope(m_index); }

		ScopeTimer(const ScopeTimer&) = delete;
		ScopeTimer& operator=(const ScopeTimer&) = delete;
		ScopeTimer(ScopeTimer&&) = delete;
		ScopeTimer& operator=(ScopeTimer&&) = delete;
	private:
		int m_index;
	};
}

#define HBE_PROFILE_CONCAT_INNER(a, b) a##b
#define HBE_PROFILE_CONCAT(a, b) HBE_PROFILE_CONCAT_INNER(a, b)

#if HBE_PROFILE_ENABLED
	#define HBE_PROFILE_SCOPE(NAME) ::HBE::Core::Profiler::ScopeTimer HBE_PROFILE_CONCAT(_hbe_prof_, __LINE__)(NAME)
	#define HBE_PROFILE_BEGIN_FRAME() ::HBE::Core::Profiler::BeginFrame()
	#define HBE_PROFILE_END_FRAME() ::HBE::Core::Profiler::EndFrame()
#else
	#define HBE_PROFILE_SCOPE(NAME) ((void)0)
	#define HBE_PROFILE_BEGIN_FRAME() ((void)0)
	#define HBE_PROFILE_END_FRAME() ((void)0)
#endif