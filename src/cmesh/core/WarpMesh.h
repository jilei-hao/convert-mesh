#ifndef CONVERTMESH_CORE_WARP_MESH_H
#define CONVERTMESH_CORE_WARP_MESH_H

#include "cmesh/core/Progress.h"

#include <itkImage.h>
#include <itkSmartPointer.h>
#include <itkVector.h>

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

namespace cmesh
{

using WarpField = itk::Image<itk::Vector<float, 3>, 3>;

struct WarpMeshParams
{
  // How to handle mesh vertices that fall outside the warp field's bounds:
  //   true  → leave them at their original position (warning if any)
  //   false → throw cmesh::AlgorithmError
  bool ignore_outside = true;
};

struct WarpMeshStats
{
  vtkIdType n_outside = 0;
};

/**
 * Displace each mesh vertex X by the warp field D evaluated at X. The mesh
 * is assumed to live in NIFTI RAS space; the warp field is an ITK image in
 * LPS — boundary conversion happens internally.
 */
vtkSmartPointer<vtkPolyData>
WarpMesh(vtkPolyData *mesh,
         WarpField *warp,
         const WarpMeshParams &p,
         WarpMeshStats *out_stats = nullptr,
         ProgressFn progress = {},
         AbortToken abort = {});

} // namespace cmesh

#endif
