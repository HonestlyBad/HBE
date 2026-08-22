# NN — <The data file> (`assets/<path>/<file>.json`)

<!--
TEMPLATE for a doc that defines or changes a data file format:
JSON maps, CSV capture layouts, item/loot tables, room templates,
biome profiles.
Model: Docs/02-World-From-Tilesets/02_map_json.md (new format)
       Docs/03-Animated-Tiles/02_map_json.md   (edit to a format)
-->

Create `/home/atulo/Projects/HBE/MegaX/assets/<path>/<file>.json` with
the content below. <What it represents, its dimensions/scale, and why
this particular content was chosen — e.g. "uses all three tilesets so
you can confirm the multi-tileset pipeline works". Say it is editable
freely afterwards.>

---

## 1. Schema (what each field means)

```jsonc
{
  "version": 1,
  "<field>":  <value>,        // <units, range, what consumes it>
  "<field>":  <value>,        // <units, range, what consumes it>

  "<array>": [                // <what the index means downstream>
    {
      "<field>": <value>,     // <meaning; note if path is relative and to what>
      "<field>": [ ... ]      // <1-based? 0-based? which system reads it>
    }
  ],

  "<reserved>": []            // reserved for item NN (engine ignores it today)
}
```

### Critical authoring rules

<!-- Every convention that silently corrupts the file when broken.
     Bold the load-bearing half of each rule. -->

- **<Origin / ordering rule.>** <e.g. "`data` is row-major, bottom-left
  origin — the first row of numbers is the BOTTOM row of the world.">
- <Indexing base:> ids are **<1-based / 0-based>** and **local to
  <what>** (`0` = <meaning>).
- <Path resolution:> `<field>` is resolved **relative to <what>**.
- <Size invariant:> `<field>` length must equal `<expression>`
  (here `<worked example>`).
- <Naming convention the loader depends on.>

---

## 2. Full file

<!-- The COMPLETE file, pasteable. No elisions. -->

```json
<the entire file>
```

---

## 3. What this produces

<!-- Read the data back in plain language. -->

- **`<entry>`** (<role>): <what it draws / does and where>.
- **`<entry>`** (<role>): <same>.

<The resulting order / layering / precedence, and why it comes out that
way from the data above.>

---

## 4. Designing your own

- <Where to find valid ids / values — tileset previews, an enum, a
  registry.>
- <Structural style rule, e.g. "keep one layer per tileset".>
- <The quick sanity check that catches the most common mistake — e.g.
  "your floor row is the **first** `data` row".>
- <Which field to touch to opt into a behavior added by a later item,
  naming the item number.>

---

## 5. Sanity check

```fish
# Valid JSON?
python3 -m json.tool /home/atulo/Projects/HBE/MegaX/assets/<path>/<file>.json > /dev/null; echo $status
# -> 0

# Size invariant holds?
python3 -c "import json;d=json.load(open('<path>'));print([len(l['data'])==l['w']*l['h'] for l in d['layers']])"
# -> all True
```

<Whether the game should load this successfully at this point, and what
log line proves it.>

Next: `NN_<name>.md`.
