#include "HBE/Renderer/SpriteRenderer2D.h"

#include "HBE/Renderer/ResourceCache.h"
#include "HBE/Renderer/Texture2D.h"
#include "HBE/Renderer/RenderItem.h"

namespace HBE::Renderer {

    // ---------------------------
    // Sprite sheet declaration
    // ---------------------------

    SpriteRenderer2D::SpriteSheetHandle SpriteRenderer2D::declareSpriteSheet(
        ResourceCache& cache,
        const std::string& cacheName,
        const std::string& filePath,
        const SpriteSheetDesc& desc)
    {
        SpriteSheetHandle handle{};

        Texture2D* tex = cache.getOrCreateTextureFromFile(cacheName, filePath);
        if (!tex) {
            // handle.texture will be null -> caller can detect failure
            return handle;
        }

        handle.texture = tex;
        handle.texWidth = tex->getWidth();
        handle.texHeight = tex->getHeight();
        handle.desc = desc;

        return handle;
    }

    // Convert a (col,row) frame into UV rect in the RenderItem
    void SpriteRenderer2D::setFrame(
        RenderItem& item,
        const SpriteSheetHandle& sheet,
        int col,
        int row)
    {
        if (!sheet.texture || sheet.texWidth <= 0 || sheet.texHeight <= 0)
            return;

        const SpriteSheetDesc& d = sheet.desc;

        // Pixel position from top-left of original image
        int origX = d.marginX + col * (d.frameWidth + d.spacingX);
        int origY = d.marginY + row * (d.frameHeight + d.spacingY);

        // Texture is loaded flipped vertically (stbi_set_flip_vertically_on_load(1)),
        // so convert from top-left origin to bottom-left.
        int loadedY = sheet.texHeight - (origY + d.frameHeight);

        if (origX < 0)       origX = 0;
        if (loadedY < 0)     loadedY = 0;

        float u0 = static_cast<float>(origX) / static_cast<float>(sheet.texWidth);
        float v0 = static_cast<float>(loadedY) / static_cast<float>(sheet.texHeight);

        float uScale = static_cast<float>(d.frameWidth) / static_cast<float>(sheet.texWidth);
        float vScale = static_cast<float>(d.frameHeight) / static_cast<float>(sheet.texHeight);

        item.uvRect[0] = u0;
        item.uvRect[1] = v0;
        item.uvRect[2] = uScale;
        item.uvRect[3] = vScale;
    }

    // ---------------------------
    // Animator
    // ---------------------------

    const SpriteAnimationDesc* SpriteRenderer2D::Animator::getCurrentClip() const {
        if (currentClipIndex < 0 || currentClipIndex >= static_cast<int>(clips.size()))
            return nullptr;
        return &clips[currentClipIndex];
    }

    void SpriteRenderer2D::Animator::addClip(const SpriteAnimationDesc& clip) {
        clips.push_back(clip);
    }

    void SpriteRenderer2D::Animator::play(const std::string& name, bool restartIfSame) {
        int idx = -1;
        for (int i = 0; i < static_cast<int>(clips.size()); ++i) {
            if (clips[i].name == name) {
                idx = i;
                break;
            }
        }
        if (idx < 0) return; // not found

        if (!restartIfSame && idx == currentClipIndex) {
            // keep current frame, just ensure it's playing
            playing = true;
            return;
        }

        currentClipIndex = idx;
        currentFrameInClip = 0;
        frameTimer = 0.0f;
        playing = true;
    }

    void SpriteRenderer2D::Animator::stop() {
        playing = false;
    }

    void SpriteRenderer2D::Animator::update(float dt) {
        if (!playing) return;

        const SpriteAnimationDesc* clip = getCurrentClip();
        if (!clip) return;

        frameTimer += dt;
        while (frameTimer >= clip->frameDuration) {
            frameTimer -= clip->frameDuration;
            ++currentFrameInClip;

            if (currentFrameInClip >= clip->frameCount) {
                if (clip->loop) {
                    currentFrameInClip = 0;
                }
                else {
                    currentFrameInClip = clip->frameCount - 1;
                    playing = false;
                    break;
                }
            }
        }
    }

    void SpriteRenderer2D::Animator::apply(RenderItem& item) const {
        if (!sheet) return;

        const SpriteAnimationDesc* clip = getCurrentClip();
        if (!clip) return;

        int col = clip->startCol + currentFrameInClip;
        int row = clip->row;

        SpriteRenderer2D::setFrame(item, *sheet, col, row);
    }

} // namespace HBE::Renderer
