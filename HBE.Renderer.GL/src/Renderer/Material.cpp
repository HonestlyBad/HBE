#include "HBE/Renderer/Material.h"
#include "HBE/Renderer/GLShader.h"
#include "HBE/Renderer/Texture2D.h"

#include <glad/glad.h>

namespace HBE::Renderer {

    void Material::apply(const float* mvp) const {
        if (!shader) return;

        // Bind shader and set MVP
        shader->use();
        shader->setMat4("uMVP", mvp);

        // Optional color uniform (if present)
        int colorLoc = shader->getUniformLocation("uColor");
        if (colorLoc >= 0) {
            glUniform4f(colorLoc, color.r, color.g, color.b, color.a);
        }

        // Texture + sampler (unit 0)
        if (texture) {
            glActiveTexture(GL_TEXTURE0);
            texture->bind();

            int texLoc = shader->getUniformLocation("uTex");
            if (texLoc >= 0) {
                glUniform1i(texLoc, 0);
            }
        }
    }

} // namespace HBE::Renderer
