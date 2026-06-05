# `cmesh` command-line reference

`cmesh` is a stack-based pipeline for medical-imaging mesh and image
processing, modeled on [Convert3D (`c3d`)](https://sourceforge.net/projects/c3d/).
A single invocation is a sequence of tokens evaluated left to right. Each
token is either a **filename** (loaded onto the stack) or a **`-command`**
(consumes zero or more following arguments and usually transforms the stack).

```
cmesh [ARG | -COMMAND [ARGS...]] ...
```

This document describes every command, its options, defaults, and an example
for each. For a terse summary at the terminal, run `cmesh --help`.

---

## Table of contents

- [The stack model](#the-stack-model)
- [Filename auto-detection](#filename-auto-detection)
- [Global & sticky flags](#global--sticky-flags)
- [I/O commands](#io-commands)
- [Stack commands](#stack-commands)
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

## The stack model

`cmesh` holds a LIFO **stack** of data items. Each item is one of three
*kinds*:

| Kind    | Produced by                                  |
|---------|----------------------------------------------|
| mesh    | reading a mesh file, or any mesh operation   |
| image   | reading an image file, or `-rasterize`       |
| ugrid   | reading an unstructured grid (output only)   |

Most commands operate on the **top** of the stack: they pop their input(s),
compute, and push the result. Commands that need two inputs (e.g.
`-sample-image`) expect a specific ordering described in their section.

Variables let you set an item aside and reuse it later:

```sh
cmesh surf.vtp -as S        # keep S on the stack and save it as "S"
cmesh surf.vtp -popas S ...  # save as "S" and remove from the stack
... -push S                  # push the saved item back onto the stack
```

Order of evaluation is strictly left to right, so a pipeline reads like a
recipe.

---

## Filename auto-detection

A bare token that is **not** prefixed with `-` is treated as a filename and
pushed onto the stack. The kind is inferred from the extension:

- **Mesh:** `.vtk`, `.vtp`, `.stl`, `.obj`, `.ply`, `.byu`, `.y`
- **Image:** `.nii`, `.nii.gz`, `.mha`, `.mhd`, `.nrrd`, `.nhdr`, `.gipl`,
  `.img`, `.hdr`, `.png`, `.jpg`/`.jpeg`, `.tif`/`.tiff`

If the extension is not recognized, `cmesh` errors out. Use the explicit
`-push-mesh` / `-push-image` forms to override detection.

---

## Global & sticky flags

These do not touch the stack. **Sticky** flags set a mode that applies to all
*subsequent* operations until changed.

| Flag | Effect |
|------|--------|
| `-verbose` | Print a progress line for each operation. |
| `-no-warn` | Silence data-loss warnings. |
| `-discard-data` | Allow operations that may drop `vtkPolyData` arrays. |
| `-use-vtk` | *(sticky, default)* Prefer VTK-backed implementations. |
| `-use-vcg` | *(sticky)* Prefer VCG-backed implementations where available. |
| `-use-gpu` | *(sticky, reserved)* Prefer GPU-backed implementations. |
| `-int MODE`, `-interpolation MODE` | *(sticky)* Set interpolation mode used by `-sample-image`. `MODE` is `linear` (default), `nn`/`nearest`/`nearestneighbor`, or `bspline`. |
| `-h`, `--help`, `-help` | Print the built-in usage summary. |
| `--version`, `-version` | Print the version string. |

The backend flag is read by each operation when it runs, so you can switch
backends mid-pipeline:

```sh
cmesh a.vtp -decimate 0.5 -use-vcg -decimate 0.5   # first VTK, then VCG
```

> Currently `-use-vcg` is honored by `-decimate` (when built with VCG
> support); other operations fall back to VTK. `-use-gpu` is reserved.

---

## I/O commands

### `-o FILE`
Write the **top** of the stack to `FILE`, choosing the writer from the
top item's kind (mesh vs. image) and the file extension.

```sh
cmesh seg.nii.gz -extract-isosurface 0.5 -o surface.vtp
```

### `-omesh FILE`
Write the top item to `FILE`, requiring it to be a mesh or ugrid.

```sh
cmesh in.vtk -smooth-mesh 20 -omesh out.stl
```

### `-oimage FILE`
Write the top item to `FILE`, requiring it to be an image.

```sh
cmesh surf.vtp -rasterize --spacing 0.5 0.5 0.5 -oimage mask.nii.gz
```

### `-push-mesh FILE`
Read a mesh file and push it (bypasses extension auto-detection).

### `-push-image FILE`
Read an image file and push it (bypasses extension auto-detection).

```sh
cmesh -push-image scan.dat -oimage scan.nii.gz   # force image interpretation
```

---

## Stack commands

| Command | Effect |
|---------|--------|
| `-pop` | Discard the top item. |
| `-dup` | Duplicate the top item. |
| `-swap` | Swap the top two items (errors if fewer than two). |
| `-clear` | Empty the stack. |
| `-as NAME` | Save the top item to variable `NAME`; **keep** it on the stack. |
| `-popas NAME` | Save the top item to variable `NAME` and **pop** it. |
| `-push NAME` | Push the saved variable `NAME` onto the stack. |

```sh
# Save a smoothed copy, then write both the smoothed and a decimated version.
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
(in NIFTI RAS space, compatible with cmrep's `vtklevelset`).

| Option | Default | Description |
|--------|---------|-------------|
| `T` *(required)* | — | Iso-value / threshold. For discrete methods, surfaces are generated for each integer label ≥ `T`. |
| `--method NAME` | `marching-cubes` | Iso-contour algorithm; see table below. |
| `--clean` | off | Triangulate and merge coincident points (drops degenerate cells). |
| `--smooth-pre SIGMA` | `0` (off) | Gaussian pre-smoothing of the image, std-dev in voxels. |
| `--decimate-post FRAC` | `0` (off) | Reduce polygon count by `FRAC` (0..1) after extraction. |

**`--method` values:**

| Name | Algorithm | Notes |
|------|-----------|-------|
| `marching-cubes` | `vtkMarchingCubes` | Single iso-value at `T` (continuous scalar). |
| `flying-edges` | `vtkFlyingEdges3D` | Faster, parallel drop-in for marching cubes; identical output. |
| `discrete-marching-cubes` | `vtkDiscreteMarchingCubes` | One surface per integer label ≥ `T`; adds a `Label` point scalar. |
| `discrete-flying-edges` | `vtkDiscreteFlyingEdges3D` | Faster parallel equivalent of the above. |
| `surface-nets` | `vtkSurfaceNets3D` | Parallel label-boundary nets with built-in constrained smoothing; carries native `BoundaryLabels` cell data. |

The continuous methods (`marching-cubes`, `flying-edges`) extract a single
surface at `T`. The discrete methods (`discrete-*`, `surface-nets`) treat the
image as a label map and extract one surface per label.

```sh
# Single iso-surface from a probability map, cleaned.
cmesh prob.nii.gz -extract-isosurface 0.5 --clean -o surf.vtp

# Per-label surfaces from a multi-label segmentation, with pre-smoothing.
cmesh seg.nii.gz \
    -extract-isosurface 1 --method discrete-flying-edges --smooth-pre 1.0 \
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
| `RELAX` *(optional)* | `0.1` | Relaxation factor per iteration. Omit it (or follow `N` with another `-command`) to use the default. |

> Internal defaults not exposed on the CLI: feature angle `45°`, boundary
> smoothing on, feature-edge smoothing off.

```sh
cmesh surf.vtp -smooth-mesh 30 0.2 -o smoothed.vtp
cmesh surf.vtp -smooth-mesh 10 -o smoothed.vtp        # RELAX defaults to 0.1
```

### `-decimate`

```
-decimate FRAC
```

Reduce polygon count by fraction `FRAC` (0..1; e.g. `0.5` removes ~50%). Pops
a mesh, pushes the decimated mesh. Honors the sticky backend: VTK
(`vtkDecimatePro`) by default, or VCG (quadric edge collapse) under `-use-vcg`
when built with VCG support.

> Internal VTK defaults not exposed on the CLI: feature angle `15°`,
> preserve-topology on, boundary-vertex deletion off.

```sh
cmesh surf.vtp -decimate 0.75 -o light.vtp
cmesh surf.vtp -use-vcg -decimate 0.75 -o light_vcg.vtp
```

### `-compute-normals`

Aliases: `-compute-normals`, `-normals`.

```
-compute-normals [--auto-orient]
```

Compute point/cell normals on the top mesh.

| Option | Default | Description |
|--------|---------|-------------|
| `--auto-orient` | off | Auto-orient normals to point consistently outward. |

> Internal defaults not exposed on the CLI: feature angle `30°`, splitting
> off, consistency on.

```sh
cmesh surf.vtp -compute-normals --auto-orient -o oriented.vtp
```

### `-flip-normals`

```
-flip-normals
```

Reverse triangle winding (and thus normal direction) on the top mesh,
preserving all data arrays. Takes no arguments.

```sh
cmesh surf.vtp -flip-normals -o flipped.vtp
```

### `-meshdiff`

```
-meshdiff REF
```

Compute the point-wise distance from the top mesh to a reference mesh `REF`
(read from file). Adds a `Distance` point-data array to the top mesh and
prints summary statistics (N, mean, RMS, directed Hausdorff source→reference)
to stdout. Pops the source mesh, pushes the annotated mesh.

```sh
cmesh candidate.vtp -meshdiff ground_truth.vtp -o annotated.vtp
# prints: MeshDiff: ground_truth.vtp
#           N=... mean=... rms=... hausdorff(source->ref)=...
```

---

## Image / mesh interop

### `-rasterize`

```
-rasterize [--ref REF | --spacing SX SY SZ] [--margin M] [--inside V]
```

Pops a **mesh** and pushes a binary **image** covering its interior. The
output grid is defined either by a reference image or by a voxel spacing plus
an auto-computed bounding box. (The CLI always produces a `float` image.)

| Option | Default | Description |
|--------|---------|-------------|
| `--ref REF` | — | Inherit origin / spacing / direction from reference image `REF`. Mutually exclusive with `--spacing`. |
| `--spacing SX SY SZ` | `1 1 1` | Voxel spacing for an auto bounding-box grid. |
| `--margin M` | `2.0` | Padding (in physical units) around the mesh bounding box when no `--ref` is given. |
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

Displace each vertex of the top mesh by an ITK vector **warp field** `WARP`
(a 3D image of 3-component `float` vectors, e.g. `.nii.gz`/`.mha`). Pops a
mesh, pushes the warped mesh. Vertices that fall outside the warp field extent
are left unchanged and reported as a warning.

```sh
cmesh surf.vtp -warp-mesh deformation.nii.gz -o warped.vtp
```

### `-sample-image`

```
-sample-image NAME
```

Sample image intensities onto a mesh's vertices, storing them as a point-data
array named `NAME`. Uses the sticky `-int` interpolation mode.

**Stack ordering:** expects the mesh **below** the image — i.e. push the mesh
first, then the image, so the layout is `[ ..., mesh, image (top) ]`. Pops
both and pushes the annotated mesh.

```sh
# Push surface, then image, then sample.
cmesh surf.vtp scan.nii.gz -sample-image Intensity -o sampled.vtp

# Nearest-neighbor sampling of a label map.
cmesh surf.vtp labels.nii.gz -int nn -sample-image LabelID -o sampled.vtp
```

### `-merge-array`

Aliases: `-merge-array`, `-merge-arrays`.

```
-merge-array SRC NAME [--cell] [--rename NEW]
```

Copy a named data array from a source mesh `SRC` (read from file) onto the
**top** mesh of the stack. Pops the destination mesh, pushes the result.

| Argument / Option | Default | Description |
|-------------------|---------|-------------|
| `SRC` *(required)* | — | Source mesh filename to read the array from. |
| `NAME` *(required)* | — | Name of the array to copy. |
| `--cell` | off | Treat `NAME` as a **cell**-data array (default is point data). |
| `--rename NEW` | keep `NAME` | Rename the copied array to `NEW` on the destination. |

```sh
# Copy point array "Thickness" from atlas.vtp onto the current mesh.
cmesh subject.vtp -merge-array atlas.vtp Thickness -o merged.vtp

# Copy a cell array and rename it.
cmesh subject.vtp -merge-array atlas.vtp RegionID --cell --rename Parcellation -o merged.vtp
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

After `-compute-normals` the mesh is on the stack; pushing `image.nii.gz`
puts the image on top, giving `-sample-image` the `[ mesh, image (top) ]`
layout it requires.

**Surface → binary mask aligned to a reference image:**

```sh
cmesh surf.vtp -rasterize --ref reference.nii.gz -oimage mask.nii.gz
```

**Compare a decimated mesh against the original (reference read from file):**

```sh
cmesh dense.vtp -decimate 0.9 -meshdiff dense.vtp -o decimated_with_distance.vtp
```
