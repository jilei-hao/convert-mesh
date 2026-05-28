# ConvertMesh

A command-line utility and C++ library for medical-imaging mesh processing
and conversion. Inspired by [Convert3D (c3d)](https://sourceforge.net/projects/c3d/),
with the same stack-based pipeline philosophy but operating on meshes
instead of images. Consolidates mesh operations that are currently
duplicated across [cmrep](https://github.com/pyushkevich/cmrep) and
[ITK-SNAP](https://www.itksnap.org/).

- Stack-based CLI: push, pop, chain operations in a single command
- Supports **VTK, VTP, STL, OBJ, PLY, BYU** meshes and anything ITK reads
  for images (NIFTI, NRRD, MetaImage, GIPL, PNG/JPEG/TIFF, …)
- Mesh generation from images (iso-surface), rasterisation, warp-field
  deformation, per-vertex image sampling, and standard VTK polydata ops
- Swappable backends per operation (`-use-vtk` / `-use-vcg` / `-use-gpu`)
- Linkable C++ library (`cmesh_api`) usable from cmrep, ITK-SNAP, or any
  downstream project; Python bindings are on the roadmap
- Meshes are stored in **NIFTI RAS** space — compatible with 3D Slicer,
  ITK-SNAP, and cmrep's `vtklevelset`


## Quick example

Extract an iso-surface from a segmentation, smooth and decimate it, then
sample the original image intensities onto the surface:

```sh
cmesh seg.nii.gz \
    -extract-isosurface 0.5 -multi-label -clean \
    -smooth-mesh 10 0.15 \
    -decimate 0.5 \
    -compute-normals \
    -popas surf \
    image.nii.gz -push surf \
    -sample-image Intensity \
    -o surface.vtp
```


## Installation

### Prerequisites

- C++17 compiler (Clang, GCC, MSVC)
- CMake 3.15+
- ITK ≥ 5.4 (built with `ITKVTK` module)
- VTK ≥ 9.3

### Build from source

```sh
git clone <repo-url> ConvertMesh
mkdir build && cd build
cmake -G Ninja \
    -DITK_DIR=/path/to/itk/build \
    -DVTK_DIR=/path/to/vtk/install/lib/cmake/vtk-9.3 \
    -DCMAKE_BUILD_TYPE=Release \
    ../ConvertMesh
ninja
ctest --output-on-failure     # optional
```

The `cmesh` binary is produced in the build directory. `ninja install`
puts it in `$CMAKE_INSTALL_PREFIX/bin` along with the library headers
and CMake package files.


## Using the CLI

### The stack model

`cmesh` maintains a stack of data items — each item is a **mesh**, an
**image**, or an **unstructured grid**. Every command either reads from
or writes to this stack:

- A bare filename on the command line is pushed onto the stack (kind
  inferred from the extension: `.nii.gz` → image, `.vtp` → mesh, etc.).
- `-o FILE` writes the top of the stack to `FILE`.
- Most operations pop their inputs and push their outputs, leaving the
  stack ready for the next command.

Commands are processed **left to right**. For example:

```sh
cmesh a.vtp b.vtp -meshdiff b.vtp -o a-vs-b.vtp
```
reads `a.vtp` onto the stack, pushes `b.vtp`, then `-meshdiff` pops the
top two, computes distances, and pushes `a.vtp` back (now annotated with
a `Distance` point-data array), which `-o` writes to disk.

### Sticky flags

Some flags set modes that persist across subsequent commands:

| Flag | Effect |
|---|---|
| `-use-vtk` (default) | Prefer VTK-backed implementations |
| `-use-vcg` | Prefer VCG-backed implementations (falls back to VTK if unavailable) |
| `-use-gpu` | Reserved for future GPU backends |
| `-int linear\|nn\|bspline` | Interpolation mode for image sampling |
| `-discard-data` | Allow topology-changing ops that may drop polydata arrays |
| `-verbose` | Print per-operation progress |
| `-no-warn` | Silence data-loss warnings |

### Command reference

**I/O**

| Command | Description |
|---|---|
| `FILE` | Auto-detect by extension and push |
| `-push-mesh FILE` | Read a mesh and push |
| `-push-image FILE` | Read an image and push |
| `-o FILE` | Write top of stack to `FILE` |
| `-omesh FILE` / `-oimage FILE` | Explicit-kind write |

**Stack ops**

| Command | Description |
|---|---|
| `-pop` | Discard top |
| `-dup` | Duplicate top |
| `-swap` | Swap top two |
| `-clear` | Empty the stack |
| `-as NAME` | Assign top to variable `NAME` (keeps on stack) |
| `-popas NAME` | Assign top to variable and pop |
| `-push NAME` | Push variable `NAME` |

**Mesh operations**

| Command | Description |
|---|---|
| `-extract-isosurface T [modifiers]` | Pop image, push iso-surface at threshold `T`. Modifiers: `-multi-label`, `-clean`, `-smooth-pre SIGMA`, `-decimate-post FRAC` |
| `-smooth-mesh N [RELAX]` | Laplacian smooth (`N` iterations, default relaxation 0.1) |
| `-decimate FRAC` | Reduce polygon count by `FRAC` ∈ (0, 1) |
| `-compute-normals [-auto-orient]` | Recompute polydata normals |
| `-flip-normals` | Reverse triangle winding (array-preserving) |
| `-meshdiff REF` | Add `Distance` array of top mesh vs. `REF`; prints mean/RMS/Hausdorff |

**Image / mesh interop**

| Command | Description |
|---|---|
| `-rasterize [-ref REF\|-spacing SX SY SZ] [-margin M] [-inside V]` | Pop mesh, push a binary image covering its interior |
| `-warp-mesh WARP` | Displace top mesh by an ITK vector warp field |
| `-sample-image NAME` | Pop image, annotate the mesh below with a named scalar array sampled via the sticky `-int` mode |
| `-merge-array SRC NAME [-cell] [-rename NEW]` | Copy named array from source mesh onto top mesh |

**Meta**

| Command | Description |
|---|---|
| `-h`, `--help` | Print the complete list of commands |
| `--version` | Print version |

### More examples

Convert between mesh formats:

```sh
cmesh surface.vtp -o surface.stl
cmesh mesh.byu -o mesh.ply
```

Extract a single iso-surface and write it out:

```sh
cmesh label.nii.gz -extract-isosurface 1.5 -clean -o label.vtp
```

Compare two meshes and save a distance-annotated version of the first:

```sh
cmesh prediction.vtp -meshdiff truth.vtp -o prediction-with-distance.vtp
```

Rasterize a surface into the same voxel grid as a reference image:

```sh
cmesh surface.vtp -rasterize -ref reference.nii.gz -o mask.nii.gz
```

Warp a mesh with a displacement field produced by `greedy`:

```sh
cmesh mesh.vtp -warp-mesh phi_warp.nii.gz -o mesh-warped.vtp
```

Sample image intensities onto a mesh (nearest-neighbour for labels):

```sh
cmesh mesh.vtp -push-image labels.nii.gz -int nn -sample-image Label -o mesh-with-labels.vtp
```


## Using the C++ library

ConvertMesh exposes two libraries:

- **`cmesh_core`** — the public library: pure mesh/image functions (e.g.
  `cmesh::ExtractIsoSurface`, `cmesh::ReadImage`, `cmesh::WritePolyData`)
  that throw `cmesh::Error` on failure. All `core/*.h` headers are public.
- **`cmesh_cli`** — the CLI parser, stack, and glue. Its only public
  header is `cli/Run.h`, which runs the same parser the `cmesh` binary
  uses. Link this if you want bit-identical CLI behavior from your own
  host (e.g. a Python binding).

### Driving the CLI parser

`cmesh::cli::Run` takes an argv-style token list (without the program
name) and returns a shell-style exit code:

```cpp
#include "cli/Run.h"
#include <sstream>

std::ostringstream out, err;
int rc = cmesh::cli::Run(
    {"seg.nii.gz", "-extract-isosurface", "0.5", "-clean",
     "-decimate", "0.5", "-smooth-mesh", "10",
     "-o", "surface.vtp"},
    out, err);

if(rc != 0)
    std::cerr << "pipeline failed: " << err.str() << std::endl;
```

### Calling core functions directly

For in-memory work, skip the parser and call the core functions:

```cpp
#include "core/ImageIO.h"
#include "core/ExtractIsoSurface.h"
#include "core/MeshIO.h"

try
{
    // Read (or hand in an existing itk::Image)
    auto image = cmesh::ReadImage("seg.nii.gz");

    cmesh::IsoSurfaceParams p;
    p.threshold = 0.5;
    p.clean     = true;
    p.decimate  = 0.5;
    vtkSmartPointer<vtkPolyData> mesh =
        cmesh::ExtractIsoSurface(image.GetPointer(), p);

    cmesh::WritePolyData(mesh, "surface.vtp");
}
catch(const cmesh::Error &e)
{
    std::cerr << "failed: " << e.what() << std::endl;
}
```

### As a CMake subproject

```cmake
add_subdirectory(external/ConvertMesh)
target_link_libraries(my_app PRIVATE cmesh_core)   # add cmesh_cli for Run()
```

Set `CONVERTMESH_BUILD_AS_SUBPROJECT=ON` in the parent project if ITK
and VTK are already found there; the subproject will skip its own
`find_package` calls. Set `CONVERTMESH_SUBPROJECT_BUILD_CLI=ON` to also
build the `cmesh` executable inside your build.

### From an installed package

```cmake
find_package(ConvertMesh REQUIRED)
target_link_libraries(my_app PRIVATE ConvertMesh::cmesh_core)
# Add ConvertMesh::cmesh_cli if you call cmesh::cli::Run.
```


## Coordinate conventions

ConvertMesh follows the `cmrep` / 3D-Slicer convention: **meshes on the
stack are in NIFTI RAS space**. ITK images are read and stored in their
native LPS convention; the conversion happens at every mesh↔image
boundary inside ConvertMesh, so you do not need to flip signs yourself.

If you feed a mesh produced by a different tool into ConvertMesh and see
X/Y-mirrored output, check whether that tool was writing LPS; if so,
apply a `diag(-1, -1, 1)` transform before pushing to the stack.


## Limitations and roadmap

Current scope (Phase 0–2):

- Stack, I/O, format conversion
- Core mesh ops: iso-surface extraction, smoothing, decimation, normals,
  mesh distance
- Image/mesh interop: rasterise, warp, sample, merge-arrays

Partially implemented:

- **VCG backend** (`-use-vcg`) — quadric-edge-collapse decimation is
  wired up (parity with cmrep's `mesh_decimate_vcg`), gated behind the
  `CONVERTMESH_BUILD_VCG` CMake option. Poisson-disk sampling and other
  VCG ops are still pending.

On the roadmap (not yet implemented):

- **TetGen backend** — surface → tetrahedral mesh
- Full coordinate-system-mode support for `-warp-mesh` (ijk / ijkos /
  lps / ras / ants, matching cmrep's `warpmesh`)
- Non-identity direction-cosine reference images in `-rasterize`
- Python bindings via pybind11


## License

Distributed under the same license as its parent projects (GPL v3+).


## Acknowledgements

Design patterns and algorithms lifted from:

- **c3d / Convert3D** (Paul Yushkevich) — stack-based CLI, adapter
  pattern, CMake organisation
- **cmrep** (Paul Yushkevich and contributors) — `vtklevelset`,
  `meshdiff`, `mesh_image_sample`, VTK ↔ ITK bridge
- **ITK-SNAP** (Paul Yushkevich and contributors) — `VTKMeshPipeline`,
  array-preserving face-flip filter
