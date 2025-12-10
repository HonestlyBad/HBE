#pragma once

#include <string>
#include <vector>

namespace HBE::Renderer {

    class Texture2D;
    class ResourceCache;
    struct RenderItem;

    // How the sheet is laid out
    struct SpriteSheetDesc {
        int frameWidth = 0;
        int frameHeight = 0;
        int marginX = 0;
        int marginY = 0;
        int spacingX = 0;
        int spacingY = 0;
    };

    // Description of a single animation clip on the sheet
    struct SpriteAnimationDesc {
        std::string name;

        int row = 0;   // which row on the sheet
        int startCol = 0;   // first column (inclusive)
        int frameCount = 1;   // number of frames along that row

        float frameDuration = 0.1f; // seconds per frame
        bool  loop = true;
    };

    class SpriteRenderer2D {
    public:
        // Handle returned when you "declare" a sprite sheet
        struct SpriteSheetHandle {
            Texture2D* texture = nullptr;

            int texWidth = 0;
            int texHeight = 0;

            SpriteSheetDesc desc;
        };

        // Load / cache the texture and return a handle with size + layout info
        static SpriteSheetHandle declareSpriteSheet(
            ResourceCache& cache,
            const std::string& cacheName,   // name in ResourceCache
            const std::string& filePath,    // disk path to PNG
            const SpriteSheetDesc& desc
        );

        // Set the UV rectangle of a RenderItem to a given (col,row) frame
        static void setFrame(
            RenderItem& item,
            const SpriteSheetHandle& sheet,
            int col,
            int row
        );

        // Per-sprite animator that uses the sheet handle
        class Animator {
        public:
            const SpriteSheetHandle* sheet = nullptr;

            std::vector<SpriteAnimationDesc> clips;
            int   currentClipIndex = -1;
            int   currentFrameInClip = 0;
            float frameTimer = 0.0f;
            bool  playing = false;

            void addClip(const SpriteAnimationDesc& clip);
            void play(const std::string& name, bool restartIfSame = false);
            void stop();
            void update(float dt);
            void apply(RenderItem& item) const;

        private:
            const SpriteAnimationDesc* getCurrentClip() const;
        };
    };

} // namespace HBE::Renderer
