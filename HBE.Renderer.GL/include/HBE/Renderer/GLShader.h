#pragma once

#include <string>

namespace HBE::Renderer {

    // Simple wrapper around an OpenGL shader program.
    // Supports:
    //  - create from in-memory source strings
    //  - create/reload from vertex+fragment files (for hot reload)
    class GLShader {
    public:
        GLShader() = default;
        ~GLShader();

        GLShader(const GLShader&) = delete;
        GLShader& operator=(const GLShader&) = delete;

        bool createFromSource(const char* vertexSrc, const char* fragmentSrc);
        bool createFromFiles(const std::string& vertexPath, const std::string& fragmentPath);

        // Reload using the currently stored file paths.
        // If compilation/link fails, the old program is kept alive.
        bool reloadFromFiles();

        bool setMat4(const char* name, const float* value) const;

        int getUniformLocation(const char* name) const;

        void use() const;

        const std::string& vertexPath() const { return m_vertexPath; }
        const std::string& fragmentPath() const { return m_fragmentPath; }

    private:
        unsigned int m_program = 0;   // GL program handle
        std::string m_vertexPath;
        std::string m_fragmentPath;

        void destroy();

        static bool readAllText(const std::string& path, std::string& out);
        static bool buildProgramFromSource(const char* vs, const char* fs, unsigned int& outProgram);
    };

} // namespace HBE::Renderer