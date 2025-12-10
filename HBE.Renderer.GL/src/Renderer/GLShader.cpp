#include "HBE/Renderer/GLShader.h"
#include "HBE/Core/Log.h"

#include <glad/glad.h>

namespace HBE::Renderer {

    using HBE::Core::LogError;

    GLShader::~GLShader() {
        destroy();
    }

    void GLShader::destroy() {
        if (m_program != 0) {
            glDeleteProgram(m_program);
            m_program = 0;
        }
    }

    static bool compileShader(GLuint shader, const char* src) {
        glShaderSource(shader, 1, &src, nullptr);
        glCompileShader(shader);

        GLint status = 0;
        glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
        if (status == GL_FALSE) {
            GLint length = 0;
            glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
            std::string log(length, '\0');
            glGetShaderInfoLog(shader, length, &length, log.data());
            LogError("Shader compile error: " + log);
            return false;
        }
        return true;
    }

    bool GLShader::createFromSource(const char* vertexSrc, const char* fragmentSrc) {
        destroy();

        GLuint vert = glCreateShader(GL_VERTEX_SHADER);
        GLuint frag = glCreateShader(GL_FRAGMENT_SHADER);

        if (!compileShader(vert, vertexSrc)) {
            glDeleteShader(vert);
            glDeleteShader(frag);
            return false;
        }
        if (!compileShader(frag, fragmentSrc)) {
            glDeleteShader(vert);
            glDeleteShader(frag);
            return false;
        }

        m_program = glCreateProgram();
        glAttachShader(m_program, vert);
        glAttachShader(m_program, frag);
        glLinkProgram(m_program);

        glDeleteShader(vert);
        glDeleteShader(frag);

        GLint linked = 0;
        glGetProgramiv(m_program, GL_LINK_STATUS, &linked);
        if (linked == GL_FALSE) {
            GLint length = 0;
            glGetProgramiv(m_program, GL_INFO_LOG_LENGTH, &length);
            std::string log(length, '\0');
            glGetProgramInfoLog(m_program, length, &length, log.data());
            LogError("Program link error: " + log);
            destroy();
            return false;
        }

        return true;
    }

    int GLShader::getUniformLocation(const char* name) const {
        if (m_program == 0) return -1;
        return glGetUniformLocation(m_program, name);
    }

    bool GLShader::setMat4(const char* name, const float* value) const {
        if (m_program == 0) return false;
        int loc = glGetUniformLocation(m_program, name);
        if (loc < 0) {
            LogError(std::string("Uniform not found: ") + name);
            return false;
        }
        glUniformMatrix4fv(loc, 1, GL_FALSE, value); // column-major
        return true;
    }

    void GLShader::use() const {
        if (m_program != 0) {
            glUseProgram(m_program);
        }
    }

} // namespace HBE::Renderer
