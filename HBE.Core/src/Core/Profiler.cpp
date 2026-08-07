#include "HBE/Core/Profiler.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstring>
#include <limits>

namespace HBE::Core::Profiler {
	namespace {
		struct SectionStats {
			const char* name = nullptr;
			int depth = 0;
			std::size_t writeCursor = 0;
			std::array<double, kRollingWindowFrames> samplesMs{};
			std::size_t count = 0;
			double currentMs = 0.0;
			double avgMs = 0.0;
			double minMs = 0.0;
			double maxMs = 0.0;
			std::size_t opensThisFrame = 0;
			bool seenThisFrame = false;
		};

		struct GpuSectionStats
		{
			const char* name = nullptr;
			int depth = 0;
			std::size_t writeCursor = 0;
			std::array<double, kRollingWindowFrames> samplesMs{};
			std::size_t count = 0;
			double currentMs = 0.0;
			double avgMs = 0.0;
			double minMs = 0.0;
			double maxMs = 0.0;
			std::size_t opensThisFrame = 0;
			bool seenThisFrame = false;
		};

		struct OpenScope {
			const char* name = nullptr;
			std::uint64_t startNs = 0;
			int depth = 0;
			int sectionIndex = -1;
		};

		struct State {
			bool enabled = true;
			bool inFrame = false;
			std::uint64_t frameStartNs = 0;
			std::uint64_t frameIndex = 0;

			std::array<SectionStats, kMaxSections> sections{};
			std::size_t sectionCount = 0;
			std::array<OpenScope, kMaxScopesPerFrame> scopeStack{};
			std::size_t scopeStackSize = 0;
			int currentDepth = 0;

			std::array<double, kRollingWindowFrames> frameSamplesMs{};
			std::size_t frameCursor = 0;
			std::size_t frameSampleCount = 0;
			double frameAvgMs = 0.0;
			double frameMinMs = 0.0;
			double frameMaxMs = 0.0;

			// GPU
			bool gpuSupported = false;
			std::array<GpuSectionStats, kMaxSections> gpuSections{};
			std::size_t gpuSectionCount = 0;
			std::array<double, kRollingWindowFrames> gpuFrameSamplesMs{};
			std::size_t gpuFrameCursor = 0;
			std::size_t gpuFrameSampleCount = 0;
			double gpuFrameAvgMs = 0.0;
			double gpuFrameMinMs = 0.0;
			double gpuFrameMaxMs = 0.0;

			RendererStats renderer{};
			bool rendererValid = false;

			Snapshot snapshot{};
		};

		State& S() {
			static State s;
			return s;
		}

		int findOrAddSection(const char* name, int depth) {
			State& s = S();
			for (std::size_t i = 0; i < s.sectionCount; ++i) {
				if (s.sections[i].name == name) {
					return static_cast<int>(i);
				}
			}
			if (s.sectionCount >= kMaxSections) {
				return -1;
			}

			SectionStats& st = s.sections[s.sectionCount];
			st = SectionStats{};
			st.name = name;
			st.depth = depth;

			const int idx = static_cast<int>(s.sectionCount);
			++s.sectionCount;
			return idx;
		}

		int findOrAddGpuSection(const char* name, int depth)
		{
			State& s = S();
			for (std::size_t i = 0; i < s.gpuSectionCount; ++i)
			{
				if (s.gpuSections[i].name == name)
				{
					return static_cast<int>(i);
				}
			}
			if (s.gpuSectionCount >= kMaxSections) return -1;

			GpuSectionStats& st = s.gpuSections[s.gpuSectionCount];
			st = GpuSectionStats{};
			st.name = name;
			st.depth = depth;

			const int idx = static_cast<int>(s.gpuSectionCount);
			++s.gpuSectionCount;
			return idx;
		}

		void recomputeGpuRollingStats(GpuSectionStats& st)
		{
			if (st.count == 0)
			{
				st.avgMs = 0.0; st.minMs = 0.0; st.maxMs = 0.0;
				return;
			}
			double sum = 0.0;
			double mn = std::numeric_limits<double>::infinity();
			double mx = -std::numeric_limits<double>::infinity();
			for (std::size_t i = 0; i < st.count; ++i)
			{
				const double v = st.samplesMs[i];
				sum += v;
				if (v < mn) mn = v;
				if (v > mx) mx = v;
			}
			st.avgMs = sum / static_cast<double>(st.count);
			st.minMs = mn;
			st.maxMs = mx;
		}

		void recomputeGpuFrameStats(State& s)
		{
			if (s.gpuFrameSampleCount == 0)
			{
				s.gpuFrameAvgMs = 0.0; s.gpuFrameMinMs = 0.0; s.gpuFrameMaxMs = 0.0;
				return;
			}
			double sum = 0.0;
			double mn = std::numeric_limits<double>::infinity();
			double mx = -std::numeric_limits<double>::infinity();
			for (std::size_t i = 0; i < s.gpuFrameSampleCount; ++i)
			{
				const double v = s.gpuFrameSamplesMs[i];
				sum += v;
				if (v < mn) mn = v;
				if (v > mx) mx = v;
			}
			s.gpuFrameAvgMs = sum / static_cast<double>(s.gpuFrameSampleCount);
			s.gpuFrameMinMs = mn;
			s.gpuFrameMaxMs = mx;
		}

		void recomputeRollingStats(SectionStats& st) {
			if (st.count == 0) {
				st.avgMs = 0.0;
				st.minMs = 0.0;
				st.maxMs = 0.0;
				return;
			}
			double sum = 0.0;
			double mn = std::numeric_limits<double>::infinity();
			double mx = -std::numeric_limits<double>::infinity();
			for (std::size_t i = 0; i < st.count; ++i) {
				const double v = st.samplesMs[i];
				sum += v;
				if (v < mn) mn = v;
				if (v > mx) mx = v;
			}
			st.avgMs = sum / static_cast<double>(st.count);
			st.minMs = mn;
			st.maxMs = mx;
		}

		void recomputeFrameStats(State& s) {
			if (s.frameSampleCount == 0) {
				s.frameAvgMs = 0.0;
				s.frameMinMs = 0.0;
				s.frameMaxMs = 0.0;
				return;
			}
			double sum = 0.0;
			double mn = std::numeric_limits<double>::infinity();
			double mx = -std::numeric_limits<double>::infinity();
			for (std::size_t i = 0; i < s.frameSampleCount; ++i) {
				const double v = s.frameSamplesMs[i];
				sum += v;
				if (v < mn) mn = v;
				if (v > mx) mx = v;
			}
			s.frameAvgMs = sum / static_cast<double>(s.frameSampleCount);
			s.frameMinMs = mn;
			s.frameMaxMs = mx;
		}
	}

	std::uint64_t NowNs() {
		const auto now = std::chrono::steady_clock::now().time_since_epoch();
		return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
	}

	void setEnabled(bool on) { S().enabled = on; }
	bool isEnabled() { return S().enabled; }

	void Reset() {
		State& s = S();
		s.sections = {};
		s.sectionCount = 0;
		s.scopeStack = {};
		s.scopeStackSize = 0;
		s.currentDepth = 0;
		s.frameSamplesMs = {};
		s.frameCursor = 0;
		s.frameSampleCount = 0;
		s.frameAvgMs = 0.0;
		s.frameMinMs = 0.0;
		s.frameMaxMs = 0.0;
		s.snapshot = Snapshot{};
		s.inFrame = false;
		s.frameIndex = 0;
		s.gpuSections = {};
		s.gpuSectionCount = 0;
		s.gpuFrameSamplesMs = {};
		s.gpuFrameCursor = 0;
		s.gpuFrameSampleCount = 0;
		s.gpuFrameAvgMs = 0.0;
		s.gpuFrameMinMs = 0.0;
		s.gpuFrameMaxMs = 0.0;
		s.renderer = RendererStats{};
		s.rendererValid = false;
	}

	void BeginFrame() {
		State& s = S();
		if (!s.enabled) return;

		for (std::size_t i = 0; i < s.sectionCount; ++i) {
			s.sections[i].currentMs = 0.0;
			s.sections[i].opensThisFrame = 0;
			s.sections[i].seenThisFrame = false;
		}

		for (std::size_t i = 0; i < s.gpuSectionCount; ++i)
		{
			s.gpuSections[i].currentMs = 0.0;
			s.gpuSections[i].opensThisFrame = 0;
			s.gpuSections[i].seenThisFrame = false;
		}
		s.rendererValid = false;

		s.scopeStackSize = 0;
		s.currentDepth = 0;
		s.frameStartNs = NowNs();
		s.inFrame = true;
	}

	int BeginScope(const char* name) {
		State& s = S();
		if (!s.enabled || !s.inFrame || name == nullptr) return -1;
		if (s.scopeStackSize >= kMaxScopesPerFrame) return -1;

		const int sectionIdx = findOrAddSection(name, s.currentDepth);
		if (sectionIdx < 0) return -1;

		OpenScope& os = s.scopeStack[s.scopeStackSize];
		os.name = name;
		os.startNs = NowNs();
		os.depth = s.currentDepth;
		os.sectionIndex = sectionIdx;

		SectionStats& st = s.sections[sectionIdx];
		if (!st.seenThisFrame) {
			st.depth = s.currentDepth;
			st.seenThisFrame = true;
		}

		++s.scopeStackSize;
		++s.currentDepth;
		return static_cast<int>(s.scopeStackSize) - 1;
	}

	void EndScope(int index) {
		State& s = S();
		if (index < 0) return;
		if (!s.enabled || !s.inFrame) return;
		if (index >= static_cast<int>(s.scopeStackSize)) return;

		while (s.scopeStackSize > 0 && static_cast<int>(s.scopeStackSize) - 1 >= index) {
			const std::size_t top = s.scopeStackSize - 1;
			OpenScope& os = s.scopeStack[top];

			const std::uint64_t endNs = NowNs();
			const std::uint64_t deltaNs = (endNs > os.startNs) ? (endNs - os.startNs) : 0;
			const double deltaMs = static_cast<double>(deltaNs) / 1'000'000.0;

			if (os.sectionIndex >= 0 && static_cast<std::size_t>(os.sectionIndex) < s.sectionCount) {
				SectionStats& st = s.sections[os.sectionIndex];
				st.currentMs += deltaMs;
				++st.opensThisFrame;
			}

			os = OpenScope{};
			--s.scopeStackSize;
			if (s.currentDepth > 0) --s.currentDepth;

			if (static_cast<int>(s.scopeStackSize) <= index) break;
		}
	}

	void EndFrame() {
		State& s = S();
		if (!s.enabled) return;
		if (!s.inFrame) return;

		while (s.scopeStackSize > 0) {
			EndScope(static_cast<int>(s.scopeStackSize) - 1);
		}

		const std::uint64_t endNs = NowNs();
		const std::uint64_t deltaNs = (endNs > s.frameStartNs) ? (endNs - s.frameStartNs) : 0;
		const double frameMs = static_cast<double>(deltaNs) / 1'000'000.0;

		s.frameSamplesMs[s.frameCursor] = frameMs;
		s.frameCursor = (s.frameCursor + 1) % kRollingWindowFrames;
		if (s.frameSampleCount < kRollingWindowFrames) ++s.frameSampleCount;
		recomputeFrameStats(s);

		for (std::size_t i = 0; i < s.sectionCount; ++i) {
			SectionStats& st = s.sections[i];
			if (st.opensThisFrame == 0) continue;
			st.samplesMs[st.writeCursor] = st.currentMs;
			st.writeCursor = (st.writeCursor + 1) % kRollingWindowFrames;
			if (st.count < kRollingWindowFrames) ++st.count;
			recomputeRollingStats(st);
		}

		s.snapshot.frameMs = frameMs;
		s.snapshot.frameAvgMs = s.frameAvgMs;
		s.snapshot.frameMinMs = s.frameMinMs;
		s.snapshot.frameMaxMs = s.frameMaxMs;
		s.snapshot.frameSampleCount = s.frameSampleCount;
		s.snapshot.frameIndex = ++s.frameIndex;
		s.snapshot.sections.clear();
		s.snapshot.sections.reserve(s.sectionCount);
		for (std::size_t i = 0; i < s.sectionCount; ++i) {
			const SectionStats& st = s.sections[i];
			Sample smp{};
			smp.name = st.name;
			smp.depth = st.depth;
			smp.currentMs = st.currentMs;
			smp.avgMs = st.avgMs;
			smp.minMs = st.minMs;
			smp.maxMs = st.maxMs;
			smp.sampleCount = st.count;
			smp.opensThisFrame = st.opensThisFrame;
			s.snapshot.sections.push_back(smp);
		}

		double gpuFrameSumMs = 0.0;
		for (std::size_t i = 0; i < s.gpuSectionCount; ++i)
		{
			GpuSectionStats& st = s.gpuSections[i];
			if (st.opensThisFrame == 0) continue;

			st.samplesMs[st.writeCursor] = st.currentMs;
			st.writeCursor = (st.writeCursor + 1) % kRollingWindowFrames;
			if (st.count < kRollingWindowFrames) ++st.count;
			recomputeGpuRollingStats(st);

			if (st.depth == 0) gpuFrameSumMs += st.currentMs;
		}

		if (s.gpuSectionCount > 0 && s.gpuSupported)
		{
			s.gpuFrameSamplesMs[s.gpuFrameCursor] = gpuFrameSumMs;
			s.gpuFrameCursor = (s.gpuFrameCursor + 1) % kRollingWindowFrames;
			if (s.gpuFrameSampleCount < kRollingWindowFrames) ++s.gpuFrameSampleCount;
			recomputeGpuFrameStats(s);
		}

		s.snapshot.gpu.supported = s.gpuSupported;
		s.snapshot.gpu.frameMs = gpuFrameSumMs;
		s.snapshot.gpu.frameAvgMs = s.gpuFrameAvgMs;
		s.snapshot.gpu.frameMinMs = s.gpuFrameMinMs;
		s.snapshot.gpu.frameMaxMs = s.gpuFrameMaxMs;
		s.snapshot.gpu.frameSampleCount = s.gpuFrameSampleCount;
		s.snapshot.gpu.sections.clear();
		s.snapshot.gpu.sections.reserve(s.gpuSectionCount);
		for (std::size_t i = 0; i < s.gpuSectionCount; ++i)
		{
			const GpuSectionStats& st = s.gpuSections[i];
			Sample g{};
			g.name = st.name;
			g.depth = st.depth;
			g.currentMs = st.currentMs;
			g.avgMs = st.avgMs;
			g.minMs = st.minMs;
			g.maxMs = st.maxMs;
			g.sampleCount = st.count;
			g.opensThisFrame = st.opensThisFrame;
			s.snapshot.gpu.sections.push_back(g);
		}
		s.snapshot.renderer = s.renderer;
		s.inFrame = false;
	}

	const Snapshot& GetSnapshot() {
		return S().snapshot;
	}

	void SetGpuSupported(bool supported)
	{
		S().gpuSupported = supported;
	}

	void PublishGpuSection(const char* name, std::uint64_t ns, int depth)
	{
		State& s = S();
		if (!s.enabled) return;
		if (name == nullptr) return;

		const int idx = findOrAddGpuSection(name, depth);
		if (idx < 0) return;

		GpuSectionStats& st = s.gpuSections[idx];
		const double ms = static_cast<double>(ns) / 1'000'000.0;
		st.currentMs += ms;
		++st.opensThisFrame;
		if (!st.seenThisFrame)
		{
			st.depth = depth;
			st.seenThisFrame = true;
		}
	}

	void PublishGpuFrame(std::uint64_t)
	{
		// intentionally empty
	}

	void PublishRendererStats(const RendererStats& stats)
	{
		State& s = S();
		if (!s.enabled) return;

		const int prevActiveLights = s.renderer.activeLights;
		const int prevShadowLights = s.renderer.shadowCastingLights;
		const int prevLiveParticles = s.renderer.liveParticles;
		const int prevTileChunks = s.renderer.visibleTileChunks;
		const int prevPPPasses = s.renderer.postProcessPasses;

		s.renderer = stats;

		if (stats.activeLights == 0) s.renderer.activeLights = prevActiveLights;
		if (stats.shadowCastingLights == 0) s.renderer.shadowCastingLights = prevShadowLights;
		if (stats.liveParticles == 0) s.renderer.liveParticles = prevLiveParticles;
		if (stats.visibleTileChunks == 0) s.renderer.visibleTileChunks = prevTileChunks;
		if (stats.postProcessPasses == 0) s.renderer.postProcessPasses = prevPPPasses;

		s.rendererValid = true;
	}

	void PublishLightStats(int activeLights, int shadowCastingLights)
	{
		State& s = S();
		if (!s.enabled) return;
		s.renderer.activeLights = activeLights;
		s.renderer.shadowCastingLights = shadowCastingLights;
	}

	void PublishParticleStats(int liveParticles)
	{
		State& s = S();
		if (!s.enabled) return;
		s.renderer.liveParticles = liveParticles;
	}

}