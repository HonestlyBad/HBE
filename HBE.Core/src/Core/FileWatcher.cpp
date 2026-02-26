#include "HBE/Core/FileWatcher.h"

namespace HBE::Core {

	bool FileWatcher::tryGetWriteTime(const std::string& path, std::filesystem::file_time_type& outTime) {
		std::error_code ec;
		if (!std::filesystem::exists(path, ec)) return false;
		outTime = std::filesystem::last_write_time(path, ec);
		return !ec;
	}

	void FileWatcher::watchFile(const std::string& path, Callback cb) {
		Entry e{};
		e.cb = std::move(cb);
		e.lastFire = std::chrono::steady_clock::now() - std::chrono::hours(24);

		std::filesystem::file_time_type wt{};
		if (tryGetWriteTime(path, wt)) {
			e.lastWrite = wt;
			e.hadFile = true;
		}
		else {
			e.hadFile = false;
		}

		m_entries[path] = std::move(e);
	}

	void FileWatcher::unwatchFile(const std::string& path) {
		m_entries.erase(path);
	}

	void FileWatcher::clear() {
		m_entries.clear();
		m_accum = 0.0f;
	}

	void FileWatcher::poll(float dtSeconds) {
		m_accum += dtSeconds;
		if (m_accum < m_opt.pollIntervalSeconds) return;
		m_accum = 0.0f;

		const auto now = std::chrono::steady_clock::now();

		for (auto& kv : m_entries) {
			const std::string& path = kv.first;
			Entry& e = kv.second;

			std::filesystem::file_time_type wt{};
			const bool hasFile = tryGetWriteTime(path, wt);

			// If it didn't exist and now it does -> treat as a change.
			if (!e.hadFile && hasFile) {
				e.hadFile = true;
				e.lastWrite = wt;

				const auto minDelta = std::chrono::duration<float>(m_opt.debounceSeconds);
				if ((now - e.lastFire) >= minDelta) {
					e.lastFire = now;
					if (e.cb) e.cb(path);
				}
				continue;
			}

			// If it still doesn't exist, ignore.
			if (!hasFile) {
				e.hadFile = false;
				continue;
			}

			// File exists and has a new write time.
			if (wt != e.lastWrite) {
				e.lastWrite = wt;

				const auto minDelta = std::chrono::duration<float>(m_opt.debounceSeconds);
				if ((now - e.lastFire) >= minDelta) {
					e.lastFire = now;
					if (e.cb) e.cb(path);
				}
			}
		}
	}

} // namespace HBE::Core