#include "TestHarness.h"

#include "core/ExtractIsoSurface.h"
#include "core/MergeArrays.h"
#include "core/MeshIO.h"
#include "core/RasterizeMesh.h"
#include "core/SampleImageAtMesh.h"
#include "core/WarpMesh.h"

#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIteratorWithIndex.h>
#include <itkVector.h>

#include <vtkFloatArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>

#include <cmath>
#include <cstdio>
#include <string>

using ImageType = itk::Image<float, 3>;

static ImageType::Pointer MakeSphereImage()
{
  constexpr int N = 32;
  ImageType::Pointer img = ImageType::New();
  ImageType::SizeType sz  = { { N, N, N } };
  ImageType::IndexType ix = { { 0, 0, 0 } };
  ImageType::RegionType region(ix, sz);
  img->SetRegions(region);
  ImageType::SpacingType s; s.Fill(1.0);
  img->SetSpacing(s);
  img->Allocate();
  const double c = N / 2.0, r = N / 4.0;
  itk::ImageRegionIteratorWithIndex<ImageType> it(img, region);
  for(; !it.IsAtEnd(); ++it)
  {
    auto i = it.GetIndex();
    double dx = i[0] - c, dy = i[1] - c, dz = i[2] - c;
    double d = std::sqrt(dx*dx + dy*dy + dz*dz);
    it.Set(d < r ? 1.0f : 0.0f);
  }
  return img;
}

static vtkSmartPointer<vtkPolyData> SphereMesh()
{
  auto img = MakeSphereImage();
  cmesh::IsoSurfaceParams p;
  p.threshold = 0.5;
  p.clean = true;
  return cmesh::ExtractIsoSurface(img.GetPointer(), p);
}


int main(int argc, char *argv[])
{
  std::string dir = (argc > 1) ? argv[1] : ".";

  // ------------------------------------------------------------------
  // RasterizeMesh: mesh -> binary image, voxel count must be > 0.
  // ------------------------------------------------------------------
  {
    auto mesh = SphereMesh();
    cmesh::RasterizeParams<float> p;
    p.spacing[0] = p.spacing[1] = p.spacing[2] = 0.5;
    p.margin = 1.0;
    auto img = cmesh::RasterizeMesh<float>(mesh, p);
    size_t n = img->GetBufferedRegion().GetNumberOfPixels();
    size_t inside = 0;
    const float *buf = img->GetBufferPointer();
    for(size_t i = 0; i < n; ++i) if(buf[i] > 0.5f) ++inside;
    CM_CHECK(inside > 0);
    CM_CHECK(inside < n);
  }

  // ------------------------------------------------------------------
  // SampleImageAtMesh: interior points sample ~1, surface < 1.
  // ------------------------------------------------------------------
  {
    auto mesh  = SphereMesh();
    auto image = MakeSphereImage();
    cmesh::SampleParams p;
    p.array_name = "Sample";
    auto out = cmesh::SampleImageAtMesh(mesh, image.GetPointer(), p);
    auto arr = out->GetPointData()->GetArray("Sample");
    CM_CHECK(arr != nullptr);
    for(vtkIdType i = 0; i < arr->GetNumberOfTuples(); ++i)
    {
      double v = arr->GetTuple1(i);
      CM_CHECK(v >= -0.01 && v <= 1.01);
    }
  }

  // ------------------------------------------------------------------
  // WarpMesh: constant translation (dx=2,0,0) shifts every point by +2
  // in LPS, which is -2 in RAS (since RAS negates X).
  // ------------------------------------------------------------------
  {
    auto mesh = SphereMesh();

    cmesh::WarpField::Pointer warp = cmesh::WarpField::New();
    cmesh::WarpField::SizeType sz = { { 32, 32, 32 } };
    cmesh::WarpField::IndexType ix = { { 0, 0, 0 } };
    warp->SetRegions(cmesh::WarpField::RegionType(ix, sz));
    cmesh::WarpField::SpacingType s; s.Fill(1.0);
    warp->SetSpacing(s);
    warp->Allocate();
    itk::Vector<float, 3> v; v[0] = 2.0; v[1] = 0.0; v[2] = 0.0;
    warp->FillBuffer(v);

    std::string warp_path = dir + "/warp-tx.nii.gz";
    itk::ImageFileWriter<cmesh::WarpField>::Pointer w
        = itk::ImageFileWriter<cmesh::WarpField>::New();
    w->SetFileName(warp_path);
    w->SetInput(warp);
    w->SetUseCompression(true);
    w->Update();

    double before[3]; mesh->GetPoint(0, before);

    cmesh::WarpMeshParams wp;
    auto out = cmesh::WarpMesh(mesh, warp.GetPointer(), wp);

    double after[3];
    out->GetPoint(0, after);
    CM_CHECK(std::fabs((after[0] - before[0]) - (-2.0)) < 1e-4);
    CM_CHECK(std::fabs(after[1] - before[1]) < 1e-4);
    CM_CHECK(std::fabs(after[2] - before[2]) < 1e-4);
  }

  // ------------------------------------------------------------------
  // MergeArrays: copy an array between two identical meshes.
  // ------------------------------------------------------------------
  {
    auto mesh = SphereMesh();
    vtkNew<vtkFloatArray> arr;
    arr->SetName("Colors");
    arr->SetNumberOfTuples(mesh->GetNumberOfPoints());
    for(vtkIdType i = 0; i < mesh->GetNumberOfPoints(); ++i)
      arr->SetTuple1(i, static_cast<float>(i));
    mesh->GetPointData()->AddArray(arr);

    std::string src_path = dir + "/sphere-with-colors.vtp";
    cmesh::WritePolyData(mesh, src_path);

    auto bare = SphereMesh();
    CM_CHECK(bare->GetPointData()->GetArray("Colors") == nullptr);

    auto source = cmesh::ReadPolyData(src_path);
    cmesh::MergeArraysParams mp;
    mp.array_name = "Colors";
    auto merged = cmesh::MergeArrays(bare, source, mp);
    auto out_arr = merged->GetPointData()->GetArray("Colors");
    CM_CHECK(out_arr != nullptr);
    CM_CHECK_EQ(out_arr->GetNumberOfTuples(), mesh->GetNumberOfPoints());
  }

  std::printf("InteropTest passed\n");
  return 0;
}
