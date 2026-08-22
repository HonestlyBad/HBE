# HBE Game Engine — API Reference

A living reference for every public endpoint and feature exposed by the
**Honestly Bad Engine** projects that a game project (like **MegaX**)
can consume. Excludes `HBE.Sandbox` (the engine's own dev scratchpad)
and `HBMapMaker` (the tilemap editor tool) — this doc is only for the
reusable engine surface.

Generated: 2026-08-03 by scanning every `.h` under each project's
`include/` directory, cross-checked against `.cpp` implementation
files.

Updated: 2026-08-15 for work items 13 (`HBE/Core/Profiler.h`) and 14
(`HBE/Renderer/GpuTimer.h`, expanded `Renderer2DStats`). Everything
else is still as of the 2026-08-03 scan — when in doubt, the header is
the source of truth.

---

## 0. Project layout

The engine is split across three static libraries + two tools:

| Project | Role | Depends on |
|---|---|---|
| `HBE.Core`         | Foundations: app lifecycle, layers, events, ECS, input map, logging, time, UUID, filesystem, file watcher, combat components/system, CPU profiler | (none) |
| `HBE.Platform.SDL` | Window/OS integration: SDL3 window, GL context, low-level input, audio (SDL_mixer), graphics settings | `HBE.Core` |
| `HBE.Renderer.GL`  | 2D OpenGL renderer: cameras, sprites, tilemaps, particles, text/fonts, post-process, UI, scene serializer, GPU timer queries | `HBE.Core`, `HBE.Platform.SDL` |
| `HBE.Sandbox` *(excluded)* | Engine dev scratchpad game | all |
| `HBMapMaker` *(excluded)* | Tilemap editor tool | all |

Game projects (like `MegaX`) link against all three engine libs and
consume the API surface documented here.

**Namespace convention**

* `HBE::Core` — foundations
* `HBE::ECS`  — entity-component-system (lives in `HBE.Core`)
* `HBE::Events` — gameplay event payloads (lives in `HBE.Core`)
* `HBE::Input` — high-level input map (lives in `HBE.Core`)
* `HBE::Platform` — SDL platform layer
* `HBE::Renderer` — GL renderer
* `HBE::Renderer::UI` — immediate-mode UI

---

## Table of contents

1. [HBE.Core — App lifecycle & foundations](#1-hbecore--app-lifecycle--foundations)
2. [HBE.Core — Input Map](#2-hbecore--input-map)
3. [HBE.Core — ECS](#3-hbecore--ecs)
4. [HBE.Core — Combat components & system](#4-hbecore--combat-components--system)
5. [HBE.Core — Gameplay events](#5-hbecore--gameplay-events)
6. [HBE.Platform.SDL — Window, input, audio, settings](#6-hbeplatformsdl--window-input-audio-settings)
7. [HBE.Renderer.GL — Rendering primitives](#7-hbereendergl--rendering-primitives)
8. [HBE.Renderer.GL — Cameras & controllers](#8-hbereendergl--cameras--controllers)
9. [HBE.Renderer.GL — Sprites, text, debug draw](#9-hbereendergl--sprites-text-debug-draw)
10. [HBE.Renderer.GL — Tilemaps & collision](#10-hbereendergl--tilemaps--collision)
11. [HBE.Renderer.GL — Particles](#11-hbereendergl--particles)
12. [HBE.Renderer.GL — Scene, ECS components, serialization](#12-hbereendergl--scene-ecs-components-serialization)
13. [HBE.Renderer.GL — Post-process stack](#13-hbereendergl--post-process-stack)
14. [HBE.Renderer.GL — Immediate-mode UI](#14-hbereendergl--immediate-mode-ui)
15. [Feature summary — what the engine can do today](#15-feature-summary--what-the-engine-can-do-today)

---

## 1. HBE.Core — App lifecycle & foundations

### `HBE/Core/Application.h`
**Namespace:** `HBE::Core`
Main engine wrapper. Owns platform, audio, GL renderer, 2D renderer,
resources, and layer stack. Handles init/run, letterboxing, coordinate
conversion.

#### `class Application`
```cpp
Application();
~Application();

bool initialize(const HBE::Platform::WindowConfig& windowCfg,
                const HBE::Core::AssetPaths::Config& assetCfg = {});
void run();
void requestQuit();

void pushLayer(std::unique_ptr<Layer> layer);
void pushOverlay(std::unique_ptr<Layer> overlay);

void setLogicalSize(int w, int h);

// Coordinate conversion (window pixels <-> logical render space).
bool screenToLogical(int screenX, int screenY, Vec2& outLogical) const;
Vec2 logicalToScreen(float logicalX, float logicalY) const;

// Subsystem access.
HBE::Platform::SDLPlatform&    platform();
HBE::Platform::Audio&          audio();
HBE::Renderer::GLRenderer&     gl();
HBE::Renderer::Renderer2D&     renderer2D();
HBE::Renderer::ResourceCache&  resources();

const std::filesystem::path&   assetRoot() const;
const std::filesystem::path&   userDataRoot() const;

int windowWidthPixels() const;   int windowHeightPixels() const;
int viewportX() const;  int viewportY() const;
int viewportW() const;  int viewportH() const;

struct Vec2 { float x; float y; };
```
Call `initialize(...)` once at startup, then `pushLayer` game layers,
then `run()`. `requestQuit()` unwinds the main loop cleanly.
`screenToLogical`/`logicalToScreen` convert between raw window pixels
and the letterboxed logical render space.

---

### `HBE/Core/AssetPaths.h`
**Namespace:** `HBE::Core::AssetPaths`
Resolves engine asset/user-data roots. Converts logical paths to
absolute paths.

```cpp
struct Config {
    std::string organization    = "HBE";
    std::string application     = "HonestlyBadEngine";
    std::string forceAssetRoot;      // override auto-detect
    std::string forceUserDataRoot;   // override auto-detect
    std::string assetFolderName = "assets";
    std::vector<std::string> siblingProjectNames = { "HBE.Sandbox" };
    int maxParentWalk = 5;           // how far up to search for /assets
};

bool Initialize(const Config& cfg = {});
bool IsInitialized();

const std::filesystem::path& AssetRoot();
const std::filesystem::path& UserDataRoot();
std::string AssetRootString();
std::string UserDataRootString();

std::string Resolve(std::string_view logicalRelPath);
std::string ResolveUser(std::string_view logicalRelPath);
std::string ResolveRelativeTo(std::string_view baseFilePath,
                              std::string_view embeddedRelPath);

bool Exists(std::string_view logicalRelPath);
```
`Application::initialize` calls `AssetPaths::Initialize` for you. Use
`Resolve("shaders/sprite.frag")` in game code to get an absolute path;
`ResolveUser("saves/slot1.json")` writes to the OS user-data folder.

---

### `HBE/Core/Event.h` and `HBE/Core/EventBus.h`
**Namespace:** `HBE::Core`
Small event hierarchy plus a type-safe pub/sub bus with queued
dispatch and RAII subscriptions.

#### Event hierarchy
```cpp
enum class EventType {
    None, WindowResize, KeyPressed, TextInput,
    MouseMoved, MouseButtonPressed, MouseButtonReleased, MouseScrolled
};

class Event { public: virtual ~Event(); virtual EventType type() const; bool handled; };

class WindowResizeEvent        : public Event { int width, height, vpX, vpY, vpW, vpH; };
class KeyPressedEvent          : public Event { int keyScancode; bool repeat; };
class TextInputEvent           : public Event { std::string text; };
class MouseMovedEvent          : public Event { float x, y, dx, dy, logicalX, logicalY; bool inViewport; };
class MouseButtonPressedEvent  : public Event { int button, clicks; float x, y, logicalX, logicalY; bool inViewport; };
class MouseButtonReleasedEvent : public Event { int button; float x, y, logicalX, logicalY; bool inViewport; };
class MouseScrolledEvent       : public Event { float wheelX, wheelY, mouseX, mouseY, logicalX, logicalY; bool inViewport; };
```
These flow through `Layer::onEvent(Event&)`. Set `e.handled = true` to
stop propagation. All mouse events carry both screen and logical
coordinates.

#### `class EventBus` — publish/subscribe
```cpp
template <class TEvent>
EventSubscription subscribe(std::function<void(const TEvent&)> handler);

void unsubscribe(const EventSubscription& sub);

template <class TEvent> void publish(const TEvent& evt);   // immediate
template <class TEvent> void enqueue(const TEvent& evt);   // deferred

void drain();                       // flush queued events
std::size_t subscriberCount(std::type_index t) const;
void clear();
```

#### `class ScopedSubscription` — RAII wrapper
```cpp
ScopedSubscription(EventBus& bus, EventSubscription sub);
~ScopedSubscription();
void reset();
bool valid() const;
```
`Scene2D::eventBus()` gives you the scene's event bus; publish
`Events::DamageEvent`, etc. from combat/game code and subscribe from
UI/audio/particle systems.

---

### `HBE/Core/FileWatcher.h`
**Namespace:** `HBE::Core`
Polling file watcher for dev hot-reload workflows.

```cpp
class FileWatcher {
public:
    using Callback = std::function<void(const std::string& path)>;

    struct Options {
        float pollIntervalSeconds = 0.25f;
        float debounceSeconds     = 0.25f;
    };

    FileWatcher();
    explicit FileWatcher(const Options& opt);
    void setOptions(const Options& opt);
    const Options& options() const;

    void watchFile(const std::string& path, Callback cb);
    void unwatchFile(const std::string& path);
    void clear();

    void poll(float dtSeconds);   // call from onUpdate; accumulates dt internally
};
```
Used by MegaX Item 11 (scene hot reload) to auto-reload
`level_01.json` and the sprite shader on save.

---

### `HBE/Core/Layer.h` and `HBE/Core/LayerStack.h`
**Namespace:** `HBE::Core`

#### `class Layer` — game layer base
```cpp
virtual void onAttach(Application& app);
virtual void onDetach();
virtual void onUpdate(float dt);
virtual void onRender();
virtual bool onEvent(Event& e);          // return true if handled
```

#### `class LayerStack` — owned by `Application`
```cpp
void pushLayer(std::unique_ptr<Layer> layer, Application& app);   // regular game layer
void pushOverlay(std::unique_ptr<Layer> overlay, Application& app); // sits above layers
void popLayer(Layer* layer);
void popOverlay(Layer* overlay);
void clear();
void dispatchEvent(Event& e);
```
Layers get `update` + `render` + `onEvent` in insertion order;
overlays render on top. `GameLayer` in MegaX is the sole regular layer.

---

### `HBE/Core/Log.h`
**Namespace:** `HBE::Core`
```cpp
enum class LogLevel { Trace, Info, Warn, Error, Fatal };

void SetLogLevel(LogLevel level);
void Log(LogLevel level, std::string_view message);

void LogTrace(std::string_view message);
void LogInfo (std::string_view message);
void LogWarn (std::string_view message);   // NOT LogWarning
void LogError(std::string_view message);
void LogFatal(std::string_view message);
```
No macros; free functions only. Use `LogWarn`, not `LogWarning`.

---

### `HBE/Core/Profiler.h`
**Namespace:** `HBE::Core::Profiler` — *added in work item 13, extended
in item 14.*

Deterministic scope timing for named CPU sections, plus the publication
point for GPU timings and renderer statistics. Main thread only.
`Application::run` already brackets every frame, so a game only needs
`HBE_PROFILE_SCOPE` and `GetSnapshot`.

```cpp
// Compile-time gate. Gated on NDEBUG (portable), NOT MSVC's _DEBUG:
//   Debug -> 1, Release/RelWithDebInfo/MinSizeRel -> 0.
// Define HBE_PROFILE_ENABLED before including to override.
#ifndef HBE_PROFILE_ENABLED
    #if defined(NDEBUG)
        #define HBE_PROFILE_ENABLED 0
    #else
        #define HBE_PROFILE_ENABLED 1
    #endif
#endif

static constexpr std::size_t kRollingWindowFrames = 120;  // ~2s at 60 FPS
static constexpr std::size_t kMaxSections         = 64;
static constexpr std::size_t kMaxScopesPerFrame   = 512;

struct Sample {
    const char* name           = nullptr;  // pointer identity, no strcmp
    int         depth          = 0;        // nesting depth when opened
    double      currentMs      = 0.0;      // sum of all opens this frame
    double      avgMs          = 0.0;      // over the rolling window
    double      minMs          = 0.0;
    double      maxMs          = 0.0;
    std::size_t sampleCount    = 0;        // <= kRollingWindowFrames
    std::size_t opensThisFrame = 0;
};

struct RendererStats {              // item 14
    int drawCalls = 0, passes = 0;
    int submittedQuads = 0, renderedQuads = 0, culledSprites = 0;
    int materialChanges = 0, textureChanges = 0;
    int visibleTileChunks = 0, postProcessPasses = 0;
    int activeLights = 0, shadowCastingLights = 0;
    int liveParticles = 0;
};

struct GpuTimings {                 // item 14
    bool                supported        = false;
    double              frameMs          = 0.0;
    double              frameAvgMs       = 0.0;
    double              frameMinMs       = 0.0;
    double              frameMaxMs       = 0.0;
    std::size_t         frameSampleCount = 0;
    std::vector<Sample> sections;
};

struct Snapshot {
    double              frameMs = 0.0, frameAvgMs = 0.0;
    double              frameMinMs = 0.0, frameMaxMs = 0.0;
    std::size_t         frameSampleCount = 0;
    std::uint64_t       frameIndex = 0;
    std::vector<Sample> sections;   // stable first-seen order, with depth
    GpuTimings          gpu;
    RendererStats       renderer;
};

// Lifecycle — Application::run calls both; a game normally does not.
void BeginFrame();
void EndFrame();

void SetEnabled(bool on);   // runtime toggle; cannot re-enable a
bool IsEnabled();           // compile-time-disabled build
void Reset();               // clear rings + snapshot (e.g. after F5)

const Snapshot& GetSnapshot();   // ref into internal storage — copy it
                                 // if you need it past the next EndFrame
std::uint64_t   NowNs();

// Used by the RAII wrapper; EndScope tolerates -1.
int  BeginScope(const char* name);
void EndScope(int index);

// Publication points — called by the renderer and by game code.
void SetGpuSupported(bool supported);
void PublishGpuSection(const char* name, std::uint64_t ns, int depth);
void PublishGpuFrame(std::uint64_t ns);
void PublishRendererStats(const RendererStats& stats);
void PublishLightStats(int activeLights, int shadowCastingLights);
void PublishParticleStats(int liveParticles);

class ScopeTimer {          // copy AND move deleted
public:
    explicit ScopeTimer(const char* name) noexcept;
    ~ScopeTimer() noexcept;
};
```

Macros — `HBE_PROFILE_SCOPE` expands to `((void)0)` when disabled, so
there is genuinely nothing left in Release:

```cpp
HBE_PROFILE_SCOPE("SceneUpdate");   // RAII, closes at end of scope
HBE_PROFILE_BEGIN_FRAME();
HBE_PROFILE_END_FRAME();
```

Notes:

* Nesting comes from **scope lifetime**, not from a parent string —
  never pass a parent name.
* Names are compared by `const char*` identity, so pass string
  literals. Zero allocations per scope.
* `PublishRendererStats` preserves the previous frame's
  `visibleTileChunks` / `postProcessPasses` when the incoming values
  are 0, so a publisher that doesn't know them can leave them alone.
* Reachable from any layer without a new include — `Application.h`
  includes `Profiler.h`.

`Application::run` publishes a 1-Hz summary to the log in Debug,
guarded by `HBE_PROFILER_LOG_ONCE_PER_SECOND`.

---

### `HBE/Core/Time.h`
```cpp
double        GetTimeSeconds();   // relative to engine start
std::uint64_t GetTimeMillis();
```

---

### `HBE/Core/Types.h`
```cpp
using i8  = std::int8_t;   using i16 = std::int16_t;
using i32 = std::int32_t;  using i64 = std::int64_t;
using u8  = std::uint8_t;  using u16 = std::uint16_t;
using u32 = std::uint32_t; using u64 = std::uint64_t;
using f32 = float;         using f64 = double;
```

---

### `HBE/Core/UUID.h`
```cpp
std::string NewUUID32();   // 32-hex-character UUID, no dashes
```

---

## 2. HBE.Core — Input Map

### `HBE/Input/InputMap.h`
**Namespace:** `HBE::Input`
High-level game action/axis binding layer over SDL. Bind semantic
"Jump" / "MoveX" to keyboard / mouse / gamepad, save/load to a file,
support runtime rebinding.

#### Enums
```cpp
enum class Action : std::uint16_t {
    Jump, Attack, UIConfirm, UICancel, Pause, FullscreenToggle
};

enum class Axis : std::uint16_t { MoveX, MoveY };
```

#### `struct Binding` — one input source
```cpp
struct Binding {
    enum class Type : std::uint8_t {
        None, Key, MouseButton, GamepadButton, GamepadAxisThreshold
    };
    Type type;
    SDL_Scancode        key;
    int                 mouseButton;
    SDL_GamepadButton   padButton;
    SDL_GamepadAxis     padAxis;
    float               axisThreshold;
    int                 axisSign;

    static Binding None();
    static Binding Key(SDL_Scancode sc);
    static Binding Mouse(int button);
    static Binding GamepadButton(SDL_GamepadButton btn);
    static Binding GamepadAxis(SDL_GamepadAxis axis, float threshold = 0.5f, int sign = 0);
};
```

#### `struct ActionBinding` / `struct AxisBinding`
```cpp
struct ActionBinding { Binding primary; Binding secondary; };

struct AxisBinding {
    Binding negative;   Binding positive;
    Binding negative2;  Binding positive2;
    bool useGamepadAxis = false;
    SDL_GamepadAxis gamepadAxis = SDL_GAMEPAD_AXIS_INVALID;
    float deadzone = 0.20f;
    bool  invert   = false;
    float scale    = 1.0f;
};
```

#### `class InputMap`
```cpp
void clear();
void newFrame();
void handleEvent(const SDL_Event& e);

bool  actionDown    (Action a) const;
bool  actionPressed (Action a) const;
bool  actionReleased(Action a) const;
float axis          (Axis a)   const;

void beginRebind(Action a, bool primary = true);
void cancelRebind();
bool isRebinding() const;

void bindAction(Action a, const Binding& b, bool primary = true);
void bindAxis  (Axis   a, const AxisBinding& b);
const ActionBinding& getActionBinding(Action a) const;
const AxisBinding&   getAxisBinding  (Axis a)   const;

bool saveToFile(const std::string& path) const;
bool loadFromFile(const std::string& path);

static const char* actionName(Action a);
static const char* axisName  (Axis   a);
```

#### Global convenience API
```cpp
using DefaultBindingsFn = void(*)(InputMap& map);

void Initialize(DefaultBindingsFn defaultsProvider);   // set defaults at startup
void Shutdown();
void NewFrame();
void HandleEvent(const SDL_Event& e);

bool  ActionDown    (Action a);
bool  ActionPressed (Action a);
bool  ActionReleased(Action a);
float AxisValue     (Axis a);

void BeginRebind(Action a, bool primary = true);
void CancelRebind();
bool IsRebinding();
InputMap& Get();
```
Higher-level than `HBE::Platform::Input`: prefer `InputMap` for
gameplay ("did the player press Jump?"), `HBE::Platform::Input` for
raw scancode polling (dev/debug hotkeys like F5/F6/F7).

---

## 3. HBE.Core — ECS

### `HBE/ECS/Entity.h`
```cpp
using Entity = std::uint32_t;
inline constexpr Entity Null = 0;   // reserved sentinel
```

### `HBE/ECS/Registry.h`
**Namespace:** `HBE::ECS`
Sparse-set ECS registry.

#### `class Registry`
```cpp
Entity create();
void   destroy(Entity e);
bool   valid(Entity e) const;

template<typename T> bool  has     (Entity e) const;
template<typename T> T*    tryGet  (Entity e);
template<typename T> T&    get     (Entity e);         // asserts
template<typename T> const T& get  (Entity e) const;

template<typename T, typename... Args>
T& emplace(Entity e, Args&&... args);                   // construct/replace

template<typename T> void  remove  (Entity e);

template<typename... Components>
View<Components...> view();                             // iterate matching entities

// Escape hatches for advanced users.
template<typename T> Storage<T>*       tryStorage();
template<typename T> const Storage<T>* tryStorage() const;
template<typename T> Storage<T>*       getOrCreateStorage();
```

#### `class View<Components...>` — range for-loop iteration
```cpp
for (Entity e : reg.view<Transform2D, RigidBody2D>()) { /* ... */ }
```
Iterates entities that have every listed component. Chooses the
smallest storage as the driver for tight loops.

#### `struct IStorage` / `class Storage<T>` — component storage
```cpp
struct IStorage {
    virtual void onEntityDestroyed(Entity e) = 0;
    virtual bool has(Entity e) const = 0;
    virtual std::size_t size() const = 0;
};

template<typename T> class Storage : public IStorage {
    T& get(Entity e);          const T& get(Entity e) const;
    template<typename... Args> T& emplace(Entity e, Args&&... args);
    void remove(Entity e);
    const std::vector<Entity>& denseEntities() const;
};
```

---

### `HBE/ECS/Components.h`
**Namespace:** `HBE::ECS`
General-purpose ECS components.

#### `struct Collider2D`
```cpp
float halfW = 0.5f, halfH = 0.5f;
float offsetX = 0.0f, offsetY = 0.0f;
bool  isTrigger = false;
```

#### `struct RigidBody2D`
```cpp
float velX = 0.0f, velY = 0.0f;
float accelX = 0.0f, accelY = 0.0f;
float linearDamping = 0.0f;
bool  isStatic = false;
bool  useGravity = false;
float gravityScale = 1.0f;
bool  grounded = false;
float maxStepUp = 0.0f;
bool  enableOneWay = true;
float oneWayDisableTimer = 0.0f;
bool  enableSlopes = true;
float maxFallSpeed = 0.0f;
```
Lightweight platformer physics state consumed by `Scene2D::update`.

#### `struct Script`
```cpp
std::string                          name;
std::function<void(Entity)>          onCreate;
std::function<void(Entity, float)>   onUpdate;
```
Attach behaviour to any entity without inheritance.

---

### `HBE/ECS/RuntimeComponents.h`
**Namespace:** `HBE::ECS`
Bookkeeping metadata.

```cpp
struct ScriptRuntimeState  { bool created = false; };
struct IDComponent         { std::string uuid;    };  // stable persistence ID
struct TagComponent        { std::string tag;     };  // human-readable label
struct AnimatorPresetComponent { std::string preset; }; // name into AnimationPresetRegistry
```

---

## 4. HBE.Core — Combat components & system

### `HBE/ECS/CombatComponents.h`
**Namespace:** `HBE::ECS`
Turn-key combat via components.

#### `enum class Faction : std::uint8_t`
```cpp
enum class Faction : std::uint8_t {
    Neutral, Player, Enemy, Environment, _Count
};
```

#### `struct FactionComponent`
```cpp
Faction team = Faction::Neutral;
```

#### `struct Health`
```cpp
int   hp = 3;
int   maxHp = 3;
float invulnTimer = 0.0f;
float deathTimer  = 0.0f;
bool  dead = false;
```

#### `struct Hurtbox`
```cpp
float halfW = 0.0f, halfH = 0.0f;
float offsetX = 0.0f, offsetY = 0.0f;
bool  active = true;
```

#### `struct Hitbox`
```cpp
Entity owner = Null;
float  halfW = 0.5f, halfH = 0.5f;
float  offsetX = 0.0f, offsetY = 0.0f;
int    damage = 1;
float  lifetime = 0.1f;
bool   follows  = true;                          // sticks to owner transform
bool   active   = true;
std::array<bool, Faction::_Count> canHit{ /*...*/ };  // per-faction mask
Faction attackerFaction = Faction::Neutral;
bool   friendlyFire = false;
float  knockbackX = 0.0f, knockbackY = 0.0f;
bool   knockbackHorizontalUsesFacing = true;
float  invulnAfterHit = 0.35f;
std::vector<Entity> alreadyHit;                  // dedup per hitbox
```

#### `struct Knockback`
```cpp
float velX = 0.0f, velY = 0.0f;                  // accumulated knockback impulse
```

---

### `HBE/ECS/CombatEvents.h`
**Namespace:** `HBE::ECS`
Preset-driven hitbox spawning.

#### `struct HitboxPreset`
Same fields as `Hitbox` (minus `owner`, `active`, `alreadyHit`); use
as a template for repeat attacks.

#### `class HitboxPresetRegistry`
```cpp
void registerPreset(const std::string& name, const HitboxPreset& p);
const HitboxPreset* get(const std::string& name) const;
bool has(const std::string& name) const;
std::size_t size();
```

#### Free functions
```cpp
Entity spawnHitbox(HBE::Renderer::Scene2D& scene,
                   Entity attacker,
                   const HitboxPreset& preset);

Entity spawnHitboxByName(HBE::Renderer::Scene2D& scene,
                         Entity attacker,
                         const HitboxPresetRegistry& registry,
                         const std::string& presetName);
```
`spawnHitboxByName` logs a warning and returns `Null` if the preset
name isn't found.

---

### `HBE/ECS/CombatSystem.h`
**Namespace:** `HBE::ECS`
```cpp
void updateCombat(Registry& reg,
                  float dt,
                  HBE::Core::EventBus* bus = nullptr);
```
Advances `Health.invulnTimer`, expires `Hitbox` after `lifetime`,
resolves hitbox-vs-hurtbox overlaps, applies `Knockback`, publishes
`DamageEvent` / `KnockbackAppliedEvent` / `DeathEvent` (if `bus`
supplied).

Falls back to `Collider2D` when `Hurtbox` is missing on a target.

---

## 5. HBE.Core — Gameplay events

### `HBE/Events/GameplayEvents.h`
**Namespace:** `HBE::Events`
Payloads for `EventBus`. Publish/subscribe to these to decouple
combat/audio/particle/UI systems.

```cpp
struct DamageEvent {
    Entity attacker, victim;
    int damage, hpBefore, hpAfter;
    float victimX, victimY;
};

struct DeathEvent {
    Entity victim, killedBy;
    float victimX, victimY;
};

struct KnockbackAppliedEvent {
    Entity victim;
    float velX, velY;
};

struct AnimationEvent {
    Entity entity;
    std::string name;
    std::string stateName;
    int frameIndex;
};

struct AudioEvent {
    std::string soundKey;
    float worldX, worldY, gain;
    bool positional;
};

struct ParticleEvent {
    std::string emitterKey;
    float worldX, worldY, intensity;
};

struct GameStateChangedEvent {
    enum class State : std::uint8_t { Playing = 0, Won = 1, Lost = 2 };
    State previous, current;
};

struct SceneChangedEvent {
    std::string scenePath;
    bool loaded;
};

struct InteractionEvent {
    Entity actor, target;
    std::string tags;
};

struct CollisionEvent {
    Entity a, b;
    float normalX, normalY;
};
```

---

## 6. HBE.Platform.SDL — Window, input, audio, settings

### `HBE/Platform/SDLPlatform.h`
**Namespace:** `HBE::Platform`
Owns the SDL window and optional GL context.

```cpp
enum class WindowMode { Windowed, FullscreenDesktop };

struct WindowConfig {
    std::string title = "Honestly Bad Engine";
    int  width = 1280, height = 720;
    bool useOpenGL = false;
    WindowMode mode = WindowMode::Windowed;
    bool vsync = true;
};

class SDLPlatform {
public:
    bool initialize(const WindowConfig& config);
    void shutdown();

    bool applyGraphicsSettings(const WindowConfig& config);  // live-update

    bool pollQuitRequested();                                // pumps + forwards to Input
    bool pumpEvents(const std::function<void(const SDL_Event&)>& onEvent);

    void delayMillis(std::uint32_t ms);
    void swapBuffers();

    SDL_Window*    getWindow()    const;
    SDL_GLContext  getGLContext() const;

    int         currentWidth()  const;
    int         currentHeight() const;
    WindowMode  currentMode()   const;
};
```
`Application::run()` calls `pollQuitRequested()` each frame, which
also forwards every SDL event to `HBE::Platform::Input::HandleEvent`.
Return `true` on `SDL_EVENT_QUIT` OR Escape.

---

### `HBE/Platform/Input.h`
**Namespace:** `HBE::Platform::Input`
Frame-based state polling. Fed by `SDLPlatform::pollQuitRequested`.

```cpp
enum class MouseButton : int { Left = 1, Middle = 2, Right = 3, X1 = 4, X2 = 5 };

void Initialize();          // opens first available gamepad
void Shutdown();
void NewFrame();            // copy state -> prev, reset deltas/wheel
void HandleEvent(const SDL_Event& e);

// Keyboard.
bool  IsKeyDown     (SDL_Scancode key);
bool  IsKeyPressed  (SDL_Scancode key);   // this-frame edge
bool  IsKeyReleased (SDL_Scancode key);

// Mouse.
bool  IsMouseDown     (MouseButton button);
bool  IsMousePressed  (MouseButton button);
bool  IsMouseReleased (MouseButton button);
float GetMouseX();       float GetMouseY();
float GetMouseDeltaX();  float GetMouseDeltaY();
float GetMouseWheelX();  float GetMouseWheelY();

// Gamepad.
bool  HasGamepad();
bool  IsGamepadButtonDown    (SDL_GamepadButton button);
bool  IsGamepadButtonPressed (SDL_GamepadButton button);
bool  IsGamepadButtonReleased(SDL_GamepadButton button);
float GetGamepadAxis         (SDL_GamepadAxis axis);   // sticks [-1..+1], triggers [0..+1]
```
For gameplay bindings, prefer `HBE::Input::InputMap`. For dev/debug
hotkeys (F5/F6/G/B/H in MegaX), poll `IsKeyPressed(SDL_SCANCODE_F5)`
directly here.

---

### `HBE/Platform/Audio.h`
**Namespace:** `HBE::Platform`
SDL3_mixer-backed audio.

```cpp
class Audio {
public:
    enum class Bus { Master = 0, Music, SFX, UI, Ambient };

    struct PlayParams {
        int   loops       = 0;             //  0 = once,  -1 = forever
        float gain        = 1.0f;
        Bus   bus         = Bus::SFX;
        bool  positional  = false;
        float worldX = 0.0f, worldY = 0.0f;
        float minDistance = 48.0f, maxDistance = 800.0f;
        float panRange    = 350.0f;
        bool  stopWhenTooFar = false;
    };

    struct AudioStats {
        int   activeVoices = 0;
        bool  musicPlaying = false;
        float masterGain = 1.0f, musicGain = 1.0f;
        float sfxGain = 1.0f, uiGain = 1.0f, ambientGain = 1.0f;
    };

    bool initialize();
    void shutdown();
    void update(float dt);
    bool isInitialized() const;

    // Asset lifecycle.
    bool loadSound  (const std::string& name, const std::string& path, bool predecode = true);
    bool loadMusic  (const std::string& name, const std::string& path);
    bool unloadSound(const std::string& name);
    bool unloadMusic(const std::string& name);

    // Playback.
    bool         playSound  (const std::string& name, int loops = 0);
    VoiceHandle  playSoundEx(const std::string& name, const PlayParams& params);
    VoiceHandle  playMusic  (const std::string& name, int loops = -1, float gain = 1.0f);

    // Control.
    void stopVoice(VoiceHandle handle);
    void stopMusic();
    void stopAll();
    void pauseAll();     void resumeAll();
    void pauseBus(Bus);  void resumeBus(Bus);  void stopBus(Bus);

    // Mix.
    void  setMasterGain(float gain);      float masterGain() const;
    void  setBusGain(Bus bus, float gain); float busGain(Bus bus) const;
    void  setListenerPosition(float x, float y);
    void  getListenerPosition(float& x, float& y) const;

    AudioStats stats() const;
};
```
Buses give per-category volume/pause/stop control. Positional voices
attenuate + pan based on distance from the listener position.

---

### `HBE/Platform/GraphicsSettings.h`
**Namespace:** `HBE::Platform`
Simple `key=value` settings file for user prefs.

```cpp
struct GraphicsSettings {
    int width      = 1280;
    int height     = 720;
    WindowMode mode = WindowMode::Windowed;
    bool vsync     = true;
    int targetFPS  = 60;   // 0 = uncapped
};

bool LoadGraphicsSettings(GraphicsSettings& out, const std::string& path);
bool SaveGraphicsSettings(const GraphicsSettings& gs, const std::string& path);
```
Keys parsed: `width`, `height`, `mode`, `vsync`, `target_fps`.

---

## 7. HBE.Renderer.GL — Rendering primitives

### `HBE/Renderer/Color.h`
```cpp
struct Color4 { float r, g, b, a; };
```

### `HBE/Renderer/Transform2D.h`
```cpp
struct Transform2D {
    float posX, posY;
    float rotation;      // radians
    float scaleX, scaleY;
};
```

### `HBE/Renderer/RenderPass.h`
```cpp
enum class RenderPass : std::uint8_t { World = 0, UI = 1, Overlay = 2, Count };

enum class BlendMode  : std::uint8_t { Alpha = 0, Additive = 1, Opaque = 2, Invalid = 0xFF };

namespace Layers {
    constexpr int TileBackground;
    constexpr int TileWorld;
    constexpr int Sprites;
    constexpr int WorldFX;
    constexpr int WorldOverlay;
    constexpr int Particles;
    constexpr int UIBackground;
    constexpr int UIWidgets;
    constexpr int UIText;
    constexpr int OverlaydDebug;   // (typo preserved)
}
```
Order draws top-to-bottom by `(pass, layer, sortKey)`.

### `HBE/Renderer/RenderItem.h`
```cpp
struct RenderItem {
    Mesh*      mesh     = nullptr;
    Material*  material = nullptr;
    Transform2D transform;
    int        layer    = 0;
    float      sortKey  = 0.0f;
    RenderPass pass     = RenderPass::World;
    Color4     tint{ 1, 1, 1, 1 };
    float      uvRect[4]{ 0.0f, 0.0f, 1.0f, 1.0f };
};
```
The one currency of draw commands. Build with a mesh + material +
transform, pass to `Renderer2D::draw`.

### `HBE/Renderer/Material.h`
```cpp
class Material {
public:
    GLShader*  shader  = nullptr;
    Texture2D* texture = nullptr;
    Color4     color{ 1, 1, 1, 1 };
    bool       useSDF = false;
    float      sdfSoftness = 1.0f;
    BlendMode  blend = BlendMode::Alpha;

    void         apply(const float* mvp) const;
    std::uint16_t shaderId() const;
    std::uint16_t textureId() const;
    bool         uniformsEqual(const Material& other) const;
};
```

### `HBE/Renderer/Mesh.h`
```cpp
class Mesh {
public:
    bool create      (const std::vector<float>& vertices, int vertexCount);  // pos+color
    bool createPostUV(const std::vector<float>& vertices, int vertexCount);  // pos+UV

    unsigned int getVAO()         const;
    int          getVertexCount() const;
};
```

### `HBE/Renderer/Texture2D.h`
```cpp
class Texture2D {
public:
    int  getWidth()  const;
    int  getHeight() const;

    bool createChecker(int width = 128, int height = 128);
    bool loadFromFile(const std::string& path);
    bool createFromRGBA(int width, int height, const unsigned char* rgbaPixels);

    void         bind()       const;
    void         setFiltering(bool linear);        // true=font-quality, false=pixel-art
    unsigned int getID() const;
};
```

### `HBE/Renderer/GLShader.h`
```cpp
class GLShader {
public:
    bool createFromSource(const char* vertexSrc, const char* fragmentSrc);
    bool createFromFiles(const std::string& vertexPath, const std::string& fragmentPath);
    bool reloadFromFiles();                       // keeps old program on failure

    bool setMat4(const char* name, const float* value) const;
    int  getUniformLocation(const char* name) const;
    void use() const;

    const std::string& vertexPath()   const;
    const std::string& fragmentPath() const;
};
```

### `HBE/Renderer/Framebuffer.h`
```cpp
class Framebuffer {
public:
    bool          resize(int width, int height);
    void          bind() const;
    static void   bindDefault();

    unsigned int  colorTextureID() const;
    unsigned int  fboID()          const;
    int           width()  const;
    int           height() const;
    bool          vlaid() const;                 // typo preserved
};
```

### `HBE/Renderer/GLRenderer.h`
Low-level 2D renderer backend.

```cpp
class GLRenderer {
public:
    bool initialize(HBE::Platform::SDLPlatform& platform);
    void beginFrame();
    void endFrame(HBE::Platform::SDLPlatform& platform);

    void setClearColor(float r, float g, float b, float a = 1.0f);
    void setCamera(const Camera2D& cam);
    void draw(const RenderItem& item);

    void resizeViewport(int width, int height);
    void setViewportRect(int x, int y, int width, int height);
    void beginFrameFullWindow(int windowW, int windowH);
    void beginFrameInViewport(int vpX, int vpY, int vpW, int vpH);

    void getViewProjection(float out16[16]) const;
    void setPostProcessStack(PostProcessStack* stack);
};
```

### `HBE/Renderer/Renderer2D.h`
High-level 2D facade over `GLRenderer` with batching.

```cpp
class Renderer2D {
public:
    // Expanded in work item 14.
    struct Renderer2DStats {
        int drawCalls = 0, quads = 0, stateChanges = 0, passes = 0;
        int submittedQuads = 0, renderedQuads = 0, culledSprites = 0;
        int materialChanges = 0, textureChanges = 0;
    };

    explicit Renderer2D(GLRenderer& backend);

    const Camera2D*  activeCamera() const;
    Renderer2DStats  getStats() const;
    void             resetFrameStats();          // item 14
    void             addCulledSprites(int n);    // item 14

    void setSpriteQuadMesh(const Mesh* quadMesh);

    void beginScene(const Camera2D& camera);
    void beginScene(const Camera2D& camera, RenderPass pass);
    void endScene();

    void draw      (const RenderItem& item);   // batched
    void drawDirect(const RenderItem& item);   // bypass batching (debug shapes)
};
```

`Application::run` calls `resetFrameStats()` before the render pass and
copies `getStats()` into `Profiler::PublishRendererStats` after
`GLRenderer::endFrame` — a game does not need to do either. Culling
code that drops sprites before submission should report them with
`addCulledSprites` so `culledSprites` stays truthful.

---

### `HBE/Renderer/GpuTimer.h`
**Namespace:** `HBE::Renderer::GpuTimer` — *added in work item 14.*

Non-blocking GPU timing via OpenGL timer queries. Results are read back
`kResultLatencyFrames` frames later and forwarded to
`HBE::Core::Profiler`, so the CPU never stalls waiting on the GPU.

```cpp
static constexpr std::size_t kResultLatencyFrames = 3;
static constexpr std::size_t kMaxGpuSections      = 32;

bool Initialize();      // called by GLRenderer::initialize
void Shutdown();        // called by Application::~Application
bool IsSupported();     // false without ARB_timer_query

void NewFrame();        // called at the top of Application::run;
                        // retires ready queries and publishes them

int  BeginScope(const char* name);
void EndScope(int handle);

class Scope {           // RAII; copy AND move deleted
public:
    explicit Scope(const char* name) noexcept;
    ~Scope() noexcept;
};
```

```cpp
HBE_GPU_SCOPE("MyPass");   // RAII GPU scope
```

Notes:

* Unsupported hardware fails **gracefully**: `IsSupported()` returns
  false, `SetGpuSupported(false)` is pushed to the profiler, scopes
  become no-ops, and rendering continues normally.
* `GLRenderer` already opens `"GpuFrame"` and `"GpuScene"` per frame
  and closes them in `endFrame()`. A game adding its own passes should
  nest inside those.
* Unlike `HBE_PROFILE_SCOPE`, `HBE_GPU_SCOPE` is **not** compiled out
  in Release — it is gated at runtime by `IsSupported()`.

### `HBE/Renderer/ResourceCache.h`
Central asset cache with hot-reload tracking.

```cpp
class ResourceCache {
public:
    // Shaders.
    GLShader* getOrCreateShader(const std::string& name, const char* vertexSrc, const char* fragmentSrc);
    GLShader* getOrCreateShaderFromFiles(const std::string& name,
                                         const std::string& vertexPath,
                                         const std::string& fragmentPath);
    bool      reloadShader(const std::string& name);

    // Textures.
    Texture2D* getOrCreateCheckerTexture(const std::string& name, int width, int height);
    Texture2D* getOrCreateTextureFromFile(const std::string& name, const std::string& path);
    Texture2D* getOrCreateTextureFromRGBA(const std::string& name, int width, int height,
                                          const unsigned char* rgbaPixels);
    Texture2D* placeholderTexture();
    bool       reloadTexture(const std::string& name);

    // Meshes.
    Mesh* getOrCreateMeshPosColor(const std::string& name, const std::vector<float>& vertices, int vertexCount);
    Mesh* getOrCreateMeshPosUV   (const std::string& name, const std::vector<float>& vertices, int vertexCount);

    // Read-only lookup.
    GLShader*  getShader (const std::string& name) const;
    Texture2D* getTexture(const std::string& name) const;
    Mesh*      getMesh   (const std::string& name) const;

    // Tracked file paths for FileWatcher hot-reload.
    const std::unordered_map<std::string, std::string>& trackedTextureFiles() const;
    const std::unordered_map<std::string, std::pair<std::string, std::string>>& trackedShaderFiles() const;
};
```
Access via `Application::resources()`. Item 11's shader hot-reload
uses `trackedShaderFiles()` + `reloadShader()`.

---

## 8. HBE.Renderer.GL — Cameras & controllers

### `HBE/Renderer/Camera2D.h`
```cpp
struct Camera2D {
    float x, y;                        // world-space center
    float zoom;                        // >1 zoom in, <1 zoom out
    float viewportWidth, viewportHeight;
};
```

### `HBE/Renderer/CameraController.h`
Stateful camera behavior. Follow, dead-zone, look-ahead, bounds,
zoom, shake, scripted moves.

```cpp
class CameraController {
public:
    // Tunables.
    float followResponse;
    bool  pixelSnap;
    bool  followEnabled;
    float deadzoneHalfW, deadzoneHalfH;
    float lookAheadX, lookAheadY;
    float lookAheadVelSaturateX, lookAheadVelSaturateY;
    float lookAheadResponse;
    int   facingHintX;
    bool  boundsEnabled;
    float boundsMinX, boundsMinY, boundsMaxX, boundsMaxY;
    float zoomTarget;
    float zoomResponse;
    float minZoom, maxZoom;
    float shakeMaxTranslation;
    float shakeMaxRotation;
    float shakeDecay;

    // Access.
    const Camera2D& camera() const;
    Camera2D&       camera();

    // Targeting.
    void setFollowTarget(float worldX, float worldY);
    void clearFollowTarget();
    void snapTo(float worldX, float worldY);
    void setViewport(float w, float h);
    void setDeadzone(float halfW, float halfH);
    void setLookAhead(float maxX, float maxY, float satVX = 240.0f, float satVY = 240.0f);
    void setFollowVelocity(float vx, float vy);
    void setFacingHintX(int sign);

    // Bounds & zoom.
    void setBounds(float minX, float minY, float maxX, float maxY);
    void clearBounds();
    void setZoomTarget(float z);
    void snapZoom(float z);

    // Shake.
    void  addShake(float amount);
    void  setShake(float amount);
    float trauma() const;

    // Follow control.
    void  pauseFollow();
    void  resumeFollow();

    // Secondary targets (weighted midpoint).
    struct SecondaryTarget { float x, y, weight; };
    void addSecondaryTarget(float x, float y, float weight = 1.0f);
    void clearSecondaryTargets();

    // Scripted focus.
    void focusOn(float worldX, float worldY, float seconds);
    bool isScripted() const;

    struct DeadzoneRect { float minX, minY, maxX, maxY; };
    DeadzoneRect deadzoneRect() const;

    void update(float dt);      // call every frame
};
```
Pass `camera()` to `GLRenderer::setCamera`. Set `pixelSnap = true`
for pixel-art games.

---

## 9. HBE.Renderer.GL — Sprites, text, debug draw

### `HBE/Renderer/Sprite2D.h`
Original sprite-sheet API.

```cpp
struct SpriteSheet {
    Texture2D* texture = nullptr;
    int  texWidth = 0,  texHeight = 0;
    int  frameWidth = 0, frameHeight = 0;
    int  marginX = 0, marginY = 0;
    int  spacingX = 0, spacingY = 0;
    bool isValid() const;
};

class SpriteRenderer2D {
public:
    static SpriteSheet DeclareSpriteSheet(ResourceCache& cache,
                                          const std::string& name,
                                          const std::string& path,
                                          int frameWidth, int frameHeight,
                                          int marginX = 0, int marginY = 0,
                                          int spacingX = 0, int spacingY = 0);

    static void SetStaticSpriteFrame(RenderItem& item,
                                     const SpriteSheet& sheet,
                                     int col, int row);
};

class SpriteAnimation {
public:
    SpriteAnimation();
    SpriteAnimation(const SpriteSheet* sheet,
                    int colStart, int colEnd, int row,
                    float framesPerSecond,
                    bool loop = true);
    void play(bool restartIfPlaying = false);
    void stop();
    void update(float dt);
    void apply(RenderItem& item) const;
    bool isPlaying() const;
};
```

### `HBE/Renderer/SpriteRenderer2D.h`
Newer, richer sprite-sheet API with a stateful `Animator` and named
clips.

```cpp
struct SpriteSheetDesc {
    int frameWidth, frameHeight;
    int marginX, marginY;
    int spacingX, spacingY;
};

struct SpriteAnimationDesc {
    std::string name;
    int   row;
    int   startCol;
    int   frameCount;
    float frameDuration;
    bool  loop;
};

class SpriteRenderer2D {
public:
    struct SpriteSheetHandle {
        Texture2D* texture = nullptr;
        int  texWidth = 0, texHeight = 0;
        SpriteSheetDesc desc;
    };

    static SpriteSheetHandle declareSpriteSheet(ResourceCache& cache,
                                                const std::string& cacheName,
                                                const std::string& filePath,
                                                const SpriteSheetDesc& desc);

    static void setFrame(RenderItem& item,
                         const SpriteSheetHandle& sheet,
                         int col, int row);

    class Animator {
    public:
        const SpriteSheetHandle*         sheet = nullptr;
        std::vector<SpriteAnimationDesc> clips;
        int   currentClipIndex   = -1;
        int   currentFrameInClip = 0;
        float frameTimer = 0.0f;
        bool  playing = false;

        void addClip(const SpriteAnimationDesc& clip);
        void play(const std::string& name, bool restartIfSame = false);
        void stop();
        void update(float dt);
        void apply(RenderItem& item) const;
    };
};
```
Prefer this API for new work — the older `Sprite2D.h` version is
still supported but has less flexibility.

### `HBE/Renderer/SpriteBatch2D.h`
CPU quad batcher used internally by `Renderer2D`.

```cpp
class SpriteBatch2D {
public:
    void setQuadMesh(const Mesh* quadMesh);
    void begin();
    void submit(const RenderItem& item);
    void flush(const float* viewProj);

    int drawCalls()    const;
    int quadCount()    const;
    int stateChanges() const;
};
```

### `HBE/Renderer/SpriteAnimationStateMachine.h`
Higher-level state-machine animation controller (used by Scene2D
`AnimationComponent2D`).

```cpp
class SpriteAnimationStateMachine {
public:
    using EventCallback = std::function<void(const std::string& eventName)>;

    const SpriteRenderer2D::SpriteSheetHandle* sheet = nullptr;
    float globalSpeed = 1.0f;

    struct Clip {
        std::string name;
        int   row = 0, startCol = 0, frameCount = 1;
        float frameDuration = 0.1f;
        float loop = true;
        float speed = 1.0f;
    };

    struct ClipEvent { std::string name; int frame = 0; };

    void addClip(const Clip& clip);
    const Clip* getClip(const std::string& name) const;
    void addEvent(const std::string& clipName, int frame, const std::string& eventName);

    struct State {
        std::string name;
        std::string clipName;
        float speed = 1.0f;
    };

    enum class TransitionType { Always, BoolEquals, Trigger, Finished };

    struct Transition {
        std::string fromState, toState;
        TransitionType type = TransitionType::Always;
        std::string varOrTrigger;
        bool boolValue = false;
    };

    void addState(const std::string& stateName, const std::string& clipName, float speed = 1.0f);
    void addTransitionAlways   (const std::string& from, const std::string& to);
    void addTransitionBool     (const std::string& from, const std::string& to, const std::string& var, bool value);
    void addTransitionTrigger  (const std::string& from, const std::string& to, const std::string& triggerName);
    void addTransitionFinished (const std::string& from, const std::string& to);

    void setBool  (const std::string& name, bool value);
    bool getBool  (const std::string& name) const;
    void trigger  (const std::string& name);

    void setState (const std::string& stateName, bool restart = true);
    const std::string& getState() const;

    void update  (float dt, const EventCallback& onEvent = {});
    void apply   (RenderItem& item) const;

    bool isClipFinished() const;
    int  currentFrame()   const;
};
```

### `HBE/Renderer/AnimationPresetRegistry.h`
Named animation "builders" — set up common state-machine layouts
once, reuse across entities/scenes.

```cpp
class AnimationPresetRegistry {
public:
    using Builder = std::function<void(HBE::ECS::Entity,
                                       SpriteAnimationStateMachine&,
                                       Scene2D&)>;
    void registerPreset(std::string name, Builder builder);
    bool has(const std::string& name) const;
    void build(const std::string& name,
               HBE::ECS::Entity entity,
               SpriteAnimationStateMachine& sm,
               Scene2D& scene) const;
    std::vector<std::string> names() const;
    int  size() const;
    void clear();
};
```

### `HBE/Renderer/Font.h`
Bitmap + SDF font atlas builder.

```cpp
class Font {
public:
    static constexpr int FirstChar = 32, LastChar = 126;
    static constexpr int CharCount = LastChar - FirstChar + 1;

    enum class Mode { Bitmap, SDF };

    bool loadFromTTF    (ResourceCache& cache, const std::string& cacheTextureName,
                         const std::string& ttfPath, float pixelHeight,
                         int atlasW = 512, int atlasH = 512, int padding = 2);
    bool loadFromTTF_SDF(ResourceCache& cache, const std::string& cacheTextureName,
                         const std::string& ttfPath, float pixelHeight,
                         int atlasW = 1024, int atlasH = 1024, int padding = 8,
                         std::uint8_t onEdgeValue = 128, float pixelDistScale = 64.0f);

    Texture2D* texture() const;
    int   atlasWidth()  const;
    int   atlasHeight() const;
    float pixelHeight() const;
    float lineHeight()  const;
    Mode  mode()        const;
    bool  isSDF()       const;

    bool buildQuad(char c,
                   float& penX, float& penY,
                   float& outX0, float& outY0, float& outX1, float& outY1,
                   float& outU0, float& outV0, float& outU1, float& outV1) const;
};
```

### `HBE/Renderer/TextRenderer2D.h`
High-level text drawing service.

```cpp
class TextRenderer2D {
public:
    enum class TextAlignH { Left, Center, Right };
    enum class TextAlignV { Baseline, Top, Middle, Bottom };

    struct TextLayout { float width = 0.0f, height = 0.0f; int lineCount = 1; };

    struct TextAnim {
        float t = 0.0f, duration = 1.0f;
        bool  typewriter = false;
        float charsPerSecond = 30.0f;
        int   maxChars = -1;
        bool  fadeIn = false;    float fadeInTime  = 0.25f;
        bool  fadeOut = false;   float fadeOutTime = 0.25f;
        float offsetX = 0.0f, offsetY = 0.0f;
        float velX = 0.0f, velY = 0.0f;
        float startScale = 1.0f, endScale = 1.0f;
        bool  autoExpire = false;
    };

    bool initialize(ResourceCache& cache, GLShader* spriteShader, Mesh* quadMesh);

    bool loadFont   (ResourceCache& cache, const std::string& fontName,
                     const std::string& ttfPath, float pixelHeight,
                     int atlasW = 512, int atlasH = 512);
    bool loadSDFont (ResourceCache& cache, const std::string& fontName,
                     const std::string& ttfPath, float pixelHeight,
                     int atlasW = 1024, int atlasH = 1024, int padding = 8);

    void setActiveFont(const std::string& fontName);

    void drawText(Renderer2D& r2d, float x, float y, const std::string& text,
                  float scale = 1.0f, Color4 tint = {1,1,1,1});

    void drawTextAligned(Renderer2D& r2d, float x, float y, const std::string& text,
                         float scale, Color4 tint,
                         TextAlignH alignH, TextAlignV alignV,
                         float maxWidth = 0.0f, float lineSpacingMult = 1.25f);

    void drawTextAnimated(Renderer2D& r2d, float x, float y, const std::string& text,
                          float baseScale, Color4 baseTint,
                          TextAlignH alignH, TextAlignV alignV,
                          float maxWidth, float lineSpacingMult,
                          const TextAnim& anim);

    TextLayout measureText(const std::string& text, float scale = 1.0f, float maxWidth = 0.0f) const;

    void setCullingEnabled(bool enabled);
    void setCullInset(float inset);
    void beginFrame(std::uint64_t frameIndex);   // call once per frame
};
```

### `HBE/Renderer/DebugDraw2D.h`
Solid/outlined rect for debug overlays.

```cpp
class DebugDraw2D {
public:
    bool initialize(ResourceCache& cache, Mesh* quadMesh);
    void rect(Renderer2D& r2d,
              float cx, float cy, float w, float h,
              float r, float g, float b, float a,
              bool filled);
};
```

### `HBE/Renderer/DebugFont8x8.h`
```cpp
static const uint8_t g_font8x8_basic[128][8];   // ASCII bitmap table
```

---

## 10. HBE.Renderer.GL — Tilemaps & collision

### `HBE/Renderer/TileMap.h`
```cpp
enum class SlopeType : uint8_t { None = 0, LeftUp = 1, RightUp = 2 };

struct TileMapTileset {
    std::string name;
    std::string texturePath;
    int  tileW = 16, tileH = 16;
    int  margin = 0, spacing = 0;
    std::unordered_set<int> solidTiles;      // 1-based tile IDs
    std::unordered_set<int> oneWayTiles;
    std::unordered_map<int, SlopeType> slopes;

    bool      isSolid   (int tileId) const;
    bool      isOneWay  (int tileId) const;
    SlopeType slopeType (int tileId) const;
};

struct TileMapLayer {
    std::string name;
    int  w = 0, h = 0;
    int  tilesetIndex = 0;
    std::vector<int> data;                   // row-major, bottom-left origin
    int at(int x, int y) const;              // 0 if OOB or empty
};

struct TileMap {
    int   version         = 1;
    int   tileSizeW = 16,   tileSizeH = 16;
    float tilePixelScale  = 1.0f;

    float worldTileW() const;
    float worldTileH() const;

    std::vector<TileMapTileset> tilesets;
    std::vector<TileMapLayer>   layers;

    const TileMapLayer* findLayer(const std::string& layerName) const;
};
```

### `HBE/Renderer/TileMapLoader.h`
```cpp
class TileMapLoader {
public:
    static bool loadFromJsonFile(const std::string& path, TileMap& outMap,
                                 std::string* outError = nullptr);

    static bool sampleTileTopColors(
        const TileMap& map,
        std::unordered_map<int, std::array<float, 4>>& outColors,
        int topRowsToSample = 3);
};
```
`sampleTileTopColors` is what MegaX Item 07 uses to tint bullet-impact
particles to match the tile's top-row color.

### `HBE/Renderer/TileMapRenderer.h`
```cpp
class TileMapRenderer {
public:
    bool build(Renderer2D& r2d, ResourceCache& cache,
               GLShader* spriteShader, Mesh* quadMesh,
               const TileMap& map);
    void draw(Renderer2D& r2d, const TileMap& map);
};
```

### `HBE/Renderer/TileCollision.h`
```cpp
struct AABB { float cx = 0, cy = 0, w = 0, h = 0; };  // center-based

struct MoveResult2D {
    bool hitX = false, hitY = false;
    bool grounded = false, ceiling = false;
    bool steppedUp = false;
};

class TileCollision {
public:
    static bool      isSolidTile (const TileMap&, const TileMapLayer&, int tx, int ty);
    static bool      isOneWayTile(const TileMap&, const TileMapLayer&, int tx, int ty);
    static SlopeType slopeTypeAt (const TileMap&, const TileMapLayer&, int tx, int ty);

    static void moveAndCollide(const TileMap& map, const TileMapLayer& layer,
                               AABB& box, float& velX, float& velY, float dt);

    static MoveResult2D moveAndCollideEx(
        const TileMap& map, const TileMapLayer& layer,
        AABB& box, float& velX, float& velY, float dt,
        float maxStepUp,
        bool  enableOneWay,
        bool  enableSlopes,
        float oneWayPrevBottom);
};
```

---

## 11. HBE.Renderer.GL — Particles

### `HBE/Renderer/EmitterConfig.h`
Pure-data description of a single emitter.

```cpp
struct EmitterConfig {
    struct Burst {
        float time       = 0.0f;
        float count      = 10;
        int   maxRepeats = 1;
        float interval   = 0.1f;
    };

    // Emission timing.
    float emissionRate = 20.0f;
    float duration     = 1.0f;
    bool  loop         = false;
    std::vector<Burst> bursts;

    // Lifetime.
    float lifetimeMin = 0.4f, lifetimeMax = 0.9f;

    // Spawn shape.
    enum class Shape { Point, Circle, Ring, Rect, Line };
    Shape shape = Shape::Point;
    float shapeRadius = 0.0f;
    float shapeWidth  = 0.0f, shapeHeight = 0.0f;

    // Motion.
    float speedMin = 60.0f, speedMax = 140.0f;
    float dirMin   = 0.0f,  dirMax   = 360.0f;   // degrees
    float inheritVX = 0.0f, inheritVY = 0.0f;
    float gravityX = 0.0f,  gravityY = 0.0f;
    float drag = 0.0f;

    // Rotation.
    float startRotMin = 0.0f, startRotMax = 0.0f;
    float rotVelMin   = 0.0f, rotVelMax   = 0.0f;

    // Size.
    float startSizeMin = 6.0f, startSizeMax = 10.0f;
    float endSizeMin   = 0.0f, endSizeMax   = 0.0f;

    // Color (start + end + optional variation).
    float startR = 1, startG = 1, startB = 1, startA = 1;
    float endR   = 1, endG   = 1, endB   = 1, endA   = 0;
    float startR2 = -1, startG2 = -1, startB2 = -1, startA2 = -1; // variation

    // Render.
    bool worldSpace     = true;      // false = follow emitter
    int  maxParticles   = 200;
    int  sortLayer      = 200;
    bool additiveBlend  = false;
    std::string name;
};

using EffectDef = std::vector<EmitterConfig>;    // multi-emitter effect
```

### `HBE/Renderer/ParticleTypes.h`
```cpp
struct Particle {
    float x, y;
    float vx, vy;
    float life, maxLife;
    float startSize, endSize, size;
    float rotation, rotVel;
    float sr, sg, sb, sa;   // start color
    float er, eg, eb, ea;   // end color
    bool  alive;
};
```
Internal; game code doesn't touch particles directly.

### `HBE/Renderer/ParticleEmitter.h`
```cpp
using ParticleRenderFn = std::function<void(
    float x, float y, float size, float rotation,
    float r, float g, float b, float a, bool additive)>;

class ParticleEmitter {
public:
    EmitterConfig config;
    float worldX = 0.0f, worldY = 0.0f;
    bool  active = true, alive = true;

    void init();
    void update(float dt);
    void render(const ParticleRenderFn& fn) const;
    bool isDone() const;
    int  liveCount() const;
    void stop();
    void burst(int count);
    void reset();
};
```

### `HBE/Renderer/ParticleSystem.h`
High-level named-effect API used by MegaX `Effects`.

```cpp
using ParticleHandle = std::uint32_t;
inline constexpr ParticleHandle kInvalidParticle = 0u;

class ParticleSystem {
public:
    bool initialize(ResourceCache& cache, Mesh* quadMesh);
    void shutdown();

    // Registration.
    void registerEffect(const std::string& name, const EffectDef& def);
    void registerEffect(const std::string& name, const EmitterConfig& single);

    // Spawning.
    void           spawn        (const std::string& name, float x, float y);
    ParticleHandle spawnManaged (const std::string& name, float x, float y);
    ParticleHandle spawnAttached(const std::string& name,
                                 Scene2D* scene, EntityID entity,
                                 float offsetX = 0.0f, float offsetY = 0.0f);

    // Handle control.
    void stop        (ParticleHandle h);
    void kill        (ParticleHandle h);
    void setPosition (ParticleHandle h, float x, float y);
    bool isAlive     (ParticleHandle h) const;

    // Frame loop.
    void update(float dt);
    void render(Renderer2D& r2d) const;
    void render(Renderer2D& r2d, const ParticleRenderFn& fn) const;

    void clear();
    int  totalLiveParticles() const;
    int  activeEffectCount()  const;
};
```
MegaX pattern: register named effects in `Effects::init` (walk_dust,
muzzle_flash, enemy_explosion, blood_splatter, etc.), then
`spawn("walk_dust", x, y)` from gameplay code.

---

## 12. HBE.Renderer.GL — Scene, ECS components, serialization

### `HBE/Renderer/ECSComponents2D.h`
Renderer-side ECS components.

```cpp
struct SpriteComponent2D {
    Mesh*     mesh     = nullptr;
    Material* material = nullptr;
    int       layer    = 0;
    float     sortKey  = 0.0f;
    float     uvRect[4]{ 0.0f, 0.0f, 1.0f, 1.0f };
};

struct AnimationComponent2D {
    SpriteAnimationStateMachine sm;
};
```

### `HBE/Renderer/Scene2D.h`
Full 2D ECS scene: entities, sprite animation, physics-lite,
tile collision, rendering.

```cpp
using EntityID = HBE::ECS::Entity;
inline constexpr EntityID InvalidEntityID = HBE::ECS::Null;

struct Physics2DSettings {
    float gravityY    = -1800.0f;
    int   maxSubSteps = 4;
    float maxStepDt   = 1.0f / 120.0f;
};

class Scene2D {
public:
    EntityID createEntity();
    EntityID createEntity(const RenderItem& templateItem);
    void     removeEntity(EntityID id);
    void     clear();

    Transform2D*                  getTransform      (EntityID id);
    SpriteAnimationStateMachine*  addSpriteAnimator (EntityID id,
                                                     const SpriteRenderer2D::SpriteSheetHandle* sheet);
    SpriteAnimationStateMachine*  getSpriteAnimator (EntityID id);

    void setTileCollisionContext(const TileMap* map, const TileMapLayer* collisionLayer);
    void setCullingEnabled(bool enabled);
    void setPhysics2DSettings(const Physics2DSettings& s);
    const Physics2DSettings& physics2DSettings() const;

    void update(float dt,
                const SpriteAnimationStateMachine::EventCallback& onAnimEvent = {});
    void render(Renderer2D& renderer);

    HBE::ECS::Registry&       registry();
    const HBE::ECS::Registry& registry() const;
    HBE::Core::EventBus&      eventBus();
    const HBE::Core::EventBus& eventBus() const;

    bool cullingEnabled() const;
    bool tryAdoptId(EntityID e, const std::string& uuid, const std::string& tag);
};
```

### `HBE/Renderer/Prefab.h`
```cpp
inline constexpr int kPrefabVersion = 1;

struct PrefabDefinition {
    std::string  name;
    std::string  tag;
    nlohmann::json components;
    std::string  sourcePath;
};

struct PrefabRefComponent {
    std::string name;    // resolves to a PrefabDefinition by name
};
```

### `HBE/Renderer/PrefabLibrary.h`
```cpp
class PrefabLibrary {
public:
    int  loadDirectory(std::string_view directoryLogicalPath);
    void addOrReplace (PrefabDefinition def);

    const PrefabDefinition* get (const std::string& name) const;
    bool                    has (const std::string& name) const;
    std::vector<std::string> names() const;
    int                     size() const;
    void                    clear();
};
```

### `HBE/Renderer/ScriptRegistry.h`
```cpp
class ScriptRegistry {
public:
    using Factory = std::function<HBE::ECS::Script(HBE::ECS::Entity, Scene2D&)>;

    void registerScript(std::string name, Factory factory);
    bool has(const std::string& name) const;
    HBE::ECS::Script create(const std::string& name, HBE::ECS::Entity entity, Scene2D& scene) const;

    std::vector<std::string> names() const;
    int  size() const;
    void clear();
};
```

### `HBE/Renderer/SceneSerializer.h`
Save/load `Scene2D` to disk with external asset resolution callbacks.

```cpp
struct SceneSaveCallbacks {
    std::function<std::string(const Mesh*)>                                          meshKey;
    std::function<std::string(const Material*)>                                      materialKey;
    std::function<std::string(const SpriteRenderer2D::SpriteSheetHandle*)>           sheetKey;
    const PrefabLibrary* prefabs = nullptr;
};

struct SceneLoadCallbacks {
    std::function<Mesh*(const std::string&)>                                                    mesh;
    std::function<Material*(const std::string&)>                                                material;
    std::function<const SpriteRenderer2D::SpriteSheetHandle*(const std::string&)>               sheet;
    std::function<bool(HBE::ECS::Entity, const std::string& scriptName, Scene2D&)>              bindScript;
    std::function<void(HBE::ECS::Entity, const std::string& presetName,
                       SpriteAnimationStateMachine&, Scene2D&)>                                 buildAnimatorPreset;
    const PrefabLibrary*             prefabs   = nullptr;
    const ScriptRegistry*            scripts   = nullptr;
    const AnimationPresetRegistry*   animators = nullptr;
};

class SceneSerializer {
public:
    static constexpr int kSceneVersion = 3;

    static bool saveToFile  (Scene2D& scene, const std::string& path,
                             const SceneSaveCallbacks& cb,
                             const std::string& tilemapPath,
                             std::string* outError = nullptr);

    static bool loadFromFile(Scene2D& scene, const std::string& path,
                             const SceneLoadCallbacks& cb,
                             std::string* outTilemapPath = nullptr,
                             std::string* outError = nullptr);
};
```

---

## 13. HBE.Renderer.GL — Post-process stack

### `HBE/Renderer/PostProcessStack.h`
Fullscreen chained effects using scene / ping / pong FBOs.

```cpp
struct PostProcessEffect {
    std::string name;
    GLShader*   shader = nullptr;
    bool        enabled = true;
    std::array<float, 8> params{};   // up to 8 uniform floats
};

class PostProcessStack {
public:
    bool  initialize(int logicalWidth, int logicalHeight);
    void  resize    (int logicalWidth, int logicalHeight);

    void  addEffect (PostProcessEffect effect);
    PostProcessEffect* getEffect(const std::string& name);
    void  setEffectEnabled(const std::string& name, bool enabled);
    void  clearEffects();
    int   effectCount() const;

    void  bindSceneFBO();                            // draw scene into offscreen buffer
    void  present(int vpX, int vpY, int vpW, int vpH); // run chain + blit to viewport

    bool               isInitialized() const;
    const Framebuffer& sceneFBO()      const;
};
```
Hook to `GLRenderer::setPostProcessStack(&stack)` and the renderer
will route rendering through it.

---

## 14. HBE.Renderer.GL — Immediate-mode UI

### `HBE/Renderer/UI/UIRect.h`
```cpp
struct UIRect {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    float left()  const;   float right() const;
    float bottom() const;  float top()   const;
    bool contains(float px, float py) const;
};
```
Bottom-left origin.

### `HBE/Renderer/UI/UIStyle.h`
```cpp
struct UIStyle {
    Color4 panelBg, panelBorder;
    Color4 btnIdle, btnHover, btnDown, btnBorder;
    Color4 text, textMuted;
    Color4 sliderTrack, sliderFill, sliderKnob, sliderKnobHover;
    float  padding = 10.0f;
    float  itemH   = 32.0f;
    float  spacing = 8.0f;
    float  baselineFudgeMul = 0.55f;
    float  textScale        = 1.0f;
};
```

### `HBE/Renderer/UI/UIThemeLoader.h`
```cpp
class UIThemeLoader {
public:
    static bool loadStyleFromJsonFile(const std::string& path,
                                      UIStyle& outStyle,
                                      std::string* outError = nullptr);
};
```

### `HBE/Renderer/UI/UIContext.h`
Immediate-mode UI. Panels, buttons, checkboxes, toggle buttons,
sliders, scroll panels.

```cpp
class UIContext {
public:
    void            setStyle(const UIStyle& style);
    UIStyle&        style();
    const UIStyle&  style() const;

    void beginFrame(float dt);
    void endFrame();
    void onEvent(HBE::Core::Event& e);

    // Panels.
    bool beginPanel      (const char* id, const UIRect& rect, const char* title = nullptr);
    bool beginScrollPanel(const char* id, const UIRect& rect, const char* title = nullptr);
    void endPanel();

    // Widgets.
    void spacing(float h);
    void label  (const char* text, bool muted = false);

    bool button    (const char* id, const char* text);
    bool buttonRect(const char* id, const UIRect& rect, const char* text);

    bool checkbox    (const char* id, const char* label, bool& value);
    bool toggleButton(const char* id, const char* text,  bool& value);

    bool sliderFloat(const char* id, const char* label, float& value,
                     float min, float max, float step = 0.0f);
    bool sliderInt  (const char* id, const char* label, int&   value,
                     int min, int max);

    // Input state.
    float mouseX() const;
    float mouseY() const;
    float wheelY() const;

    // Bind backends every frame.
    void bind(HBE::Renderer::Renderer2D* r2d,
              HBE::Renderer::DebugDraw2D* debug,
              HBE::Renderer::TextRenderer2D* text);
};
```
IDs must be stable across frames. Scroll panels retain scroll offset
across frames.

---

## 15. Feature summary — what the engine can do today

A quick capabilities checklist keyed off the public API surface
documented above.

### App / lifecycle
* Windowed and borderless-fullscreen SDL3 window with configurable
  size, vsync, and title.
* Live-swap graphics settings (window mode / size / vsync) at runtime.
* Letterboxed logical rendering with automatic screen ↔ logical
  coordinate conversion.
* Layer-based application structure with regular layers and overlays;
  layers get `onAttach/onDetach/onUpdate/onRender/onEvent`.
* Asset-root and user-data-root discovery via `AssetPaths::Initialize`
  with sibling-project walking.
* Structured logging with levels (`SetLogLevel`, `LogTrace/Info/Warn/Error/Fatal`).
* Engine time accessors (seconds + milliseconds since start).
* UUID generator for asset / entity IDs.

### Input
* Two-tier input model:
  * `HBE::Platform::Input` — raw keyboard / mouse / one-gamepad
    polling with frame-based edge detection (`IsKeyPressed/Down/Released`,
    mouse position + delta + wheel, gamepad buttons + axes).
  * `HBE::Input::InputMap` — semantic action / axis bindings with
    primary+secondary slots, gamepad axis thresholding, save/load to
    file, runtime rebinding.
* Text input events for widgets / console.

### Events
* Type-erased pub/sub `EventBus` with immediate and queued dispatch.
* RAII `ScopedSubscription` wrapper.
* Ready-made event payloads for combat, animation, audio, particles,
  scene changes, game state, interactions, and collisions.
* Layer-based `Event` hierarchy for window / input events.

### File watching
* Polling `FileWatcher` for dev hot-reload of assets — used by MegaX
  Item 11 for level JSON + sprite shader reload.

### ECS
* Sparse-set registry with `create/destroy/valid/has/get/tryGet/emplace/remove/view`.
* Range-for iteration via `Registry::view<Components...>`.
* Generic components: `Collider2D`, `RigidBody2D`, `Script`.
* Runtime metadata: `IDComponent`, `TagComponent`, `ScriptRuntimeState`,
  `AnimatorPresetComponent`.

### Combat
* Full ECS combat with `Faction`, `Health`, `Hurtbox`, `Hitbox`,
  `Knockback`.
* Per-faction can-hit mask, friendly fire flag, invuln timer,
  facing-based knockback flip, one-hit-per-target dedup.
* `updateCombat(reg, dt, bus)` — one call resolves damage, knockback,
  death, and publishes `DamageEvent` / `KnockbackAppliedEvent` /
  `DeathEvent`.
* Hitbox presets + registry + `spawnHitboxByName` for reusable attack
  templates.

### Audio
* SDL3_mixer-backed named sound + music assets.
* Voices with per-voice gain, positional (distance + pan) attenuation,
  and looping.
* Master + 5-bus mix (Music / SFX / UI / Ambient), per-bus pause /
  stop / gain.
* Listener position for positional voices.
* Runtime stats: active voices, music state, per-bus gains.

### Graphics — core
* OpenGL 2D renderer with clear color, viewport control, and
  `View + Projection` handling.
* View-projection accessor for shader authors.
* Optional post-process stack routing via `GLRenderer::setPostProcessStack`.

### Graphics — assets
* Central `ResourceCache` for shaders, textures, and meshes.
* Shader hot-reload with source-file tracking (`reloadShader` +
  `trackedShaderFiles`).
* Texture hot-reload (`reloadTexture` + `trackedTextureFiles`).
* Placeholder-safe API — `placeholderTexture()` is always available.
* Mesh presets: pos+color, pos+UV.

### Graphics — 2D drawing
* Unified `RenderItem` (mesh + material + transform + tint + UVs)
  drawn through `Renderer2D::draw` (batched) or `drawDirect` (bypass).
* `Renderer2DStats` — per-frame draw calls, quads, state changes,
  passes.
* Layered rendering with `RenderPass` (World / UI / Overlay) and
  named `Layers::*` constants.
* Blend modes: Alpha, Additive, Opaque.
* SDF and bitmap material support (`Material::useSDF`, `sdfSoftness`).
* SpriteBatch2D CPU batcher (used internally).

### Graphics — sprites & animation
* Two sprite-sheet APIs: `Sprite2D.h` (older, simple) and
  `SpriteRenderer2D.h` (newer, richer `Animator` + named clips).
* State-machine flipbook animator: clips, states, transitions
  (`Always` / `BoolEquals` / `Trigger` / `Finished`), bool vars,
  triggers, frame events, per-clip speed, global speed.
* Named animation-preset registry for reusable state-machine
  configurations.

### Graphics — cameras
* `Camera2D` — center + zoom + logical viewport.
* `CameraController` — follow target with smoothing, dead-zone,
  look-ahead, secondary targets (weighted midpoint), world bounds,
  zoom targeting, screen shake with trauma decay, scripted `focusOn`
  moves, pixel-snap toggle.

### Graphics — text
* TTF loader producing bitmap or SDF atlases via `Font::loadFromTTF` /
  `loadFromTTF_SDF`.
* `TextRenderer2D` — measure, align (H/V), max-width wrap, per-frame
  culling, animated text (typewriter, fade-in/out, scale, offset,
  velocity).
* Multi-font switching (`setActiveFont`).
* 8x8 debug bitmap font baked in.

### Graphics — debug
* `DebugDraw2D::rect` for filled / outlined debug boxes.

### Graphics — tilemaps
* JSON-loaded tilemaps with multiple tilesets and layers.
* Per-tile solidity, one-way, and slope metadata (`LeftUp`/`RightUp`).
* World-space tile size scaling (`tilePixelScale`).
* `TileMapRenderer` builds materials and renders layered.
* `TileMapLoader::sampleTileTopColors` extracts representative
  per-tile RGBA colors (used by MegaX for tile-colored dust /
  impact particles).

### Physics / collision
* AABB tile-collision helper with `moveAndCollide` (basic) and
  `moveAndCollideEx` (step-up, one-way disabling via previous-bottom,
  slopes).
* `MoveResult2D` — hit-X, hit-Y, grounded, ceiling, steppedUp flags.
* `Scene2D` integrates `TileCollision` via `setTileCollisionContext`
  and `Physics2DSettings` (gravity, sub-stepping).

### Particles
* Data-driven `EmitterConfig` — emission rate + duration + bursts
  (with repeat + interval), lifetime range, shape (Point / Circle /
  Ring / Rect / Line), velocity range, direction range, inherit
  velocity, gravity, drag, rotation, start / end size, start / end
  color with optional variation, world-space vs emitter-space, max
  particles, sort layer, additive or alpha blend.
* Multi-emitter `EffectDef` = layered emitters.
* `ParticleSystem::registerEffect` + fire-and-forget `spawn` or
  handle-based `spawnManaged` / `spawnAttached` (bound to Scene2D
  entity with offset).
* Stop / kill / setPosition / isAlive per handle.
* Batched rendering via `Renderer2D` with optional custom render
  callback.

### Scenes
* `Scene2D` — ECS-backed 2D scene with entity creation from render
  templates, sprite animator attachment, tile collision integration,
  physics-lite update, sub-stepping, culling, custom animation event
  callback.
* Full save / load via `SceneSerializer::saveToFile` / `loadFromFile`
  with pluggable asset-resolution callbacks for meshes, materials,
  sprite sheets, scripts, and animation presets.
* Prefab library — load prefab definitions from a directory,
  reference by name via `PrefabRefComponent`.
* Script registry — register named script factories, bound at
  serialization load time.

### Post-processing
* Chained fullscreen shader effects with up to 8 float params each.
* Runtime enable / disable / lookup / clear.
* Automatic scene-FBO and ping-pong FBOs; auto-resize on window
  resize.

### Profiling & diagnostics
* RAII CPU scope timing (`HBE_PROFILE_SCOPE`) with arbitrary nesting,
  inferred from scope lifetime — current / avg / min / max over a
  120-frame rolling window, zero allocations in the hot path.
* Compiled out entirely in Release (`HBE_PROFILE_ENABLED` → `((void)0)`),
  plus a runtime `SetEnabled` toggle and a `Reset` for scene reloads.
* Non-blocking GPU timer queries (`HBE_GPU_SCOPE`) read back three
  frames later, degrading gracefully where `ARB_timer_query` is absent.
* Renderer statistics: draw calls, passes, submitted / rendered quads,
  culled sprites, material and texture changes, visible tile chunks,
  post-process passes, active and shadow-casting lights, live particles.
* One combined `Profiler::Snapshot` per frame pairing the CPU section
  list, the GPU section list, and the renderer stats; plus an optional
  1-Hz log dump in Debug. No engine profiler window — by design.

### UI
* Immediate-mode UI with panels, scroll panels, labels, buttons,
  checkboxes, toggle buttons, float / int sliders.
* Themed via `UIStyle`; loadable from JSON via `UIThemeLoader`.
* Mouse position + wheel accessors.
* Bindable to `Renderer2D` + `DebugDraw2D` + `TextRenderer2D`
  backends.

---

## Appendix — how MegaX uses the engine (context)

MegaX is a Mega Man X-style platformer built entirely on top of the
above surface. It never modifies the engine — every game feature is
built by composing engine APIs from a `GameLayer` (regular layer)
plus a handful of game classes:

* `GameLayer` — `Layer` subclass. Owns `World`, `Player`, `EnemyManager`,
  `BulletManager`, `Effects`, `CameraController`, `TextRenderer2D`,
  `FileWatcher`.
* `World` — wraps `TileMap`, `TileMapRenderer`, and animated tiles.
* `Player` — uses `SpriteRenderer2D::Animator`, `TileCollision::moveAndCollideEx`.
* `Enemy` / `EnemyManager` — same pattern; uses `EmitterConfig` fire
  callback + `Effects` for muzzle flash / explosion.
* `Effects` — thin wrapper around `ParticleSystem` that registers all
  gameplay effects by name.
* Hot reload uses `FileWatcher` + `ResourceCache::reloadShader` /
  `reloadTexture` + custom `World::reload`.

This means every gameplay feature added to MegaX is a new file inside
`MegaX/src/Game/` — no engine edits, no lib rebuilds.
