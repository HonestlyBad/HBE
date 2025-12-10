#pragma once

#include <string>

namespace HBE::Renderer {

    // Simple wrapper around an OpenGL shader program.
    // We keep the OpenGL types opaque here (no glad in the header).
    class GLShader {
    public:
        GLShader() = default;
        ~GLShader();

        GLShader(const GLShader&) = delete;
        GLShader& operator=(const GLShader&) = delete;


        bool createFromSource(const char* vertexSrc, const char* fragmentSrc);
        bool setMat4(const char* name, const float* value) const;

        int getUniformLocation(const char* name) const;

        void use() const;

    private:
        unsigned int m_program = 0;   // GL program handle

        void destroy();
    };

} // namespace HBE::Renderer
