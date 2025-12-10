#pragma once
#include <vector>

namespace HBE::Renderer {

    class Mesh {
    public:
        Mesh() = default;
        ~Mesh();

        Mesh(const Mesh&) = delete;
        Mesh& operator=(const Mesh&) = delete;

        bool create(const std::vector<float>& vertices, int vertexCount);

        // position + UV (x,y,z,u,v)
        bool createPostUV(const std::vector<float>& vertices, int vertexCount);

        unsigned int getVAO() const { return m_vao; }
        int getVertexCount() const { return m_vertexCount; }

    private:
        unsigned int m_vao = 0;
        unsigned int m_vbo = 0;
        int m_vertexCount = 0;

        void destroy();
    };

}
