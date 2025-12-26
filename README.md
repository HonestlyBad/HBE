# Honestly Bad Engine (HBE)

**Honestly Bad Engine (HBE)** is a lightweight, modular **2D-first game engine**
built completely from scratch in **C++**, using **SDL3** and **OpenGL**.

HBE is intentionally explicit and educational — the goal is not to hide systems
behind magic, but to expose *real engine architecture* in a clean, readable way.
It serves both as a learning project and as a solid foundation for small-to-medium
games.

---

## 🚀 Current Features

### 🧠 Core Systems (`HBE.Core`)
- Application runtime & main loop
- Layer & LayerStack system (game states, overlays, UI layers)
- Logging system (Trace → Fatal)
- High-resolution timing & delta time
- Centralized ownership of platform, renderer, and resources

### 🖥 Platform Layer (`HBE.Platform.SDL`)
- SDL3 window creation & lifecycle
- OpenGL context creation
- Windowed / borderless fullscreen switching
- VSync control
- Keyboard input system (pressed / released / held)
- Event pumping & platform abstraction

### 🎨 Renderer (`HBE.Renderer` / `HBE.Renderer.GL`)
- OpenGL 3.3 Core rendering backend
- Renderer2D abstraction
- Camera2D with logical resolution support
- Mesh system (Pos+Color, Pos+UV)
- Texture2D loading (stb_image)
- Material system (shader + texture + color)
- Sprite rendering with UV rects
- Sprite sheet animation system
- Resource cache (shaders, meshes, textures)

### 🧩 Scene & Gameplay
- Scene2D entity container
- RenderItem + Transform2D model
- SpriteAnimator with named clips
- Camera follow
- Sandbox game layer example

---

## 📂 Project Structure

```
HBE/
├── HBE.Core/
│   ├── include/HBE/Core/
│   └── src/
├── HBE.Platform.SDL/
│   ├── include/HBE/Platform/
│   └── src/
├── HBE.Renderer/
│   ├── include/HBE/Renderer/
│   └── src/
├── HBE.Renderer.GL/
│   ├── include/HBE/Renderer/
│   └── src/
├── Sandbox/
│   ├── GameLayer.h / .cpp
│   └── main.cpp
└── CMakeLists.txt
```

All engine headers are included via:

```cpp
#include "HBE/..."
```

Each module exports its own `include/` directory.

---

## 🧠 Engine Architecture

### Application + Layer Stack

- `Application` owns:
  - SDLPlatform
  - GLRenderer
  - Renderer2D
  - ResourceCache
  - LayerStack

- Layers represent:
  - Game states (GameLayer, MenuLayer, PauseLayer)
  - Overlays (UI, Debug tools)

The main loop lives **entirely inside `Application`**.

`main.cpp` is intentionally minimal.

---

## 🧪 Sandbox Example

```cpp
Application app;
app.initialize(windowCfg);

app.pushLayer(std::make_unique<GameLayer>());
app.run();
```

Each layer implements:

```cpp
void onAttach(Application&);
void onUpdate(float dt);
void onRender();
```

---

## 🎬 Sprite Sheets & Animation

```cpp
SpriteSheetDesc desc;
desc.frameWidth  = 100;
desc.frameHeight = 100;

auto sheet = SpriteRenderer2D::declareSpriteSheet(
    resources,
    "orc_sheet",
    "assets/Orc.png",
    desc
);

SpriteAnimationDesc idle;
idle.name = "Idle";
idle.row = 0;
idle.startCol = 0;
idle.frameCount = 6;
idle.frameDuration = 0.15f;
idle.loop = true;

Animator animator;
animator.sheet = &sheet;
animator.addClip(idle);
animator.play("Idle");
```

Call `update(dt)` and `apply(renderItem)` each frame.

---

## 🛠 Build Instructions

### Requirements
- CMake **3.20+**
- Visual Studio **2022 / 2025** (MSVC) or Clang/GCC
- SDL3
- OpenGL 3.3+
- GLAD
- stb_image

### Build
```bash
mkdir build
cd build
cmake ..
cmake --build .
```

---

## 🗺 Roadmap

### 2D Focus
- Letterboxed logical viewport
- Sprite flipping & facing
- Tilemaps
- Physics-lite (AABB)
- UI / debug overlay
- Audio wrapper

### Future 3D
- Camera3D
- OBJ mesh loading
- Basic lighting & materials
- Transform hierarchy

---

## 🤝 Contributing

Planned contribution areas:
- Renderer improvements
- Animation & tooling
- Scene & entity systems
- Documentation & examples

This project favors **clarity over cleverness**.

---

## 📝 License

MIT License — free to use, modify, break, and rebuild.

---

## ⭐ Support

If you enjoy the project, consider starring the repository and following
development. Feedback and discussion are always welcome.
