#pragma once

namespace HBE::ECS {

	// completly optional in gameplay code. bookkeeping components for systems.
	struct ScriptRuntimeState {
		bool created = false;
	};
}