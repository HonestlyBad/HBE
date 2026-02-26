#include "HBE/Core/UUID.h"

#include <random>
#include <sstream>
#include <iomanip>

namespace HBE::Core {

    std::string NewUUID32() {
        // 128-bit random -> 32 hex chars
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<unsigned long long> dist;

        const unsigned long long a = dist(gen);
        const unsigned long long b = dist(gen);

        std::ostringstream oss;
        oss << std::hex << std::setfill('0')
            << std::setw(16) << a
            << std::setw(16) << b;

        return oss.str(); // 32 chars
    }

}