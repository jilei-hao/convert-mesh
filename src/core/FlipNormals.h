#ifndef CONVERTMESH_CORE_FLIP_NORMALS_H
#define CONVERTMESH_CORE_FLIP_NORMALS_H

#include "core/Progress.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

namespace cmesh
{

/**
 * Reverse triangle winding while preserving point and cell data arrays.
 * Existing "Normals" arrays are passed through unchanged — call
 * ComputeNormals afterward if you need recomputed vectors.
 */
vtkSmartPointer<vtkPolyData>
FlipNormals(vtkPolyData *in,
            ProgressFn progress = {},
            AbortToken abort = {});

} // namespace cmesh

#endif
