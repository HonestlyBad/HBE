#include "HBE/Platform/Audio.h"
#include "HBE/Core/Log.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

namespace HBE::Platform {

	using HBE::Core::LogError;
	using HBE::Core::LogInfo;
	using HBE::Core::LogWarn;

	Audio::~Audio() {
		shutdown();
	}

	bool Audio::initialize() {
		if (m_initialized) return true;

		if (!MIX_Init()) {
			LogError(std::string("MIX_Init failed: ") + SDL_GetError());
			return false;
		}

		m_mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
		if (!m_mixer) {
			LogError(std::string("MIX_CreateMixerDevice failed: ") + SDL_GetError());
			MIX_Quit();
			return false;
		}

		m_initialized = true;
		LogInfo("Audio initialized (SDL3_mixer).");
		return true;
	}

	void Audio::shutdown() {
		if (!m_initialized) return;

		for (auto& [name, entry] : m_sounds) {
			if (entry.track) {
				MIX_StopTrack(entry.track, 0);
				MIX_DestroyTrack(entry.track);
			}
			if (entry.audio) {
				MIX_DestroyAudio(entry.audio);
			}
		}
		m_sounds.clear();

		if (m_mixer) {
			MIX_DestroyMixer(m_mixer);
			m_mixer = nullptr;
		}

		MIX_Quit();
		m_initialized = false;
		LogInfo("Audio shut down.");
	}

	bool Audio::loadSound(const std::string& name, const std::string& path, bool predecode) {
		if (!m_initialized) {
			LogError("Audio::loadSound called before initialize().");
			return false;
		}

		if (m_sounds.count(name)) {
			LogWarn("Audio: sound '" + name + "' already loaded, skipping.");
			return true;
		}

		MIX_Audio* audio = MIX_LoadAudio(m_mixer, path.c_str(), predecode);
		if (!audio) {
			LogError("Audio: failed to load '" + path + "': " + SDL_GetError());
			return false;
		}

		MIX_Track* track = MIX_CreateTrack(m_mixer);
		if (!track) {
			LogError("Audio: failed to create track for '" + name + "': " + SDL_GetError());
			MIX_DestroyAudio(audio);
			return false;
		}

		m_sounds[name] = { audio, track };
		LogInfo("Audio: loaded '" + name + "' from " + path);
		return true;
	}

	bool Audio::playSound(const std::string& name, int loops) {
		auto it = m_sounds.find(name);
		if (it == m_sounds.end()) {
			LogError("Audio: sound '" + name + "' not loaded.");
			return false;
		}

		auto& entry = it->second;
		MIX_SetTrackAudio(entry.track, entry.audio);
		MIX_SetTrackLoops(entry.track, loops);
		return MIX_PlayTrack(entry.track, 0);
	}

	void Audio::stopSound(const std::string& name) {
		auto it = m_sounds.find(name);
		if (it != m_sounds.end() && it->second.track) {
			MIX_StopTrack(it->second.track, 0);
		}
	}

	void Audio::stopAll() {
		if (m_mixer) {
			MIX_StopAllTracks(m_mixer, 0);
		}
	}

	void Audio::setMasterGain(float gain) {
		if (m_mixer) {
			MIX_SetMixerGain(m_mixer, gain);
		}
	}

	float Audio::masterGain() const {
		if (m_mixer) {
			return MIX_GetMixerGain(m_mixer);
		}
		return 0.0f;
	}

} // namespace HBE::Platform
