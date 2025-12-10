#pragma once

#include <string_view>

namespace HBE::Core {

	enum class LogLevel {
		Trace,
		Info,
		Warn,
		Error,
		Fatal
	};

	void SetLogLevel(LogLevel level);
	void Log(LogLevel level, std::string_view message);

	// helpers
	void LogTrace(std::string_view message);
	void LogInfo(std::string_view message);
	void LogWarn(std::string_view message);
	void LogError(std::string_view message);
	void LogFatal(std::string_view message);
}