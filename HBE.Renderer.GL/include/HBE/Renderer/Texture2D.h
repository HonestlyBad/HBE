#pragma once

#include <string>

namespace HBE::Renderer {

    class Texture2D {
    public:
        Texture2D() = default;
        ~Texture2D();

        int getWidth() const { return m_width; }
        int getHeight() const { return m_height; }

        Texture2D(const Texture2D&) = delete;
        Texture2D& operator=(const Texture2D&) = delete;

        // Create simple checkerboard procedural texture
        bool createChecker(int width = 128, int height = 128);

        // load from image file (PNG, JPG, etc.)
        bool loadFromFile(const std::string& path);
        bool createFromRGBA(int width, int height, const unsigned char* rgbaPixels);


        void bind() const;

        unsigned int getID() const { return m_id; }

    private:
        unsigned int m_id = 0;

        int m_width = 0;
        int m_height = 0;

        void destroy();
    };

} // namespace HBE::Renderer
