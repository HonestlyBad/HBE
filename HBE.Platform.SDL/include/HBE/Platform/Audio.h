#pragma once

#include <string>
#include <unordered_map>

struct MIX_Mixer;
struct MIX_Track;
struct MIX_Audio;

namespace HBE::Platform {

	class Audio {
	public:
		Audio() = default;
		~Audio();

		Audio(const Audio&) = delete;
		Audio& operator=(const Audio&) = delete;

		bool initialize();
		void shutdown();

		// Load an audio file (WAV, MP3, OGG, FLAC, etc.) and store it under `name`.
		// Set `predecode` to true for short sound effects (decode fully into RAM).
		bool loadSound(const std::string& name, const std::string& path, bool predecode = true);

		// Play a previously loaded sound. Returns true on success.
		// loops: 0 = play once, -1 = loop forever, N = loop N times.
		bool playSound(const std::string& name, int loops = 0);

		// Stop a named sound if it is currently playing.
		void stopSound(const std::string& name);

		// Stop all sounds.
		void stopAll();

		// Master gain (0.0 = silent, 1.0 = full).
		void setMasterGain(float gain);
		float masterGain() const;

		bool isInitialized() const { return m_initialized; }

	private:
		bool m_initialized = false;
		MIX_Mixer* m_mixer = nullptr;

		struct SoundEntry {
			MIX_Audio* audio = nullptr;
			MIX_Track* track = nullptr;
		};

		std::unordered_map<std::string, SoundEntry> m_sounds;
	};

} // namespace HBE::Platform
