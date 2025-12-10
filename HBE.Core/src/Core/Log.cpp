#include "HBE/Core/Log.h"

#include <iostream>
#include <mutex>

namespace HBE::Core {

	namespace {
		LogLevel g_minLevel = LogLevel::Trace;
		std::mutex g_logMutex;

		const char* LevelToString(LogLevel level) {
			switch (level) {
			case LogLevel::Trace: return "TRACE";
			case LogLevel::Info: return "INFO";
			case LogLevel::Warn: return "WARN";
			case LogLevel::Error: return "ERROR";
			case LogLevel::Fatal: return "FATAL";
			}
			return "UNKN";
		}
	}

	void SetLogLevel(LogLevel level) {
		g_minLevel = level;
	}

	void Log(LogLevel level, std::string_view message) {
		if (static_cast<int>(level) < static_cast<int>(g_minLevel)) {
			return;
		}

		std::lock_guard<std::mutex> lock(g_logMutex);
		std::cerr << "[" << LevelToString(level) << "]" << message << '\n';
	}

	void LogTrace(std::string_view message) { Log(LogLevel::Trace, message); }
	void LogInfo(std::string_view message) { Log(LogLevel::Info, message); }
	void LogWarn(std::string_view message) { Log(LogLevel::Warn, message); }
	void LogError(std::string_view message) { Log(LogLevel::Error, message); }
	void LogFatal(std::string_view message) { Log(LogLevel::Fatal, message); }
}