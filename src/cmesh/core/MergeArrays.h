#ifndef CONVERTMESH_CORE_MERGE_ARRAYS_H
#define CONVERTMESH_CORE_MERGE_ARRAYS_H

#include "cmesh/core/Progress.h"

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <string>

namespace cmesh
{

struct MergeArraysParams
{
  std::string array_name;
  bool        cell_data = false;
  std::string rename_to = "";  // empty = keep source name
};

/**
 * Copy a named point-data (or cell-data) array from `source` onto `dest`.
 * Returns a shallow copy of `dest` with the new array attached. Throws
 * cmesh::AlgorithmError if the source array is missing or its length does
 * not match `dest`'s vertex/cell count.
 */
vtkSmartPointer<vtkPolyData>
MergeArrays(vtkPolyData *dest,
            vtkPolyData *source,
            const MergeArraysParams &p,
            ProgressFn progress = {},
            AbortToken abort = {});

} // namespace cmesh

#endif
