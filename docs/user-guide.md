# ConvertMesh (`cmesh`) User Guide

`cmesh` is a stack-based command-line pipeline for medical-imaging mesh and
image processing, modeled on [Convert3D (`c3d`)](https://sourceforge.net/projects/c3d/).
A single invocation is a list of tokens evaluated strictly left to right. Each
token is either a **filename** (loaded onto the stack) or a **`-command`**
(consumes zero or more following arguments and usually transforms the stack).

```
cmesh [ARG | -COMMAND [ARGS...]] ...
```

This guide documents every command in the c3d reference style: a synopsis, a
description, its options, and at least one worked example. For a terse summary
at the terminal, run `cmesh --help`.

---

## Contents

- [Concepts](#concepts)
  - [The stack](#the-stack)
  - [Filename auto-detection](#filename-auto-detection)
  - [Coordinate conventions](#coordinate-conventions)
- [Global & sticky flags](#global--sticky-flags)
  - [`-verbose`](#-verbose) · [`-no-warn`](#-no-warn) · [`-discard-data`](#-discard-data)
  - [`-use-vtk` / `-use-vcg` / `-use-gpu`](#-use-vtk----use-vcg----use-gpu)
  - [`-int` / `-interpolation`](#-int----interpolation)
  - [`-h` / `--help`](#-h----help) · [`--version`](#--version)
- [I/O commands](#io-commands)
  - [`-o`](#-o) · [`-omesh`](#-omesh) · [`-oimage`](#-oimage)
  - [`-push-mesh`](#-push-mesh) · [`-push-image`](#-push-image)
- [Stack commands](#stack-commands)
  - [`-pop`](#-pop) · [`-dup`](#-dup) · [`-swap`](#-swap) · [`-clear`](#-clear)
  - [`-as`](#-as) · [`-popas`](#-popas) · [`-push`](#-push)
- [Mesh operations](#mesh-operations)
  - [`-extract-isosurface`](#-extract-isosurface)
  - [`-smooth-mesh`](#-smooth-mesh)
  - [`-decimate`](#-decimate)
  - [`-compute-normals`](#-compute-normals)
  - [`-flip-normals`](#-flip-normals)
  - [`-meshdiff`](#-meshdiff)
- [Image / mesh interop](#image--mesh-interop)
  - [`-rasterize`](#-rasterize)
  - [`-warp-mesh`](#-warp-mesh)
  - [`-sample-image`](#-sample-image)
  - [`-merge-array`](#-merge-array)
- [Worked pipelines](#worked-pipelines)

---

## Concepts

### The stack

`cmesh` holds a LIFO **stack** of data items. Each item is one of three *kinds*:

| Kind    | Produced by                                  |
|---------|----------------------------------------------|
| mesh    | reading a mesh file, or any mesh operation   |
| image   | reading an image file, or `-rasterize`       |
| ugrid   | reading an unstructured grid (output only)   |

Most commands operate on the **top** of the stack: they pop their input(s),
compute, and push the result. Commands that need two inputs (e.g.
`-sample-image`) expect a specific stack ordering, documented in their entry.
Because evaluation is strictly left to right, a pipeline reads like a recipe.

### Filename auto-detection

A bare token **not** prefixed with `-` is treated as a filename and pushed onto
the stack. The kind is inferred from the extension:

- **Mesh:** `.vtk`, `.vtp`, `.stl`, `.obj`, `.ply`, `.byu`, `.y`
- **Image:** `.nii`, `.nii.gz`, `.mha`, `.mhd`, `.nrrd`, `.nhdr`, `.gipl`,
  `.img`, `.hdr`, `.png`, `.jpg`/`.jpeg`, `.tif`/`.tiff`

If the extension is unrecognized, `cmesh` errors out. Use `-push-mesh` /
`-push-image` to override detection.

### Coordinate conventions

Meshes on the stack are in **NIFTI RAS** space (the cmrep / 3D-Slicer
convention). ITK images are read and stored in their native LPS convention; the
RAS↔LPS conversion happens automatically at every mesh↔image boundary, so you
never flip signs yourself. If a mesh from another tool comes out X/Y-mirrored,
that tool likely wrote LPS — apply a `diag(-1, -1, 1)` transform before pushing.

---

## Global & sticky flags

These do not touch the stack. **Sticky** flags set a mode that applies to all
*subsequent* operations until changed; they can be toggled mid-pipeline.

### `-verbose`

```
-verbose
```

Print a progress line for each operation as it runs. Useful for diagnosing
which step in a long pipeline is slow or failing.

```sh
cmesh -verbose seg.nii.gz -extract-isosurface 0.5 -decimate 0.5 -o out.vtp
```

### `-no-warn`

```
-no-warn
```

Silence data-loss warnings: the dropped-array warnings described under
`-discard-data`, and `-warp-mesh`'s out-of-extent vertex warning. Errors
are still reported.

### `-discard-data`

```
-discard-data
```

Acknowledge that subsequent operations may drop `vtkPolyData` arrays.
When an operation's output is missing point/cell data arrays that its
input carried (e.g. `-decimate` discarding a cell array), `cmesh` prints
a warning naming the lost arrays; `-discard-data` declares the loss
intentional and suppresses those warnings.

```sh
cmesh annotated.vtp -discard-data -decimate 0.5 -o light.vtp
```

### `-use-vtk` / `-use-vcg` / `-use-gpu`

```
-use-vtk      (sticky, default)
-use-vcg      (sticky)
-use-gpu      (sticky, reserved)
```

Select the backend preferred by subsequent operations. The flag is read by each
operation when it runs, so you can mix backends in one pipeline.

- `-use-vtk` — VTK-backed implementations (the default).
- `-use-vcg` — VCG-backed implementations where available; currently honored by
  `-decimate` (when built with VCG support), otherwise falls back to VTK.
- `-use-gpu` — reserved for future GPU backends; currently a no-op fallback.

```sh
cmesh a.vtp -decimate 0.5 -use-vcg -decimate 0.5   # first VTK, then VCG
```

### `-int` / `-interpolation`

```
-int MODE
-interpolation MODE
```

*(sticky)* Set the interpolation mode used by `-sample-image`. `MODE` is one of:

| MODE | Aliases | Use for |
|------|---------|---------|
| `linear` *(default)* | — | Continuous intensity images. |
| `nn` | `nearest`, `nearestneighbor` | Label maps / segmentations. |
| `bspline` | — | Smooth higher-order resampling. |

Any other `MODE` is a parse error. (More generally, numeric arguments are
validated strictly throughout: a token that is not entirely a number, e.g.
`-decimate abc`, is a parse error rather than a silent zero.)

```sh
cmesh surf.vtp labels.nii.gz -int nn -sample-image LabelID -o out.vtp
```

### `-h` / `--help`

```
-h | -help | --help
```

Print the built-in usage summary and exit.

### `--version`

```
--version | -version
```

Print the version string and exit.

---

## I/O commands

### `-o`

```
-o FILE
```

Write the **top** of the stack to `FILE`, choosing the writer from the top
item's kind (mesh vs. image) and the file extension. This is the
general-purpose "save" command; the format is whatever `FILE`'s extension
implies (`.vtp`, `.stl`, `.ply`, `.nii.gz`, …). The stack is left unchanged.

```sh
cmesh seg.nii.gz -extract-isosurface 0.5 -o surface.vtp
cmesh surface.vtp -o surface.stl          # format conversion by extension
```

### `-omesh`

```
-omesh FILE
```

Write the top item to `FILE`, **requiring** it to be a mesh or ugrid. Use this
(instead of `-o`) when you want an error rather than a surprise if the top of
the stack is unexpectedly an image.

```sh
cmesh in.vtk -smooth-mesh 20 -omesh out.stl
```

### `-oimage`

```
-oimage FILE
```

Write the top item to `FILE`, **requiring** it to be an image. The companion to
`-omesh` for image outputs such as rasterized masks.

```sh
cmesh surf.vtp -rasterize --spacing 0.5 0.5 0.5 -oimage mask.nii.gz
```

### `-push-mesh`

```
-push-mesh FILE
```

Read a mesh file and push it onto the stack, **bypassing** extension
auto-detection. Use this to force mesh interpretation of a file whose extension
`cmesh` would not otherwise recognize as a mesh.

```sh
cmesh -push-mesh shape.dat -o shape.vtp
```

### `-push-image`

```
-push-image FILE
```

Read an image file and push it, bypassing extension auto-detection.

```sh
cmesh -push-image scan.dat -oimage scan.nii.gz   # force image interpretation
```

---

## Stack commands

These rearrange the stack without computing anything. Variables let you set an
item aside and reuse it later in the pipeline.

### `-pop`

```
-pop
```

Discard the top item.

### `-dup`

```
-dup
```

Duplicate the top item (the copy becomes the new top).

### `-swap`

```
-swap
```

Swap the top two items. Errors if fewer than two items are on the stack.

### `-clear`

```
-clear
```

Empty the stack entirely.

### `-as`

```
-as NAME
```

Save the top item to variable `NAME` and **keep** it on the stack.

### `-popas`

```
-popas NAME
```

Save the top item to variable `NAME` and **pop** it off the stack.

### `-push`

```
-push NAME
```

Push the previously saved variable `NAME` back onto the stack.

```sh
# Save a smoothed copy, then write both a smoothed and a decimated version.
cmesh surf.vtp -smooth-mesh 20 -popas smooth \
    -push smooth -omesh smoothed.vtp \
    -push smooth -decimate 0.5 -omesh smoothed_light.vtp
```

---

## Mesh operations

### `-extract-isosurface`

Aliases: `-extract-isosurface`, `-isosurface`.

```
-extract-isosurface T [--method NAME] [--clean] [--smooth-pre SIGMA] [--decimate-post FRAC]
```

Pops an **image**, extracts an iso-surface, and pushes the resulting **mesh**
(in NIFTI RAS space, compatible with cmrep's `vtklevelset`). The continuous
methods extract a single surface at the iso-value `T`; the discrete methods
treat the image as a label map and extract one surface per label ≥ `T`.

| Option | Default | Description |
|--------|---------|-------------|
| `T` *(required)* | — | Iso-value / threshold. For discrete methods, one surface is generated per integer label ≥ `T`. |
| `--method NAME` | `marching-cubes` | Iso-contour algorithm; see table below. |
| `--clean` | off | Triangulate and merge coincident points (drops degenerate cells). |
| `--smooth-pre SIGMA` | `0` (off) | Gaussian pre-smoothing of the image, std-dev in voxels. **Continuous methods only** — combining it with a discrete method is a parse error, because smoothing a label map blends adjacent labels into spurious intermediate values. Smooth the extracted mesh with `-smooth-mesh` instead. |
| `--decimate-post FRAC` | `0` (off) | Reduce polygon count by `FRAC` (0..1) after extraction. |

Unknown `--options` after the threshold are an error attributed to
`-extract-isosurface` (this holds for every command that takes `--options`).

**`--method` values:**

| Name | Algorithm | Notes |
|------|-----------|-------|
| `marching-cubes` | `vtkMarchingCubes` | Single iso-value at `T` (continuous scalar). |
| `flying-edges` | `vtkFlyingEdges3D` | Faster, parallel drop-in for marching cubes; identical output. |
| `discrete-marching-cubes` | `vtkDiscreteMarchingCubes` | One surface per integer label ≥ `T`; adds a `Label` point scalar. |
| `discrete-flying-edges` | `vtkDiscreteFlyingEdges3D` | Faster parallel equivalent of the above. |
| `surface-nets` | `vtkSurfaceNets3D` | Parallel label-boundary nets with built-in constrained smoothing; carries native `BoundaryLabels` cell data. |

```sh
# Single iso-surface from a probability map, cleaned and pre-smoothed.
cmesh prob.nii.gz -extract-isosurface 0.5 --clean --smooth-pre 1.0 -o surf.vtp

# Per-label surfaces from a multi-label segmentation, smoothed afterwards.
cmesh seg.nii.gz \
    -extract-isosurface 1 --method discrete-flying-edges \
    -smooth-mesh 10 0.15 \
    -o labels.vtp

# Smooth surface nets, then reduce triangle count.
cmesh seg.nii.gz \
    -extract-isosurface 1 --method surface-nets --decimate-post 0.5 \
    -o nets.vtp
```

### `-smooth-mesh`

```
-smooth-mesh N [RELAX]
```

Laplacian-smooths the top mesh. Pops a mesh, pushes the smoothed mesh.

| Argument | Default | Description |
|----------|---------|-------------|
| `N` *(required)* | — | Number of smoothing iterations. |
| `RELAX` *(optional)* | `0.1` | Relaxation factor per iteration. The token after `N` is taken as `RELAX` only if it parses as a number, so a following `-command` or filename is never consumed by mistake. |

Internal defaults not exposed on the CLI: feature angle `45°`, boundary
smoothing on, feature-edge smoothing off.

```sh
cmesh surf.vtp -smooth-mesh 30 0.2 -o smoothed.vtp
cmesh surf.vtp -smooth-mesh 10 -o smoothed.vtp        # RELAX defaults to 0.1
```

### `-decimate`

```
-decimate FRAC
```

Reduce polygon count by fraction `FRAC` (0..1; e.g. `0.5` removes ~50% of the
polygons). Pops a mesh, pushes the decimated mesh. Honors the sticky backend:
VTK (`vtkDecimatePro`) by default, or VCG (quadric edge collapse) under
`-use-vcg` when built with VCG support.

Internal VTK defaults not exposed on the CLI: feature angle `15°`,
preserve-topology on, boundary-vertex deletion off.

```sh
cmesh surf.vtp -decimate 0.75 -o light.vtp
cmesh surf.vtp -use-vcg -decimate 0.75 -o light_vcg.vtp
```

### `-compute-normals`

Aliases: `-compute-normals`, `-normals`.

```
-compute-normals [--auto-orient]
```

Compute point and cell normals on the top mesh. Pops a mesh, pushes the same
mesh with normals.

| Option | Default | Description |
|--------|---------|-------------|
| `--auto-orient` | off | Auto-orient normals to point consistently outward. |

Internal defaults not exposed on the CLI: feature angle `30°`, splitting off,
consistency on.

```sh
cmesh surf.vtp -compute-normals --auto-orient -o oriented.vtp
```

### `-flip-normals`

```
-flip-normals
```

Reverse triangle winding (and thus normal direction) on the top mesh, preserving
all data arrays. Takes no arguments. Useful when a surface renders inside-out or
when downstream tools assume the opposite winding convention.

```sh
cmesh surf.vtp -flip-normals -o flipped.vtp
```

### `-meshdiff`

```
-meshdiff [REF]
```

Compute the point-wise distance from a source mesh to a reference mesh. Adds a
`Distance` point-data array to the source and prints summary statistics — N,
mean, RMS, and directed Hausdorff (source→reference) — to stdout.

Two forms:

- **File form** — `-meshdiff REF`: the reference is read from file `REF`; the
  source is popped from the stack.
- **Stack form** — `-meshdiff` with no argument (i.e. followed by another
  `-command` or end of line): the reference is popped from the stack too, with
  layout `[ ..., source, reference (top) ]`. Use this to diff against a mesh
  you just computed without writing it to disk.

Both forms push the annotated source mesh.

```sh
cmesh candidate.vtp -meshdiff ground_truth.vtp -o annotated.vtp
# prints: MeshDiff: ground_truth.vtp
#           N=... mean=... rms=... hausdorff(source->ref)=...

# Stack form: compare a decimated copy against the original, no temp files.
cmesh dense.vtp -dup -decimate 0.9 -swap -meshdiff -o decimated_vs_dense.vtp
```

> A filename immediately after `-meshdiff` is always interpreted as the file
> form; with the stack form, push any further operands after the command
> instead.

---

## Image / mesh interop

### `-rasterize`

```
-rasterize [--ref REF | --spacing SX SY SZ] [--margin M] [--inside V]
```

Pops a **mesh** and pushes a binary **image** covering its interior. The output
grid is defined either by a reference image (`--ref`) or by a voxel spacing plus
an auto-computed bounding box (`--spacing`). The CLI always produces a `float`
image.

| Option | Default | Description |
|--------|---------|-------------|
| `--ref REF` | — | Inherit origin / spacing / direction from reference image `REF`. Mutually exclusive with `--spacing`. |
| `--spacing SX SY SZ` | `1 1 1` | Voxel spacing for an auto bounding-box grid. |
| `--margin M` | `2.0` | Padding (physical units) around the mesh bounding box when no `--ref` is given. |
| `--inside V` | `1` | Foreground (inside) pixel value. Background is `0`. |

```sh
# Rasterize into the same grid as an existing image.
cmesh surf.vtp -rasterize --ref scan.nii.gz -oimage mask.nii.gz

# Rasterize onto a fresh 0.5mm grid with a 5mm margin.
cmesh surf.vtp -rasterize --spacing 0.5 0.5 0.5 --margin 5 -oimage mask.nii.gz
```

### `-warp-mesh`

```
-warp-mesh WARP
```

Displace each vertex of the top mesh by an ITK vector **warp field** `WARP` (a
3D image of 3-component `float` vectors, e.g. `.nii.gz`/`.mha`, such as the
deformation field produced by `greedy`). Pops a mesh, pushes the warped mesh.
Vertices that fall outside the warp field extent are left unchanged and reported
as a warning.

```sh
cmesh surf.vtp -warp-mesh deformation.nii.gz -o warped.vtp
```

### `-sample-image`

```
-sample-image NAME
```

Sample image intensities onto a mesh's vertices, storing them as a point-data
array named `NAME`. Uses the sticky `-int` interpolation mode.

**Stack ordering:** expects the mesh **below** the image — push the mesh first,
then the image, so the layout is `[ ..., mesh, image (top) ]`. Pops both and
pushes the annotated mesh.

```sh
# Push surface, then image, then sample.
cmesh surf.vtp scan.nii.gz -sample-image Intensity -o sampled.vtp

# Nearest-neighbor sampling of a label map.
cmesh surf.vtp labels.nii.gz -int nn -sample-image LabelID -o sampled.vtp
```

### `-merge-array`

Aliases: `-merge-array`, `-merge-arrays`.

```
-merge-array [SRC] NAME [--cell] [--rename NEW]
```

Copy a named data array from a source mesh onto a destination mesh. By
default the array is treated as point data; the source and destination are
expected to be in correspondence (same point/cell ordering).

Two forms, disambiguated by whether the first argument has a recognized mesh
extension:

- **File form** — `-merge-array SRC NAME`: the source is read from file
  `SRC`; the destination is popped from the stack.
- **Stack form** — `-merge-array NAME`: the source is popped from the stack
  too, with layout `[ ..., destination, source (top) ]`.

Both forms push the merged destination mesh. (An array name that itself ends
in a mesh extension needs the file form.)

| Argument / Option | Default | Description |
|-------------------|---------|-------------|
| `SRC` *(optional)* | stack | Source mesh filename to read the array from. |
| `NAME` *(required)* | — | Name of the array to copy. |
| `--cell` | off | Treat `NAME` as a **cell**-data array (default is point data). |
| `--rename NEW` | keep `NAME` | Rename the copied array to `NEW` on the destination. |

```sh
# Copy point array "Thickness" from atlas.vtp onto the current mesh.
cmesh subject.vtp -merge-array atlas.vtp Thickness -o merged.vtp

# Copy a cell array and rename it.
cmesh subject.vtp -merge-array atlas.vtp RegionID --cell --rename Parcellation -o merged.vtp

# Stack form: source pushed on top of the destination.
cmesh subject.vtp atlas.vtp -merge-array Thickness -o merged.vtp
```

---

## Worked pipelines

**Segmentation → smoothed, decimated surface with sampled intensities:**

```sh
cmesh seg.nii.gz \
    -extract-isosurface 1 --method discrete-flying-edges --clean \
    -smooth-mesh 10 0.15 \
    -decimate 0.5 \
    -compute-normals \
    image.nii.gz \
    -sample-image Intensity \
    -o surface.vtp
```

After `-compute-normals` the mesh is on the stack; pushing `image.nii.gz` puts
the image on top, giving `-sample-image` the `[ mesh, image (top) ]` layout it
requires.

**Surface → binary mask aligned to a reference image:**

```sh
cmesh surf.vtp -rasterize --ref reference.nii.gz -oimage mask.nii.gz
```

**Compare a decimated mesh against the original (reference read from file):**

```sh
cmesh dense.vtp -decimate 0.9 -meshdiff dense.vtp -o decimated_with_distance.vtp
```

**Format conversion:**

```sh
cmesh surface.vtp -o surface.stl
cmesh mesh.byu -o mesh.ply
```
