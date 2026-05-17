#include "TestHarness.h"

#include "core/ComputeNormals.h"
#include "core/DecimateMesh.h"
#include "core/FlipNormals.h"
#include "core/MeshDiff.h"
#include "core/MeshIO.h"
#include "core/SmoothMesh.h"

#include <vtkCellArray.h>
#include <vtkCellArrayIterator.h>
#include <vtkCubeSource.h>
#include <vtkIdList.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkTriangleFilter.h>

#include <cstdio>
#include <string>

static vtkSmartPointer<vtkPolyData> MakeCube()
{
  vtkNew<vtkCubeSource> cube;
  cube->SetXLength(1.0);
  cube->SetYLength(1.0);
  cube->SetZLength(1.0);
  cube->Update();

  vtkNew<vtkTriangleFilter> tri;
  tri->SetInputConnection(cube->GetOutputPort());
  tri->Update();

  vtkSmartPointer<vtkPolyData> out = vtkSmartPointer<vtkPolyData>::New();
  out->DeepCopy(tri->GetOutput());
  return out;
}

int main(int argc, char *argv[])
{
  std::string dir = (argc > 1) ? argv[1] : ".";

  // --- SmoothMesh
  {
    cmesh::SmoothParams p;
    p.iterations = 5;
    auto out = cmesh::SmoothMesh(MakeCube(), p);
    CM_CHECK(out != nullptr);
    CM_CHECK(out->GetNumberOfPoints() > 0);
  }

  // --- DecimateMesh
  {
    auto cube = MakeCube();
    vtkNew<vtkTriangleFilter> tri;
    tri->SetInputData(cube);
    tri->Update();
    vtkIdType before = tri->GetOutput()->GetNumberOfCells();

    cmesh::DecimateParams p;
    p.reduction = 0.5;
    p.preserve_topology = false;
    auto out = cmesh::DecimateMesh(tri->GetOutput(), p);
    CM_CHECK(out->GetNumberOfCells() <= before);
  }

  // --- ComputeNormals
  {
    cmesh::NormalsParams p;
    auto out = cmesh::ComputeNormals(MakeCube(), p);
    CM_CHECK(out->GetPointData()->GetNormals() != nullptr);
  }

  // --- FlipNormals: every cell's winding should reverse.
  {
    auto cube = MakeCube();
    vtkNew<vtkIdList> orig;
    cube->GetCellPoints(0, orig);
    vtkIdType n_orig = orig->GetNumberOfIds();
    CM_CHECK(n_orig >= 3);
    std::vector<vtkIdType> orig_ids(n_orig);
    for(vtkIdType i = 0; i < n_orig; ++i) orig_ids[i] = orig->GetId(i);

    auto flipped = cmesh::FlipNormals(cube);
    vtkNew<vtkIdList> after;
    flipped->GetCellPoints(0, after);
    CM_CHECK_EQ(after->GetNumberOfIds(), n_orig);
    for(vtkIdType i = 0; i < n_orig; ++i)
      CM_CHECK_EQ(after->GetId(i), orig_ids[n_orig - 1 - i]);
  }

  // --- MeshDiff against a shifted copy.
  {
    auto cube = MakeCube();
    auto shifted = MakeCube();
    vtkPoints *pts = shifted->GetPoints();
    for(vtkIdType i = 0; i < pts->GetNumberOfPoints(); ++i)
    {
      double p[3]; pts->GetPoint(i, p); p[0] += 1.0; pts->SetPoint(i, p);
    }

    cmesh::MeshDiffParams p;
    p.array_name = "D";
    cmesh::MeshDiffStats stats;
    auto annotated = cmesh::MeshDiff(cube, shifted, p, &stats);
    CM_CHECK(annotated->GetPointData()->GetArray("D") != nullptr);
    CM_CHECK(stats.mean > 0.0);
    CM_CHECK(stats.hausdorff >= stats.mean);
  }

  std::printf("MeshOpsTest passed\n");
  return 0;
}
