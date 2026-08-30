# Where file-format I/O lives in `toolboxes/`

Authoritative rule for placing readers / writers of any on-disk file
format inside the numkit codebase. Read this **before** adding a new
format implementation (TIFF reader, HDF5 binding, Parquet writer, etc.).

The rule is a placement convention, not an architectural constraint —
the C++ namespaces and CMake structure mirror it but don't enforce it.

---

## The rule

A format implementation belongs to the **domain library that owns the
data type it produces or consumes**, not to a separate "formats" library.

| Domain   | Format I/O lives in        | Public C++ namespace        | Example formats |
| -------- | -------------------------- | --------------------------- | --------------- |
| Image    | `toolboxes/image/src/io/`       | `numkit::image`             | TIFF, PNG, JPEG, BMP, GIF, HDR, PNM, TGA, EXR (future) |
| Audio    | `toolboxes/audio/src/io/`       | `numkit::audio` (future)    | WAV, FLAC, OGG, MP3 (future) |
| Video    | `toolboxes/video/src/io/`       | `numkit::video` (future)    | MP4, AVI, MOV (future) |
| Workspace| `toolboxes/io/src/workspace/`   | `numkit::io`                | `.mat`, `.npy`, `.npz` (future) |
| Tabular  | `toolboxes/io/src/text/` *or* `toolboxes/io/src/tabular/` | `numkit::io` | CSV, TSV, Parquet (future), Arrow (future) |
| Scientific generic | `toolboxes/io/src/scientific/` (when added) | `numkit::io` | HDF5, NetCDF, Zarr |
| Raw bytes / streams | `toolboxes/io/src/file_io/` | `numkit::io`                | `fopen`/`fread`/`fwrite`/etc. |
| Paths     | `toolboxes/io/src/paths/`       | `numkit::io`                | `fileparts`, `fullfile`, `tempname` |

Public API (the user-callable function name, e.g. `imread` / `audioread`)
is registered as a `compat::<fn>` alias in addition to the domain
namespace — that part of the convention has not changed.

---

## Decision tree

When adding a new format implementation, ask:

1. **Does the format primarily contain *one* domain's data?**
   *Yes* → put it in that domain's `io/` subdirectory.
   - JPEG / PNG / TIFF → `toolboxes/image/src/io/`
   - WAV / FLAC → `toolboxes/audio/src/io/`
   - MP4 → `toolboxes/video/src/io/`

2. **Does the format carry mixed / general numeric / tabular / workspace data?**
   *Yes* → put it in `toolboxes/io/`, picking the right sub-area:
   - Tabular (rows × cols of mixed types) → `toolboxes/io/src/tabular/` (create
     if needed) or `toolboxes/io/src/text/` for purely-text formats.
   - Scientific containers (HDF5, NetCDF, Zarr, BlobStore) →
     `toolboxes/io/src/scientific/` (create when first one lands).
   - Workspace dump (whole-state save/load) → `toolboxes/io/src/workspace/`.

3. **Does the format wrap a raw byte stream with no structural opinions?**
   *Yes* → it belongs in `toolboxes/io/src/file_io/` (this is where `fopen`,
   `fread`, `fwrite`, etc. live). Don't put a *format* here — only the
   low-level POSIX-like building blocks.

If a single format genuinely spans two domains (rare — e.g. a hypothetical
"audio embedded in image" container), put the bulk of the code in the
domain whose decoder is harder, and have the secondary domain depend on
it through the public header.

---

## Current layout (verified at this commit)

Use this as the model for new additions.

```
toolboxes/io/                                  general-purpose I/O
  src/file_io/fileio.cpp                  fopen / fread / fwrite / fprintf / fscanf / fseek / ftell / frewind
  src/paths/paths.cpp                     fileparts / fullfile / tempname / tempdir / which
  src/text/csv.cpp                        csvread / csvwrite / dlmread / dlmwrite
  src/text/extras.cpp                     readlines / writelines / fileread / writematrix / readmatrix
  src/workspace/saveload.cpp              save / load dispatcher
  src/workspace/saveload_mat.cpp          .mat binary I/O (matio)

toolboxes/image/                               image processing + image format I/O
  src/io/io.cpp                           imread / imwrite / imfinfo dispatcher
  src/io/stb_impl.cpp                     stb_image / stb_image_write (PNG/JPEG/BMP/GIF/HDR/PNM/TGA)
  src/io/tiff_reader.cpp                  TIFF reader (classic + BigTIFF)
  src/io/tiff_writer.cpp                  TIFF writer
  src/color/...                           color-space conversions
  src/filter/...                          imfilter, medfilt2, modefilt, …
  …                                        rest of image lib

toolboxes/audio/                               audio analysis (no audio-format I/O yet)
  src/features/...                        pitch, harmonicRatio, …
  src/spectral/...                        mfcc, melSpectrogram, …
  src/scale/...                           hz2mel, mel2hz, …
  …                                        (an audioread/audiowrite would land in src/io/)
```

---

## Why the convention is per-domain, not a unified `toolboxes/formats/`

Considered and rejected during the discussion that produced this rule
(see the git log around the time this file was added):

1. **A format and its consumers travel together.** The TIFF reader
   produces `numkit::Value` shaped like an H×W×C image; its only sensible
   caller is image-processing code. Pulling it into `toolboxes/formats/` would
   force every image-processing user to also pull in unrelated formats
   (audio, tabular). The domain split is the natural unit of dependency.

2. **Dependency graphs stay tight.** `toolboxes/image` brings `stb_image` +
   `zlib` (for TIFF Deflate). `toolboxes/io` brings `matio` (for .mat) and
   has its own zlib path. Cross-cutting a `toolboxes/formats` would force one
   place to own all those deps, complicating WASM / portable builds where
   we sometimes drop a dependency.

3. **Discoverability follows mental model.** A reader looking for "the
   TIFF code" already starts in `toolboxes/image` because TIFF is an image
   format. Pulling it to `toolboxes/formats/image/tiff_reader.cpp` adds a
   layer with no information gain.

4. **`toolboxes/io/` already exists and works.** It's the right home for the
   format families that *don't* belong to one domain (CSV, .mat, future
   HDF5/Parquet/Zarr). Keep that role focused; don't grow it into a
   second image library.

---

## When to revisit

This convention is intentionally lightweight. Revisit it only if one of
these triggers fires:

- A format truly serves **three or more** domains and feels arbitrary in
  any single one (e.g. a hypothetical universal scientific container
  carrying images + audio + tabular).
- We get a public-API user explicitly asking for a slimmer subset
  (e.g. "give me just the TIFF reader without the rest of image lib").
- A new sub-project sprouts that doesn't fit any existing domain at all.

Until then, the per-domain rule stands.
