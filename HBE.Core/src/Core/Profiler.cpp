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
	}

	void BeginFrame() {
		State& s = S();
		if (!s.enabled) return;

		for (std::size_t i = 0; i < s.sectionCount; ++i) {
			s.sections[i].currentMs = 0.0;
			s.sections[i].opensThisFrame = 0;
			s.sections[i].seenThisFrame = false;
		}
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
		s.inFrame = false;
	}
	const Snapshot& GetSnapshot() {
		return S().snapshot;
	}
}