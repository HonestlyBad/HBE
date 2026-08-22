# NN — Build, run, verify

<!--
TEMPLATE. Always the last doc in the set.
Model: Docs/13-HBE-CPU-Frame-And-System-Timing/05_build_run_and_verify.md
-->

You've now touched every file listed in `00_overview.md` §4:

* **NEW** `<Project>/include/HBE/<Area>/<Name>.h`
* **NEW** `<Project>/src/<Area>/<Name>.cpp`
* **EDIT** `<Project>/include/HBE/<Area>/<Existing>.h`
* **EDIT** `<Project>/src/<Area>/<Existing>.cpp`
* **EDIT** `<Project>/CMakeLists.txt`
* **EDIT** `MegaX/src/Game/GameLayer.cpp` (verification only — *or*
  required, if the engine change would otherwise break the build)

No new files in <the projects that got none>. No changes to
<projects untouched>. No `.vcxproj` / `.slnx` edits — CMake is the
only build system.

---

## 1. Pre-build sanity check

```fish
cd /home/atulo/Projects/HBE

grep -c "<Name>" <Project>/CMakeLists.txt
# -> 1  (the <ClCompile-equivalent> entry: src/<Area>/<Name>.cpp)

grep -n "<Symbol>" <Project>/src/<Area>/<Existing>.cpp
# -> <N> matches: <name each>

grep -c "<Macro>" MegaX/src/Game/GameLayer.cpp
# -> <N>
```

<Any "if you accidentally touched X, revert it" note.>

---

## 2. Build

```fish
cmake --build --preset linux-clang-debug --target MegaX
```

Expected recompiles:

```
[N/M] Building CXX object <Project>/CMakeFiles/<Project>.dir/src/<Area>/<Name>.cpp.o
[N/M] Building CXX object <Project>/CMakeFiles/<Project>.dir/src/<Area>/<Existing>.cpp.o
[N/M] Building CXX object MegaX/CMakeFiles/MegaX.dir/src/Game/GameLayer.cpp.o
[M/M] Linking CXX executable bin/Debug/MegaX
```

Only pre-existing warnings should appear. If you see:

* **``fatal error: 'HBE/<Area>/<Name>.h' file not found``**
  — the header wasn't saved to the path in doc `01` §1.

* **``undefined reference to 'HBE::<NS>::<Symbol>(...)'``**
  — `<Name>.cpp` isn't in the source list in `<Project>/CMakeLists.txt`
  (doc `02` §2).

* **``error: no member named '<Member>' in namespace 'HBE::<NS>'``**
  — the `#include` from doc `0X` §1 is missing.

* **``warning: unused variable '<var>' [-Wunused-variable]``** in
  Release — <the compile-time gate isn't expanding as intended>.

* **``ninja: error: '<file>.cpp', needed by ..., missing``**
  — the file is listed in CMake but not on disk, or vice versa. Check
  the exact spelling and re-run `cmake --preset linux-clang`.

### Release build check

```fish
cmake --build --preset linux-clang-release --target MegaX
```

Expected:

* Builds clean.
* <What must differ in Release — silenced output, macros expanded to
  `((void)0)`, no measurable overhead.>

---

## 3. Run (Debug)

```fish
./build/linux-clang/bin/Debug/MegaX
```

Expected startup log lines (unchanged from item N-1):

```
[INFO ] <existing startup lines>
```

<What new output appears, when, and how often:>

```
<the expected new output block>
```

Exact numbers vary by machine. Structure must match:

* <structural invariant 1>
* <structural invariant 2>
* <structural invariant 3>

---

## 4. Verify checklist

### Group A — Item N-1 regression (nothing new pressed)

* [ ] Walk, jump, shoot — same feel as item N-1.
* [ ] All existing particle effects still fire.
* [ ] `F1`/`F2`/`F3` difficulty switching still works.
* [ ] `F5` scene reload still works.
* [ ] `F8` profiler snapshot still prints.

### Group B — <new feature basics>

* [ ] <observable check>
* [ ] <observable check>

### Group C — <correctness relationships>

* [ ] <numeric or structural check, with tolerance>

### Group D — <runtime toggles / edge cases>

* [ ] <check>

### Group E — <interaction with earlier features>

* [ ] <check — hot reload, difficulty change, respawn>

### Group F — Release build

* [ ] Release runs.
* [ ] <what must be absent or unchanged in Release>

---

## 5. Tuning table — the flavor knobs

<!--
REQUIRED for any item that changes gameplay feel or balance — present
in 9 of the 10 gameplay items (04-12). Omit ONLY for pure
infrastructure items with no tunable output (as 13/14 did).
Read every current value out of the code; never invent one.
-->

<Where the knobs live and how to iterate — e.g. "everything here lives
in `Effects.cpp`'s `make<name>()` factories (edit + F5 to iterate)".>

### <Cadence / intensity>

| Symptom | Field | Direction |
|---|---|---|
| <feel complaint, phrased as the user would say it> | `<exact field>` | `<current> → <suggested>` |
| <feel complaint> | `<exact field>` | `<current> → <suggested>` |

### <Color>

| Symptom | Field | Direction |
|---|---|---|
| <feel complaint> | `<exact field>` | `<current> → <suggested>` |

### <Layout / anchor>

| Symptom | Field | Direction |
|---|---|---|
| <feel complaint> | `<exact field>` | `<current> → <suggested>` |

<Any ordering constraint on applying these — e.g. per-enemy overrides
must happen before `snapshotBaseStats()` so the multiplier lands on
the intended base.>

---

## 6. Troubleshooting matrix

| Symptom | Likely cause | Fix |
|---|---|---|
| <symptom> | <cause> | <specific fix, naming the doc and section> |
| <symptom> | <cause> | <fix> |
| <symptom> | <cause> | <fix> |

<!-- Aim for 10-15 rows. One per failure the user could realistically
hit, including "it built but nothing happens" and hardware/driver
cases. This is the highest-value section in the set. -->

---

## 7. Done-criterion repro

Item NN, `Docs/WorkItems.txt`: *"<the done criterion, quoted
verbatim>"*

You have satisfied it if <the precise observation>:

1. <step / observed value>
2. <step / observed value>
3. <step / observed value>

<Sentence stating the margin by which the bar is cleared.>

---

## 8. What comes after this item

Item NN+1 (`<title from WorkItems.txt>`) <what it adds on top and
which part of this API it consumes>.

Item NN+2 (`<title>`) <same>.

<Why this item's API was kept minimal, in terms of what the next
items need from it.>

---

## 9. Cleanup

<Any debug toggle left on deliberately, and when to turn it off. What
must NOT be deleted because a later item reuses the pattern.>

Item NN is complete.
