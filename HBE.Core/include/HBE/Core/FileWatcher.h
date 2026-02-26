#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <filesystem>
#include <chrono>

namespace HBE::Core {

	// Simple polling-based file watcher.
	// - Cross-platform (std::filesystem)
	// - Designed for dev hot-reload (shader/json/texture)
	// - Call poll() every frame or every few frames.
	class FileWatcher {
	public:
		using Callback = std::function<void(const std::string& path)>;

		struct Options {
			// How often we check the filesystem.
			float pollIntervalSeconds = 0.25f;

			// Minimum time between firing callbacks for the same file.
			// Helps avoid double-fires from "atomic save" behavior.
			float debounceSeconds = 0.25f;
		};

		FileWatcher() = default;
		explicit FileWatcher(const Options& opt) : m_opt(opt) {}

		void setOptions(const Options& opt) { m_opt = opt; }
		const Options& options() const { return m_opt; }

		// Add or replace a watch.
		// If the file doesn't exist yet, we'll still watch it and fire once it appears/changes.
		void watchFile(const std::string& path, Callback cb);

		void unwatchFile(const std::string& path);
		void clear();

		// Poll for changes (call from your main update loop).
		void poll(float dtSeconds);

	private:
		struct Entry {
			Callback cb;
			std::filesystem::file_time_type lastWrite{};
			std::chrono::steady_clock::time_point lastFire{};
			bool hadFile = false;
		};

		Options m_opt{};
		float m_accum = 0.0f;
		std::unordered_map<std::string, Entry> m_entries;

		static bool tryGetWriteTime(const std::string& path, std::filesystem::file_time_type& outTime);
	};

} // namespace HBE::Core