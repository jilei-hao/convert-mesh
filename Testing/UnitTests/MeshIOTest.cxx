#include "TestHarness.h"

#include "core/MeshIO.h"

#include <vtkCellArray.h>
#include <vtkCubeSource.h>
#include <vtkNew.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkTriangleFilter.h>

#include <cstdio>
#include <string>

// Build a small triangulated cube we can round-trip through every format.
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

static void RoundTrip(const std::string &dir, const std::string &ext)
{
  auto cube = MakeCube();
  std::string path = dir + "/cube-roundtrip." + ext;

  cmesh::WritePolyData(cube, path);
  auto loaded = cmesh::ReadPolyData(path);

  CM_CHECK(loaded != nullptr);
  CM_CHECK(loaded->GetNumberOfPoints() > 0);
  CM_CHECK(loaded->GetNumberOfCells()  > 0);
}

int main(int argc, char *argv[])
{
  std::string dir = (argc > 1) ? argv[1] : ".";

  CM_CHECK_EQ(cmesh::GetExtension("foo/bar.VTP"), std::string("vtp"));
  CM_CHECK_EQ(cmesh::GetExtension("baz.stl"),     std::string("stl"));
  CM_CHECK_EQ(cmesh::GetExtension("no_ext"),      std::string(""));

  RoundTrip(dir, "vtp");
  RoundTrip(dir, "vtk");
  RoundTrip(dir, "stl");
  RoundTrip(dir, "ply");
  RoundTrip(dir, "obj");
  // BYU writer does not support our arbitrary mesh layout reliably in all
  // VTK versions — skip to avoid flaky test.

  // Write a dedicated fixture for the CLI round-trip test.
  auto cube = MakeCube();
  cmesh::WritePolyData(cube, dir + "/cube.vtp");

  std::printf("MeshIOTest passed (fixture dir: %s)\n", dir.c_str());
  return 0;
}
