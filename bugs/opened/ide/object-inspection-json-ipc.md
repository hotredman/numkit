# ide.protocol — classdef objects are opaque in the IDE: the JSON IPC inspector emits a "[1x1 object]" placeholder instead of properties

- **Status:** 🔴 OPEN
- **Severity:** P2 (objects are unviewable in the IDE workspace browser; struct/cell drill-in works, objects do not)
- **Kind:** bug
- **Found:** 2026-09-05 (user report: "the IDE has no object viewing support"; verified live through the real IPC pipe)

## Symptom (verified on the live `--ide-session` pipe)

A classdef instance in the workspace serializes as a matrix with a TEXT
PLACEHOLDER — no properties, no drill-in, no class name:

```
__PATH_DATA__:{"kind":"matrix","type":"object","rows":1,"cols":1,"data":[["[1x1 object]"]]}
```

while a struct in the same workspace gets the full inspector payload:
`{"kind":"struct","fields":["f"],"elems":[{...,"drill":true,"stats":{...}}]}`.

## Repro (self-contained — the real IDE protocol)

```
{ cat <<'EOF'
classdef Box
    properties
        a = 1
        b = [1 2 3]
    end
end
x = Box();
x.a = 42;
EOF
echo '__END_OF_INPUT__'
echo '__INSPECT_PATH__:x'
echo '__QUIT__'
} | build/windows/release/apps/numkit/Release/numkit_repl.exe --ide-session
```

## Root cause — missing OBJECT branch in the JSON IPC (three sites)

1. `emitInspectPayload` handles only struct / cell / else-matrix, in BOTH
   serialization sites that must stay format-identical:
   - `apps/numkit/ide_serializer.hpp` (~line 203, native `--ide-session`)
   - `wasm/src/repl_bindings.cpp` (~line 279, the WASM engine the IDE uses)
   → OBJECT falls into the matrix branch; `valuePreview` stringifies it as
   "[1x1 object]".
2. `resolveInspectPath` (ide_serializer.hpp + wasm twin) walks only struct
   fields / cell elements — no object-property step, so even a proper
   payload would not be navigable (`x.a` paths fail).
3. `mtypeName(OBJECT)` returns the generic "object" — `objectClassName()`
   exists but is never used in the serialization path.

## Suggested fix

Add an OBJECT branch to both `emitInspectPayload` twins:
`{"kind":"object","class":"Box","rows":R,"cols":C,"elems":[[cell per
element]]}` where each cell lists the class properties (names, sizes,
previews, drill into nested values) — mirroring the struct payload so the
IDE UI can reuse its struct tree. Add the property step to
`resolveInspectPath` (`f`-kind step against the object's property list).
Keep the two sites byte-compatible (the header comment mandates it).

## References

- **Guard:** `DISABLED_ObjectInspectionPayload` in
  `src/core/tests/ide_serializer_test.cpp` (asserts `kind:"object"` +
  class name + property `a` visible for the Box repro above).
