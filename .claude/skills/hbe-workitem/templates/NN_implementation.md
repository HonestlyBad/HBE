# NN — <Short description> (`HBE/<Area>/<Name>.h`)

<!--
TEMPLATE. Use the sections that apply, in this order; drop the rest.
Prose wraps at ~60 columns. Linux absolute paths. fish snippets.
Model: Docs/13-*/01_profiler_api.md (new file)
       Docs/13-*/03_application_integration.md (edits)
-->

This doc <creates / edits> <files>. <What it depends on from the
previous doc. What it deliberately leaves out. For a new header: what
it may include and why that matters.>

---

## 1. Create the file

**Full path:** `/home/atulo/Projects/HBE/<Project>/include/HBE/<Area>/<Name>.h`

**Status:** file must not exist yet. If it does exist, stop and check —
someone else likely started this work item.

Paste the following content **verbatim** (no reformatting):

```cpp
<THE ENTIRE FILE. Not a sketch. Comments included — they explain the
non-obvious choices and are part of the deliverable. Indentation must
match its project: tabs under HBE.Core, 4 spaces for new
CLion-authored files. Be consistent within the file.>
```

> **PASTE NOTE.** <Anything an editor may mangle: macro
> line-continuation backslashes, tabs vs spaces, a trailing blank
> line. Show both acceptable forms if there are two.>

Do **not** add any other includes to this header. <Why — e.g. it is
pulled into `Application.h`, which every layer sees.>

---

## 2. Register the source in `<Project>/CMakeLists.txt`

<!-- Only for a new .cpp. Headers need no registration. -->

Open `/home/atulo/Projects/HBE/<Project>/CMakeLists.txt`.

The source list inside `add_library(<Project> STATIC ...)` is
alphabetical within each subdirectory group. Insert between
`src/<Area>/<Before>.cpp` (line X) and `src/<Area>/<After>.cpp`
(line Y):

```cmake
    src/<Area>/<Name>.cpp
```

The final block should read:

```cmake
add_library(<Project> STATIC
    <the complete patched source list>
)
```

Headers are found through `target_include_directories` — there is
nothing to register for `<Name>.h`.

---

## 3. `<ExistingFile>.h` — <what changes>

Open `/home/atulo/Projects/HBE/<Project>/include/HBE/<Area>/<ExistingFile>.h`.

The <include block / declaration> currently reads (lines A-B):

```cpp
<the exact current text>
```

Insert **right after** `<anchor line text>` (line A) and **before**
`<next line text>` (line A+1):

```cpp
<the new text alone>
```

Final block should read:

```cpp
<the complete patched region>
```

Nothing else in `<ExistingFile>.h` changes.

> **WHY HERE?** <The reasoning, so the user understands rather than
> just types.>

---

## 4. `<ExistingFile>.cpp` — <what changes>

Open `/home/atulo/Projects/HBE/<Project>/src/<Area>/<ExistingFile>.cpp`.

### 4.1 Includes

<Either "nothing new is needed here, `<Name>.h` is already reachable
through `<Header>.h` — do **not** add a duplicate include", or the
exact include to add with its anchor.>

### 4.2 <The function being changed>

`<Function>` runs from line **A** to line **B**. We are changing
**<N>** locations inside it:

1. <change, one line>
2. <change, one line>
3. <change, one line>

**Do NOT** <the adjacent-looking change that must not be made, and
why>.

Replace lines **A-B** inclusive with the following:

```cpp
<the ENTIRE patched function or loop body — every line, including the
untouched ones, so this is one clean replace rather than N hunks>
```

<Sentence naming what outside that range is unchanged.>

---

## 5. What NOT to touch

* Do **not** <plausible over-application of the pattern> — <why it
  breaks or degrades things>.
* Do **not** <edit adjacent file that looks related> — <that is item
  NN's scope>.
* Do **not** <call the new API from the wrong place> — <consequence>.
* Do **not** <thread it into an unrelated subsystem>.

---

## 6. Sanity check before doc NN+1

```fish
test -f /home/atulo/Projects/HBE/<Project>/include/HBE/<Area>/<Name>.h; echo $status
# -> 0

grep -c "<Name>.cpp" /home/atulo/Projects/HBE/<Project>/CMakeLists.txt
# -> 1

grep -n "<Symbol>" /home/atulo/Projects/HBE/<Project>/src/<Area>/<ExistingFile>.cpp
# -> <exact expected count> matches: <what they are>
```

<Say whether the project should compile at this point.>

<!-- If it should NOT yet compile, say so plainly: -->
Do not attempt to build yet — `<Name>.h` declares
`<symbols>`, which are not implemented until doc `NN+1`.

<!-- If it should compile: -->
Try building just this target:

```fish
cmake --build --preset linux-clang-debug --target <Project>
```

Expected: builds clean, no new warnings from `<Name>.h/.cpp`.

If it fails with ``fatal error: 'HBE/<Area>/<Name>.h' file not found``,
the header wasn't saved to the path in §1.

If it fails with ``undefined reference to 'HBE::<NS>::<Symbol>'``, the
`.cpp` is missing from `CMakeLists.txt` (§2).

Next: `NN_<name>.md`.
