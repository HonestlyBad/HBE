#include "HBE/Core/LayerStack.h"
#include "HBE/Core/Application.h"

namespace HBE::Core {

	void LayerStack::pushLayer(std::unique_ptr<Layer> layer, Application& app) {
		if (!layer) return;

		m_layers.insert(m_layers.begin() + static_cast<long>(m_insertIndex), std::move(layer));
		++m_insertIndex;

		m_layers[m_insertIndex - 1]->onAttach(app);
	}

	void LayerStack::pushOverlay(std::unique_ptr<Layer> overlay, Application& app) {
		if (!overlay) return;

		m_layers.emplace_back(std::move(overlay));
		m_layers.back()->onAttach(app);
	}

	void LayerStack::popLayer(Layer* layer) {
		if (!layer) return;

		for (std::size_t i = 0; i < m_insertIndex; ++i) {
			if (m_layers[i].get() == layer) {
				m_layers[i]->onDetach();
				m_layers.erase(m_layers.begin() + static_cast<long>(i));
				--m_insertIndex;
				return;
			}
		}
	}

	void LayerStack::popOverlay(Layer* overlay) {
		if (!overlay)return;

		for (std::size_t i = m_insertIndex; i < m_layers.size(); ++i) {
			if (m_layers[i].get() == overlay) {
				m_layers[i]->onDetach();
				m_layers.erase(m_layers.begin() + static_cast<long>(i));
				return;
			}
		}
	}

	void LayerStack::clear() {
		for (auto& l : m_layers) {
			if (l) l->onDetach();
		}
		m_layers.clear();
		m_insertIndex = 0;
	}
}