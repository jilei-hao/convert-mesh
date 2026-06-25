#include "TestHarness.h"

#include "cmesh/core/ExtractIsoSurface.h"
#include "cmesh/core/MeshIO.h"

#include <itkImage.h>
#include <itkImageFileWriter.h>
#include <itkImageRegionIteratorWithIndex.h>

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

  const double cx = N / 2.0, cy = N / 2.0, cz = N / 2.0;
  const double r  = N / 4.0;
  itk::ImageRegionIteratorWithIndex<ImageType> it(img, region);
  for(; !it.IsAtEnd(); ++it)
  {
    auto i = it.GetIndex();
    double dx = i[0] - cx, dy = i[1] - cy, dz = i[2] - cz;
    double d = std::sqrt(dx * dx + dy * dy + dz * dz);
    it.Set(d < r ? 1.0f : 0.0f);
  }
  return img;
}

int main(int argc, char *argv[])
{
  std::string dir = (argc > 1) ? argv[1] : ".";
  auto img = MakeSphereImage();

  std::string img_path = dir + "/sphere.nii.gz";
  itk::ImageFileWriter<ImageType>::Pointer w = itk::ImageFileWriter<ImageType>::New();
  w->SetFileName(img_path);
  w->SetInput(img);
  w->SetUseCompression(true);
  w->Update();

  cmesh::IsoSurfaceParams params;
  params.threshold = 0.5;
  params.clean = true;
  params.compute_normals = true;
  auto mesh = cmesh::ExtractIsoSurface(img.GetPointer(), params);

  CM_CHECK(mesh != nullptr);
  CM_CHECK(mesh->GetNumberOfPoints() > 0);
  CM_CHECK(mesh->GetNumberOfCells()  > 0);

  double bounds[6];
  mesh->GetBounds(bounds);
  double dx = bounds[1] - bounds[0];
  double dy = bounds[3] - bounds[2];
  double dz = bounds[5] - bounds[4];
  CM_CHECK(dx > 10.0 && dx < 20.0);
  CM_CHECK(dy > 10.0 && dy < 20.0);
  CM_CHECK(dz > 10.0 && dz < 20.0);

  cmesh::WritePolyData(mesh, dir + "/sphere-mesh.vtp");

  std::printf("IsoSurfaceTest passed: %lld points, %lld cells\n",
              static_cast<long long>(mesh->GetNumberOfPoints()),
              static_cast<long long>(mesh->GetNumberOfCells()));
  return 0;
}
