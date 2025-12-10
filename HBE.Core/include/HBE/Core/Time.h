#pragma once

#include <cstdint>

namespace HBE::Core {
	// simple timing api later back with SDL or std::chrono in platform layer
	double GetTimeSeconds(); // since engine start
	std::uint64_t GetTimeMillis();
}