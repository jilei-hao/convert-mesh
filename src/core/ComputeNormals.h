#ifndef CONVERTMESH_CORE_COMPUTE_NORMALS_H
#define CONVERTMESH_CORE_COMPUTE_NORMALS_H

#include "core/Progress.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

namespace cmesh
{

struct NormalsParams
{
  double feature_angle = 30.0;
  bool   splitting     = false;
  bool   consistency   = true;
  bool   auto_orient   = false;
};

vtkSmartPointer<vtkPolyData>
ComputeNormals(vtkPolyData *in,
               const NormalsParams &p,
               ProgressFn progress = {},
               AbortToken abort = {});

} // namespace cmesh

#endif
