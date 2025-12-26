#pragma once

namespace HBE::Core {

	class Application;

	class Layer {
	public:
		virtual ~Layer() = default;

		virtual void onAttach(Application& app) {}
		virtual void onDetach() {}

		virtual void onUpdate(float dt) {}
		virtual void onRender() {}
	};
}