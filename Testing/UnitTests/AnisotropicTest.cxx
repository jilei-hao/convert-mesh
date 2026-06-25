#include "TestHarness.h"

#include "cmesh/core/ExtractIsoSurface.h"

#include <itkImage.h>
#include <itkImageRegionIteratorWithIndex.h>

#include <vtkPolyData.h>

#include <cmath>
#include <cstdio>

// Regression test: spacing != 1, direction != identity, origin != 0.
// Prior to the VtkToRasMatrix fix, this combination caused meshes to
// come out ~`spacing`× too large in X/Y (the "squeezed" output reported
// on real NIFTI segmentations with anisotropic voxels).

using ImageType = itk::Image<float, 3>;

// Sphere of radius 8 *mm* centered at physical (0, 0, 0).
// Image is 40×40×40 voxels, spacing [2, 2, 1], direction diag(1, 1, -1).
// Origin is chosen so the RAS center of the image is (0, 0, 0).
static ImageType::Pointer MakeAnisoSphere()
{
  constexpr int N = 40;
  ImageType::Pointer img = ImageType::New();
  ImageType::SizeType sz  = { { N, N, N } };
  ImageType::IndexType ix = { { 0, 0, 0 } };
  img->SetRegions(ImageType::RegionType(ix, sz));

  ImageType::SpacingType s;
  s[0] = 2.0; s[1] = 2.0; s[2] = 1.0;
  img->SetSpacing(s);

  ImageType::DirectionType D;
  D.SetIdentity();
  D(2, 2) = -1.0;    // flip Z
  img->SetDirection(D);

  // Pick ITK-LPS origin so the voxel-grid center (voxel 20,20,20) lands at
  // physical LPS (0, 0, 0). That is origin = -D * diag(s) * 20.
  ImageType::PointType origin;
  origin[0] = -1.0 * 2.0 * 20.0;      // -40
  origin[1] = -1.0 * 2.0 * 20.0;      // -40
  origin[2] = -(-1.0) * 1.0 * 20.0;   // +20
  img->SetOrigin(origin);

  img->Allocate();

  // Fill with 1 inside a physical-space sphere of radius 8.
  itk::ImageRegionIteratorWithIndex<ImageType> it(img, img->GetBufferedRegion());
  const double r = 8.0;
  for(; !it.IsAtEnd(); ++it)
  {
    ImageType::PointType P_lps;
    img->TransformIndexToPhysicalPoint(it.GetIndex(), P_lps);
    // Convert LPS to RAS: negate X, Y
    double x = -P_lps[0], y = -P_lps[1], z = P_lps[2];
    double d = std::sqrt(x*x + y*y + z*z);
    it.Set(d < r ? 1.0f : 0.0f);
  }
  return img;
}

int main()
{
  auto img = MakeAnisoSphere();

  cmesh::IsoSurfaceParams params;
  params.threshold = 0.5;
  params.clean = true;
  params.compute_normals = true;
  auto mesh = cmesh::ExtractIsoSurface(img.GetPointer(), params);
  CM_CHECK(mesh->GetNumberOfPoints() > 0);

  // Mesh should live around RAS (0, 0, 0) with radius ~8mm.
  double b[6];
  mesh->GetBounds(b);
  std::printf("Aniso bounds: X=[%.3f, %.3f], Y=[%.3f, %.3f], Z=[%.3f, %.3f]\n",
              b[0], b[1], b[2], b[3], b[4], b[5]);
  for(int axis = 0; axis < 3; ++axis)
  {
    double lo = b[2*axis];
    double hi = b[2*axis + 1];
    double span = hi - lo;
    // Physical-space diameter is 16 (radius 8). Marching cubes on a
    // voxelized sphere under-approximates by roughly one voxel per axis —
    // e.g. X/Y span ≈ 14 (spacing 2) and Z span ≈ 15 (spacing 1).
    // Before the VtkToRas fix the spans came back as 32 / 32 / 16
    // (spacing applied twice), which this tolerance clamps out.
    CM_CHECK(span >= 13.0 && span < 18.0);
    CM_CHECK(std::fabs(lo + span / 2.0) < 1.5);  // centered at 0
  }

  std::printf("AnisotropicTest passed\n");
  return 0;
}
