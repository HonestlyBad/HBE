---
description: Write the step-by-step markdown doc set for an HBE / MegaX work item
argument-hint: <item number, e.g. 15> or <free description of an unlisted item>
allowed-tools: Bash(ls:*), Bash(awk:*), Bash(git status:*), Bash(git log:*)
---

Author the complete work item doc set for **$ARGUMENTS**.

Every check, grep, and build in this file is **your** work, not the user's.
They typed one line; do not hand any of it back to them as a step to run. The
only thing they do is read the finished docs and type the code.

Invoke the `hbe-workitem` skill and follow it. It carries the doc format, the
project map, build commands, conventions, and templates. `CLAUDE.md` carries the
hard rules. `Docs/13-HBE-CPU-Frame-And-System-Timing/` and
`Docs/14-HBE-GPU-Timing-And-Renderer-Stats/` are the reference for depth, tone,
and precision.

## Preflight (already gathered — don't re-run these)

Existing work item directories:

!`ls -1 Docs/ | grep -E '^[0-9]'`

`WorkItems.txt` entry for this argument (empty means it is not in the backlog —
follow the unlisted-item path below):

!`awk -v n="$ARGUMENTS" '$0 ~ "^"n"\\." {f=1;print;next} f && /^[0-9]+\./ {exit} f {print}' Docs/WorkItems.txt`

Working tree state:

!`git status --short`

## Resolving the argument

**If `$ARGUMENTS` is a number** — that is a `Docs/WorkItems.txt` item. Read its
entry, note its `[HBE]` / `[MEGAX]` tag, and use its title for the directory
name.

**If `$ARGUMENTS` is a description** — this item predates or sits outside
`WorkItems.txt`, which is normal for engine work that came up mid-flight. Then:

1. Number it as the next unused `Docs/NN-` prefix from the listing above.
2. Derive a Kebab-Case title, prefixed `HBE-` if the work is engine-side.
3. Decide the tag yourself from which projects the work touches, state that
   decision in the overview, and scope the docs to it exactly as if it were
   tagged in the file.
4. Do **not** append it to `WorkItems.txt` — that file is the planned backlog,
   not a log of everything done. Say in your report that it isn't listed there.

If the number already has a directory, this is an **update**, not a new item —
read what's there and revise in place rather than starting over.

## Before writing

1. Read the item's entry (or interpret the description) and fix the tag scope.
   The tag is **not symmetric**:
   - `[MEGAX]` — `MegaX/` only. Never touch an `HBE.*` project. If the item
     can't be done without an engine change, stop and tell me.
   - `[HBE]` — the engine projects, **and keep MegaX working**. Once you know
     which symbols the change touches, run the impact grep yourself — it can't
     be preflighted, since the symbol list doesn't exist until you've planned
     the change:

     ```fish
     grep -rn "<Symbol>\|<Type>\|<MACRO>" /home/atulo/Projects/HBE/MegaX/
     ```

     Hits mean updating MegaX is **required** and part of this item. No hits
     still leaves the verification pass, which is expected. Either way the
     MegaX edits go in their own doc, are labelled in the files-touched table
     (`verification only` vs `required — API changed`), and never push game
     logic into HBE.
   - `HBE.Sandbox/` and `HBMapMaker/` are off-limits unless I named them.
2. Read the previous item's `00_overview.md` §1 so your opening names what it
   finished. Read items 00–12 for content only — imitate 13/14 for structure.
3. **Read every file you will instruct an edit to, in full.** Every line number
   you cite must come from a read you did this session.
4. Check the APIs you will compose in `Docs/GameEngine/ENGINE_API_REFERENCE.md`,
   then confirm each signature in the real header. The header wins.
5. Decide the conditional sections before you start — tuning table, controls,
   asset notes, data-format doc. Skipping these on a gameplay item is the most
   likely way to ship a doc set that looks right and is missing what I use most.

## While writing

- Linux absolute paths, fish for every shell snippet, prose wrapped at ~60
  columns.
- Complete compilable code blocks — no `// ...`, no "add the rest".
- Anchor every insertion point by line number **and** the text at that line.
- New `.cpp` files get a `CMakeLists.txt` entry. Never mention `.vcxproj` or
  `.slnx`.
- Don't pull work forward from later items. Name the item that owns anything
  deferred.

## After writing

- Prove the pasteable code compiles: copy the tree to the scratchpad, apply the
  edits there, build it. Never test in my working tree. Fix the docs, not just
  the copy.
- Re-check `00_overview.md` §4 against what the other docs actually edit, and
  re-check every line number for a file an earlier doc in the same set shifted.
- Report the directory, the doc list with one line each, and the reading order.
- **Do not implement the item.** I type the code.
