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

struct IsoSurfaceParams
{
  double  threshold       = 0.5;
  bool    multi_label     = false;
  double  smooth_pre      = 0.0;   // Gaussian std-dev in voxels
  double  decimate        = 0.0;   // 0 disables
  bool    clean           = false;
  bool    compute_normals = true;
  Backend backend         = Backend::VTK;
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
