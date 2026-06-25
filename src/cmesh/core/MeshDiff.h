#ifndef CONVERTMESH_CORE_MESH_DIFF_H
#define CONVERTMESH_CORE_MESH_DIFF_H

#include "cmesh/core/Progress.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <cstddef>
#include <string>

namespace cmesh
{

struct MeshDiffParams
{
  std::string array_name = "Distance";
};

struct MeshDiffStats
{
  std::size_t n_points  = 0;
  double      mean      = 0.0;
  double      rms       = 0.0;
  double      hausdorff = 0.0;  // directed: source -> reference
};

/**
 * For each vertex of `source`, compute the closest-point distance to a
 * triangle of `reference`. Returns a new polydata that is a shallow copy
 * of `source` with an extra point-data array (named via params.array_name)
 * containing per-vertex distances; the stats are written into `out_stats`
 * if non-null.
 *
 * The reference mesh is internally re-triangulated for reliable locator
 * results; the input is not modified.
 */
vtkSmartPointer<vtkPolyData>
MeshDiff(vtkPolyData *source,
         vtkPolyData *reference,
         const MeshDiffParams &p,
         MeshDiffStats *out_stats = nullptr,
         ProgressFn progress = {},
         AbortToken abort = {});

} // namespace cmesh

#endif
