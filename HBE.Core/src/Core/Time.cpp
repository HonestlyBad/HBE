#include "HBE/Core/Time.h"
#include <chrono>

namespace HBE::Core {
	using Clock = std::chrono::steady_clock;

	static const auto g_startTime = Clock::now();

	double GetTimeSeconds() {
		const auto now = Clock::now();
		const auto diff = now - g_startTime;
		const auto secs = std::chrono::duration<double>(diff);
		return secs.count();
	}

	std::uint64_t GetTimeMillis() {
		const auto now = Clock::now();
		const auto diff = now - g_startTime;
		const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
		return static_cast<std::uint64_t>(millis.count());
	}
}