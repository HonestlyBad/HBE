#include "HBE/Renderer/SpriteAnimationStateMachine.h"

#include "HBE/Renderer/RenderItem.h"

namespace HBE::Renderer {
	// clips - events
	void SpriteAnimationStateMachine::addClip(const Clip& clip) {
		m_clips[clip.name] = clip;
	}

	const SpriteAnimationStateMachine::Clip* SpriteAnimationStateMachine::getClip(const std::string& name) const {
		auto it = m_clips.find(name);
		return (it == m_clips.end()) ? nullptr : &it->second;
	}

	void SpriteAnimationStateMachine::addEvent(const std::string& clipName, int frame, const std::string& eventName) {
		if (frame < 0) frame = 0;
		m_events[clipName].push_back(ClipEvent{ eventName, frame });
	}

	// states / transitions
	void SpriteAnimationStateMachine::addState(const std::string& stateName, const std::string& clipName, float speed) {
		State s;
		s.name = stateName;
		s.clipName = clipName;
		s.speed = speed;
		m_states[stateName] = s;

		// if we don't have a state yet, default to first added.
		if (m_currentState.empty()) {
			m_currentState = stateName;
			resolvePointersForState();
			restartClip();
		}
	}

	void SpriteAnimationStateMachine::addTransitionAlways(const std::string& fromState, const std::string& toState) {
		Transition t;
		t.fromState = fromState;
		t.toState = toState;
		t.type = TransitionType::Always;
		m_transitions.push_back(t);
	}

	void SpriteAnimationStateMachine::addTransitionBool(const std::string& fromState, const std::string& toState, const std::string& var, bool value) {
		Transition t;
		t.fromState = fromState;
		t.toState = toState;
		t.type = TransitionType::BoolEquals;
		t.varOrTrigger = var;
		t.boolValue = value;
		m_transitions.push_back(t);
	}

	void SpriteAnimationStateMachine::addTransitionTrigger(const std::string& fromState, const std::string& toState, const std::string& triggerName) {
		Transition t;
		t.fromState = fromState;
		t.toState = toState;
		t.type = TransitionType::Trigger;
		t.varOrTrigger = triggerName;
		m_transitions.push_back(t);
	}

	void SpriteAnimationStateMachine::addTransitionFinished(const std::string& fromState, const std::string& toState) {
		Transition t;
		t.fromState = fromState;
		t.toState = toState;
		t.type = TransitionType::Finished;
		m_transitions.push_back(t);
	}

	void SpriteAnimationStateMachine::setBool(const std::string& name, bool value) {
		m_bools[name] = value;
	}

	bool SpriteAnimationStateMachine::getBool(const std::string& name) const {
		auto it = m_bools.find(name);
		return(it == m_bools.end()) ? false : it->second;
	}

	void SpriteAnimationStateMachine::trigger(const std::string& name) {
		m_triggers.insert(name);
	}

	void SpriteAnimationStateMachine::setState(const std::string& stateName, bool restart) {
		if (m_currentState == stateName && !restart) {
			return;
		}
		m_currentState = stateName;
		resolvePointersForState();
		if (restart)restartClip();
	}

	// update / apply
	void SpriteAnimationStateMachine::update(float dt, const EventCallback& onEvent) {
		// if sheet isn't set, we can still tick state/vars, but can't apply frames.
		if (dt < 0.0f) dt = 0.0f;

		// allow transitions to react immediately, even if dt == 0
		runTransitions();

		if (!m_statePtr || !m_clipPtr) {
			// try to resolve if state exists now
			resolvePointersForState();
		}
		if (!m_statePtr || !m_clipPtr) return;

		// scale dt
		float speed = globalSpeed;
		speed *= (m_clipPtr->speed <= 0.0f ? 1.0f : m_clipPtr->speed);
		speed *= (m_clipPtr->speed <= 0.0f ? 1.0f : m_statePtr->speed);
		float scaledDt = dt * speed;

		stepFrames(scaledDt, onEvent);

		// transitions can depend on "finished"
		runTransitions();

		// triggers are "signle tick" by default
		m_triggers.clear();
	}

	void SpriteAnimationStateMachine::apply(RenderItem& item) const {
		if (!sheet) return;
		if (!m_clipPtr) return;

		int col = m_clipPtr->startCol + m_frame;
		int row = m_clipPtr->row;

		SpriteRenderer2D::setFrame(item, *sheet, col, row);
	}

	// internals

	void SpriteAnimationStateMachine::resolvePointersForState() {
		auto itS = m_states.find(m_currentState);
		m_statePtr = (itS == m_states.end()) ? nullptr : &itS->second;

		m_clipPtr = nullptr;
		if (m_statePtr) {
			auto itC = m_clips.find(m_statePtr->clipName);
			if (itC != m_clips.end()) {
				m_clipPtr = &itC->second;
			}
		}
	}

	void SpriteAnimationStateMachine::restartClip() {
		m_frame = 0;
		m_timer = 0.0f;
		m_playing = true;
		m_clipFinished = false;
	}

	void SpriteAnimationStateMachine::fireFrameEvents(const EventCallback& onEvent, int frame) {
		if (!onEvent) return;
		if (!m_clipPtr) return;

		auto it = m_events.find(m_clipPtr->name);
		if (it == m_events.end()) return;

		for (const ClipEvent& ev : it->second) {
			if (ev.frame == frame) {
				onEvent(ev.name);
			}
		}
	}

	void SpriteAnimationStateMachine::stepFrames(float scaledDt, const EventCallback& onEvent) {
		if (!m_clipPtr) return;
		if (!m_playing) return;

		// if clip has 0 or 1 frames, keep at 0
		if (m_clipPtr->frameCount <= 1) {
			m_frame = 0;
			return;
		}
		float frameDuration = m_clipPtr->frameDuration;
		if (frameDuration <= 0.00001f) frameDuration = 0.00001f;

		m_timer += scaledDt;

		// If this is the first time we are updating after (re)entering a state,
		// fire frame-0 events once.
		// We treat "frameTimer just advanced from 0" as a good proxy by firing
		// if timer == scaledDt and frame == 0.
		if (scaledDt > 0.0f && m_frame == 0 && m_timer == scaledDt) {
			fireFrameEvents(onEvent, 0);
		}

		while (m_timer >= frameDuration) {
			m_timer -= frameDuration;

			int nextFrame = m_frame + 1;
			if (nextFrame >= m_clipPtr->frameCount) {
				if (m_clipPtr->loop) {
					nextFrame = 0;
				}
				else {
					// clamp at last frame
					nextFrame = m_clipPtr->frameCount - 1;

					// IMPORTANT:
					// For non-looping, consider the clip "finished" as soon as we ENTER the last frame.
					// This removes the extra frameDuration of latency before transitions fire.
					m_clipFinished = true;
					m_playing = false;
				}
			}
			else {
				// If we're entering the last frame of a non-looping clip, finish immediately
				if (!m_clipPtr->loop && nextFrame == (m_clipPtr->frameCount - 1)) {
					m_clipFinished = true;
					m_playing = false;
				}
			}

			// fire events for the frame we are entering
			if (nextFrame != m_frame) {
				m_frame = nextFrame;
				fireFrameEvents(onEvent, m_frame);
			}
			if (!m_playing) break;
		}
	}

	bool SpriteAnimationStateMachine::transitionMatchesFrom(const Transition& t) const {
		if (t.fromState == "*") return true;
		return t.fromState == m_currentState;
	}

	bool SpriteAnimationStateMachine::transitionConditionMet(const Transition& t)const {
		switch (t.type) {
		case TransitionType::Always:
			return true;
		case TransitionType::BoolEquals: {
			auto it = m_bools.find(t.varOrTrigger);
			bool v = (it == m_bools.end()) ? false : it->second;
			return v == t.boolValue;
		}
		case TransitionType::Trigger:
			return m_triggers.find(t.varOrTrigger) != m_triggers.end();
		case TransitionType::Finished:
			return m_clipFinished;
		default:
			return false;
		}
	}

	void SpriteAnimationStateMachine::consumeTriggerIfUsed(const Transition& t) {
		if (t.type == TransitionType::Trigger) {
			m_triggers.erase(t.varOrTrigger);
		}
	}

	void SpriteAnimationStateMachine::runTransitions() {
		if (m_transitions.empty()) return;

		// evaluate in order; first match wins.
		for (const Transition& t : m_transitions) {
			if (!transitionMatchesFrom(t)) continue;
			if (!transitionConditionMet(t)) continue;

			// switch state
			if (t.toState != m_currentState) {
				m_currentState = t.toState;
				resolvePointersForState();
				restartClip();
			}
			consumeTriggerIfUsed(t);
			// one transition per tick (keeps behavior deterministic)
			break;
		}
	}
}