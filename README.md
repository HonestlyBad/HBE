# Honestly Bad Engine (HBE)

A lightweight, modular 2D/3D game engine built from scratch in **C++**,
using **SDL3**, **OpenGL**, and a fully custom architecture. HBE is
designed to be clean, explicit, and educational---perfect for learning
real engine‑level development while also building a performant sandbox
for your own games.

------------------------------------------------------------------------

## 🚀 Features (So Far)

### 🖥 Platform Layer (HBE.Platform)

-   SDL3 window creation & management\
-   GL context setup\
-   Input system (keyboard, mouse, scancodes)\
-   Time utilities & delta‑time\
-   Platform abstraction for cross‑compatibility

### 🎨 Renderer (HBE.Renderer)

-   OpenGL‑based rendering pipeline\
-   Texture2D loading & caching\
-   Mesh & RenderItem batching\
-   Camera2D\
-   Sprite Renderer (2D)\
-   Animation system (sprite sheet frame selection with timing)\
-   Basic shader system (GLShader)\
-   Resource cache for textures/materials

### 🧩 Core Systems (HBE.Core)

-   Logging system (Trace → Error)\
-   Time management\
-   Utility helpers\
-   Math structures for transforms

------------------------------------------------------------------------

## 📂 Project Structure

    HBE/
     ├── Core/
     │    ├── Log.h / .cpp
     │    ├── Time.h / .cpp
     │    └── ...
     ├── Platform/
     │    ├── SDLPlatform.h / .cpp
     │    ├── Input.h / .cpp
     │    └── ...
     ├── Renderer/
     │    ├── GLRenderer.h / .cpp
     │    ├── Renderer2D.h / .cpp
     │    ├── Mesh.h / .cpp
     │    ├── Texture2D.h / .cpp
     │    ├── Material.h / .cpp
     │    ├── ResourceCache.h / .cpp
     │    └── ...
     ├── Sandbox/
     │    └── main.cpp
     └── CMakeLists.txt

------------------------------------------------------------------------

## 🧠 Vision for the Engine

The long‑term direction for HBE includes:

### ✔ 2D Engine Goals

-   Physics lite system\
-   Tilemap support\
-   UI system\
-   Audio wrapper

### ✔ 3D Future Goals

-   Basic GL mesh loading (OBJ first)\
-   Camera3D & transforms\
-   Lighting & shading basics

------------------------------------------------------------------------

## 🛠 Build Instructions

### 1️⃣ Requirements

-   **CMake 3.20+**
-   **Visual Studio 2022/2025** or Clang/GCC\
-   **SDL3**, **SDL3_image**, **SDL3_ttf**
-   **GLAD** or equivalent OpenGL loader

### 2️⃣ Configure Project

    mkdir build
    cd build
    cmake ..

### 3️⃣ Build

    cmake --build .

------------------------------------------------------------------------

## 🧪 Sandbox Example

Your `main.cpp` typically initializes:

``` cpp
SDLPlatform platform;
platform.initialize(windowCfg);

GLRenderer renderer;
Renderer2D::Init();

Scene2D scene;
Camera2D camera;

while (!platform.pollQuitRequested()) {
    float dt = Time::deltaTime();
    scene.update(dt);
    renderer.render(scene, camera);
    platform.swapBuffers();
}
```

------------------------------------------------------------------------

## 🎬 Sprite Rendering + Animation

``` cpp
auto sheet = SpriteRenderer2D::declareSpriteSheet(
    "assets/player.png",
    frameWidth,
    frameHeight,
    imageW,
    imageH
);

Animation idle(sheet, 0, 3, 0, 0.12f);
Animation walk(sheet, 0, 5, 1, 0.08f);
```

Just call `animation.update(dt)` every frame and draw the correct source
rect.

------------------------------------------------------------------------

## 🤝 Contributing

Planned contribution categories:

-   Renderer improvements\
-   Animation system expansion\
-   ECS refactor (future)\
-   Documentation & examples\
-   Platform backends (Win32, Linux, etc.)

------------------------------------------------------------------------

## 📝 License

MIT License --- free to use, modify, break, and rebuild.

------------------------------------------------------------------------

## ⭐ Support the Project

If you like the engine or want more features, star the repo on GitHub
and share feedback!
