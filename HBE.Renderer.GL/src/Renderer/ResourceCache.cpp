#include "HBE/Renderer/ResourceCache.h"

#include "HBE/Renderer/GLShader.h"
#include "HBE/Renderer/Texture2D.h"
#include "HBE/Renderer/Mesh.h"
#include "HBE/Core/Log.h"

namespace HBE::Renderer {

	using HBE::Core::LogError;

	GLShader* ResourceCache::getOrCreateShader(const std::string& name, const char* vertexSrc, const char* fragmentSrc) {
		auto it = m_shaders.find(name);
		if (it != m_shaders.end()) {
			return it->second.get();
		}

		auto shader = std::make_unique<GLShader>();
		if (!shader->createFromSource(vertexSrc, fragmentSrc)) {
			LogError("ResourceCache: failed to create shader '" + name + "'");
			return nullptr;
		}

		GLShader* raw = shader.get();
		m_shaders.emplace(name, std::move(shader));
		return raw;
	}

	Texture2D* ResourceCache::getOrCreateCheckerTexture(const std::string& name, int width, int height) {
		auto it = m_textures.find(name);
		if (it != m_textures.end()) {
			return it->second.get();
		}

		auto tex = std::make_unique<Texture2D>();
		if (!tex->createChecker(width, height)) {
			LogError("ResourceCache: failed to create checker texture '" + name + "'");
			return nullptr;
		}

		Texture2D* raw = tex.get();
		m_textures.emplace(name, std::move(tex));
		return raw;
	}

	Mesh* ResourceCache::getOrCreateMeshPosColor(const std::string& name,
		const std::vector<float>& vertices,
		int vertexCount)
	{
		auto it = m_meshes.find(name);
		if (it != m_meshes.end()) {
			return it->second.get();
		}

		auto mesh = std::make_unique<Mesh>();
		if (!mesh->create(vertices, vertexCount)) {
			LogError("ResourceCache: failed to create pos+color mesh '" + name + "'");
			return nullptr;
		}

		Mesh* raw = mesh.get();
		m_meshes.emplace(name, std::move(mesh));
		return raw;
	}


	Mesh* ResourceCache::getOrCreateMeshPosUV(const std::string& name, const std::vector<float>& vertices, int vertexCount) {
		auto it = m_meshes.find(name);
		if (it != m_meshes.end()) {
			return it->second.get();
		}

		auto mesh = std::make_unique<Mesh>();
		if (!mesh->createPostUV(vertices, vertexCount)) {
			LogError("ResourceCache: failed to create pos+UV mesh '" + name + "'");
			return nullptr;
		}

		Mesh* raw = mesh.get();
		m_meshes.emplace(name, std::move(mesh));
		return raw;
	}

	GLShader* ResourceCache::getShader(const std::string& name) const {
		auto it = m_shaders.find(name);
		if (it == m_shaders.end()) return nullptr;
		return it->second.get();
	}

	Texture2D* ResourceCache::getTexture(const std::string& name) const {
		auto it = m_textures.find(name);
		if (it == m_textures.end()) return nullptr;
		return it->second.get();
	}

	Texture2D* ResourceCache::getOrCreateTextureFromFile(
		const std::string& name,
		const std::string& path)
	{
		auto it = m_textures.find(name);
		if (it != m_textures.end()) {
			return it->second.get();
		}

		auto tex = std::make_unique<Texture2D>();
		if (!tex->loadFromFile(path)) {
			LogError("ResourceCache: failed to load texture '" + name +
				"' from '" + path + "'");
			return nullptr;
		}

		Texture2D* raw = tex.get();
		m_textures.emplace(name, std::move(tex));
		return raw;
	}


	Mesh* ResourceCache::getMesh(const std::string& name) const {
		auto it = m_meshes.find(name);
		if (it == m_meshes.end()) return nullptr;
		return it->second.get();
	}
}