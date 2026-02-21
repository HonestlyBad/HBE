# HBE --- Honestly Bad Engine

HBE is a modern **2D game engine built in C++** focused on learning,
extensibility, and clean architecture.\
It combines an OpenGL renderer, SDL platform layer, and a custom ECS
gameplay framework to provide a solid foundation for real games --- not
just demos.

This project is actively developed as both a learning platform and a
long‑term engine architecture experiment.

------------------------------------------------------------------------

## ✨ Current Features

### Rendering

-   OpenGL‑based 2D renderer
-   Sprite batching
-   Sprite sheets + UV animation
-   Tilemap rendering
-   Text rendering with SDF fonts
-   Debug drawing tools
-   Layer + Y‑sorted rendering

### ECS Gameplay Framework

-   Registry‑based ECS (entt‑style design)
-   Components:
    -   Transform2D
    -   SpriteComponent2D
    -   AnimationComponent2D
    -   Collider2D
    -   RigidBody2D
    -   Script
-   Systems:
    -   Script execution
    -   Physics integration
    -   Tilemap collision
    -   Entity‑entity collision
    -   Animation playback
    -   Render sorting

Gameplay is now fully ECS‑driven.

### Animation System

-   Clip‑based animations
-   State machines
-   Parameter‑driven transitions
-   Animation events (footsteps, hit frames, etc.)

### Tilemaps

-   JSON tilemap loading
-   Multiple layers
-   Dedicated collision layers
-   Integrated physics resolution

### UI Framework

-   Immediate‑mode style UI
-   Panels, sliders, buttons, checkboxes
-   Debug overlays

------------------------------------------------------------------------

## 🧠 Engine Philosophy

HBE is designed around a simple rule:

> Rendering is not the engine --- gameplay architecture is.

The engine is built so that:

-   Rendering is modular
-   Gameplay lives in ECS
-   Systems define behavior
-   Components define data

This makes future features easier to add:

-   AI
-   particles
-   audio triggers
-   save/load
-   networking
-   editor tools
-   prefabs

------------------------------------------------------------------------

## 🏗️ Project Structure

    HBE.Platform.SDL/        → windowing, input, platform layer
    HBE.Renderer.GL/         → OpenGL renderer + resources
    HBE.Core/                → engine framework + layer system
    HBE.Sandbox/             → test game + engine showcase
    assets/                  → maps, sprites, fonts

The sandbox project demonstrates how to build a game using the engine.

------------------------------------------------------------------------

## 🛠️ Building

### Requirements

-   Windows
-   Visual Studio 2022+
-   OpenGL 3.3+ capable GPU

### Steps

1.  Open the solution in Visual Studio
2.  Set **HBE.Sandbox** as startup project
3.  Build (x64 Debug or Release)
4.  Run

The sandbox scene should load automatically.

------------------------------------------------------------------------

## 🎮 Current Gameplay Demo

The sandbox demonstrates:

-   Player movement via ECS scripts
-   Tilemap collision
-   NPC collision
-   Sprite animation state machines
-   Animation event popups
-   UI overlay controls
-   Debug rendering

This serves as the reference implementation for engine usage.

------------------------------------------------------------------------

## 🚧 Roadmap

### Near‑Term

-   Health / damage components
-   Attack hitboxes
-   AI movement system
-   Event messaging framework
-   Prefab spawning system

### Mid‑Term

-   Editor tooling
-   Save/load serialization
-   Scene format
-   Audio system

### Long‑Term

-   Multi‑scene workflows
-   Networking model
-   Full game project built on HBE

------------------------------------------------------------------------

## 📌 Status

The engine now has a complete gameplay loop:

    Script → Physics → Collision → Animation → Render

This marks the transition from "rendering framework" to **actual game
engine**.

------------------------------------------------------------------------

## 🙌 Author

Albert Tulo IV\
GitHub: HonestlyBad

This engine is part of a broader effort to build a reusable engine
ecosystem and real production‑ready tooling.

------------------------------------------------------------------------

## 📄 License

Currently private / experimental.\
License will be defined when the engine stabilizes.
