#ifndef CONVERTMESH_CORE_DECIMATE_MESH_H
#define CONVERTMESH_CORE_DECIMATE_MESH_H

#include "cmesh/core/Backend.h"
#include "cmesh/core/Progress.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

namespace cmesh
{

struct DecimateParams
{
  double  reduction         = 0.5;
  double  feature_angle     = 15.0;
  bool    preserve_topology = true;
  bool    boundary_deletion = false;
  Backend backend           = Backend::VTK;
};

/**
 * Reduce polygon count. VTK backend uses vtkDecimatePro (topology-preserving
 * by default). VCG backend uses quadric-edge-collapse remeshing (matches
 * cmrep's mesh_decimate_vcg); only available when built with
 * CONVERTMESH_HAVE_VCG, otherwise falls back to VTK.
 */
vtkSmartPointer<vtkPolyData>
DecimateMesh(vtkPolyData *in,
             const DecimateParams &p,
             ProgressFn progress = {},
             AbortToken abort = {});

} // namespace cmesh

#endif
