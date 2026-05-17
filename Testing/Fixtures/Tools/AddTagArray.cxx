// AddTagArray — load a VTK polydata, append a named float array whose
// values are the point indices, write it back. Used by
// scripts/generate-cmrep-ground-truth.sh to build a "tagged" mesh that feeds
// `mesh_merge_arrays` (which expects the array already on its input mesh).
//
// Usage: AddTagArray <in.vtk> <out.vtk> <array_name>

#include "core/MeshIO.h"

#include <vtkFloatArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>

#include <cstdio>

int main(int argc, char *argv[])
{
  if(argc < 4)
  {
    std::fprintf(stderr, "Usage: %s <in.vtk> <out.vtk> <array_name>\n", argv[0]);
    return 1;
  }

  auto mesh = cmesh::ReadPolyData(argv[1]);
  vtkNew<vtkFloatArray> arr;
  arr->SetName(argv[3]);
  arr->SetNumberOfComponents(1);
  arr->SetNumberOfTuples(mesh->GetNumberOfPoints());
  for(vtkIdType i = 0; i < mesh->GetNumberOfPoints(); ++i)
    arr->SetTuple1(i, static_cast<float>(i));
  mesh->GetPointData()->AddArray(arr);

  // mesh_merge_arrays uses the legacy ASCII reader; write ASCII to match.
  cmesh::WritePolyData(mesh, argv[2], /*binary=*/false);
  std::printf("Tagged %s -> %s with array %s\n", argv[1], argv[2], argv[3]);
  return 0;
}
