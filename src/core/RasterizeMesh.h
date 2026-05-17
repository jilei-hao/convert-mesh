#ifndef CONVERTMESH_CORE_RASTERIZE_MESH_H
#define CONVERTMESH_CORE_RASTERIZE_MESH_H

#include "core/Progress.h"

#include <itkImage.h>
#include <itkImageBase.h>
#include <itkSmartPointer.h>

#include <vtkPolyData.h>

namespace cmesh
{

/**
 * Geometry source for the output image. Mutually exclusive:
 *   - Reference != nullptr: inherit origin / spacing / direction from it.
 *   - Reference == nullptr: auto-bounding-box of the mesh with `margin`,
 *     spacing taken from `spacing[]`, identity direction.
 *
 * If you have a reference image on disk, call cmesh::ReadImage and pass it
 * here; that keeps RasterizeMesh decoupled from I/O.
 */
template <class TOut>
struct RasterizeParams
{
  itk::ImageBase<3> *reference = nullptr;
  double             spacing[3]    = { 1.0, 1.0, 1.0 };
  double             margin        = 2.0;
  TOut               inside_value  = static_cast<TOut>(1);
  TOut               outside_value = static_cast<TOut>(0);
};

/**
 * Rasterize a polydata (in NIFTI RAS space) into an itk::Image<TOut,3>.
 * Reference images with non-identity direction cosines are not yet
 * supported and will throw cmesh::AlgorithmError.
 */
template <class TOut>
itk::SmartPointer<itk::Image<TOut, 3>>
RasterizeMesh(vtkPolyData *mesh,
              const RasterizeParams<TOut> &p,
              ProgressFn progress = {},
              AbortToken abort = {});

} // namespace cmesh

#endif
