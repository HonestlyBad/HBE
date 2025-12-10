#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>

#include "HBE/Renderer/GLShader.h"    
#include "HBE/Renderer/Texture2D.h"   
#include "HBE/Renderer/Mesh.h"        

namespace HBE::Renderer {

    class ResourceCache {
    public:
        ResourceCache() = default;
        ~ResourceCache() = default;

        ResourceCache(const ResourceCache&) = delete;
        ResourceCache& operator=(const ResourceCache&) = delete;

        GLShader* getOrCreateShader(const std::string& name,
            const char* vertexSrc,
            const char* fragmentSrc);
        Texture2D* getOrCreateCheckerTexture(const std::string& name,
            int width,
            int height);
        Mesh* getOrCreateMeshPosColor(const std::string& name,
            const std::vector<float>& vertices,
            int vertexCount);
        Mesh* getOrCreateMeshPosUV(const std::string& name,
            const std::vector<float>& vertices,
            int vertexCount);

        // in class ResourceCache public section
        Texture2D* getOrCreateTextureFromFile(const std::string& name,
            const std::string& path);


        GLShader* getShader(const std::string& name) const;
        Texture2D* getTexture(const std::string& name) const;
        Mesh* getMesh(const std::string& name) const;

    private:
        std::unordered_map<std::string, std::unique_ptr<GLShader>>  m_shaders;
        std::unordered_map<std::string, std::unique_ptr<Texture2D>> m_textures;
        std::unordered_map<std::string, std::unique_ptr<Mesh>>      m_meshes;
    };

} // namespace HBE::Renderer
