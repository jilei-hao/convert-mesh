#ifndef CONVERTMESH_CORE_EXTRACT_ISOSURFACE_H
#define CONVERTMESH_CORE_EXTRACT_ISOSURFACE_H

#include "core/Backend.h"
#include "core/Progress.h"

#include <itkImage.h>
#include <itkImageBase.h>
#include <itkSmartPointer.h>

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

namespace cmesh
{

/**
 * Iso-contour algorithm flavor. This is the algorithm axis, orthogonal to
 * Backend (which selects the implementation library). All flavors are VTK.
 *
 *   MarchingCubes / FlyingEdges   single iso-value at `threshold` (continuous).
 *   Discrete* / SurfaceNets       one surface per integer label >= `threshold`
 *                                 (label images); each output point carries a
 *                                 "Label" scalar.
 *
 * FlyingEdges is a faster, parallel drop-in for MarchingCubes; likewise
 * DiscreteFlyingEdges for DiscreteMarchingCubes. SurfaceNets is parallel and
 * applies its own constrained smoothing.
 */
enum class IsoMethod
{
  MarchingCubes,
  FlyingEdges,
  DiscreteMarchingCubes,
  DiscreteFlyingEdges,
  SurfaceNets
};

inline bool IsDiscrete(IsoMethod m)
{
  return m == IsoMethod::DiscreteMarchingCubes
      || m == IsoMethod::DiscreteFlyingEdges
      || m == IsoMethod::SurfaceNets;
}

struct IsoSurfaceParams
{
  double    threshold       = 0.5;
  IsoMethod method          = IsoMethod::MarchingCubes;
  double    smooth_pre      = 0.0;   // Gaussian std-dev in voxels
  double    decimate        = 0.0;   // 0 disables
  bool      clean           = false;
  bool      compute_normals = true;
  Backend   backend         = Backend::VTK;
};

/**
 * Extract an iso-surface (single contour or per-label) from an image.
 * Output mesh is in NIFTI RAS space (matches cmrep's vtklevelset).
 */
template <class T>
vtkSmartPointer<vtkPolyData>
ExtractIsoSurface(itk::Image<T, 3> *image,
                  const IsoSurfaceParams &p,
                  ProgressFn progress = {},
                  AbortToken abort = {});

/**
 * Polymorphic overload: dispatches on the runtime pixel type of `image`.
 */
vtkSmartPointer<vtkPolyData>
ExtractIsoSurface(itk::ImageBase<3> *image,
                  const IsoSurfaceParams &p,
                  ProgressFn progress = {},
                  AbortToken abort = {});

} // namespace cmesh

#endif
