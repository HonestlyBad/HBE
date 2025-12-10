#pragma once

#include <string>
#include <cstdint>
#include <SDL3/SDL.h>

namespace HBE::Platform {

    // High-level window mode (we’ll support exclusive later if you want)
    enum class WindowMode {
        Windowed,
        FullscreenDesktop  // borderless fullscreen at desktop resolution
    };

    struct WindowConfig {
        std::string title = "Honestly Bad Engine";
        int width = 1280;
        int height = 720;

        bool useOpenGL = false;

        WindowMode mode = WindowMode::Windowed;
        bool vsync = true;
    };

    class SDLPlatform {
    public:
        SDLPlatform() = default;
        ~SDLPlatform();

        SDLPlatform(const SDLPlatform&) = delete;
        SDLPlatform& operator = (const SDLPlatform&) = delete;

        // init / shutdown
        bool initialize(const WindowConfig& config);
        void shutdown();

        // apply new graphics settings at runtime
        bool applyGraphicsSettings(const WindowConfig& config);

        // pump events, return true if user asked to quit.
        bool pollQuitRequested();

        // simple delay helper
        void delayMillis(std::uint32_t ms);

        void swapBuffers();

        SDL_Window* getWindow() const { return m_window; }
        SDL_GLContext getGLContext() const { return m_glContext; }

        // query current state
        int currentWidth()  const { return m_width; }
        int currentHeight() const { return m_height; }
        WindowMode currentMode() const { return m_mode; }

    private:
        SDL_Window* m_window = nullptr;
        SDL_GLContext m_glContext = nullptr;
        bool          m_initialized = false;

        int        m_width = 0;
        int        m_height = 0;
        WindowMode m_mode = WindowMode::Windowed;
        bool       m_vsync = true;

        bool createGLContext(const WindowConfig& config);

        // internal helpers for mode changes
        bool setWindowMode(WindowMode mode);
        bool setWindowSize(int width, int height);
        bool applyVSync(bool enabled);
    };

} // namespace HBE::Platform
