#ifndef CONVERTMESH_CORE_SMOOTH_MESH_H
#define CONVERTMESH_CORE_SMOOTH_MESH_H

#include "core/Backend.h"
#include "core/Progress.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

namespace cmesh
{

struct SmoothParams
{
  int     iterations           = 20;
  double  relaxation_factor    = 0.1;
  double  feature_angle        = 45.0;
  bool    boundary_smoothing   = true;
  bool    feature_edge_smoothing = false;
  Backend backend              = Backend::VTK;
};

/**
 * Laplacian smoothing. Returns a new polydata; the input is not modified.
 * VCG backend is not yet wired up — request Backend::VCG to get a fallback
 * to VTK with no warning.
 */
vtkSmartPointer<vtkPolyData>
SmoothMesh(vtkPolyData *in,
           const SmoothParams &p,
           ProgressFn progress = {},
           AbortToken abort = {});

} // namespace cmesh

#endif
