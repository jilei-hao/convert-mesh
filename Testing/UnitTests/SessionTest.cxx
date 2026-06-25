// Exercises the public cmesh::cli::Session API: seeding in-memory variables,
// running pipelines against them, and reading results back without touching
// the filesystem.

#include "TestHarness.h"

#include "cmesh/cli/Run.h"
#include "cmesh/core/Error.h"
#include "cmesh/core/ExtractIsoSurface.h"
#include "cmesh/core/Version.h"

#include <itkImage.h>
#include <itkImageRegionIteratorWithIndex.h>

#include <cmath>
#include <sstream>

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

int main()
{
  cmesh::cli::Session session;
  std::ostringstream out, err;

  // Seed an in-memory image, extract a surface, read the mesh back.
  auto img = MakeSphereImage();
  session.SetImageVariable("seg", img.GetPointer());
  CM_CHECK(session.HasVariable("seg"));
  CM_CHECK(!session.HasVariable("surf"));

  int rc = session.Run({"-push", "seg",
                        "-extract-isosurface", "0.5", "--clean",
                        "-popas", "surf"},
                       out, err);
  CM_CHECK_EQ(rc, 0);
  CM_CHECK(session.HasVariable("surf"));

  auto surf = session.GetMeshVariable("surf");
  CM_CHECK(surf != nullptr);
  CM_CHECK(surf->GetNumberOfPoints() > 0);
  CM_CHECK(surf->GetNumberOfCells() > 0);

  // State persists across Run() calls: reuse "surf" in a second pipeline.
  rc = session.Run({"-push", "surf", "-decimate", "0.5", "-popas", "light"},
                   out, err);
  CM_CHECK_EQ(rc, 0);
  auto light = session.GetMeshVariable("light");
  CM_CHECK(light->GetNumberOfCells() < surf->GetNumberOfCells());

  // Seeded meshes work as inputs too.
  session.SetMeshVariable("ref", surf);
  rc = session.Run({"-push", "surf", "-push", "ref", "-meshdiff",
                    "-popas", "diff"},
                   out, err);
  CM_CHECK_EQ(rc, 0);
  CM_CHECK(out.str().find("mean=0") != std::string::npos);

  // Kind mismatches and undefined names throw typed errors.
  CM_CHECK_THROWS(session.GetImageVariable("surf"), cmesh::TypeError);
  CM_CHECK_THROWS(session.GetMeshVariable("seg"), cmesh::TypeError);
  CM_CHECK_THROWS(session.GetMeshVariable("nope"), cmesh::ParseError);

  // Errors inside Run() map to exit codes, not exceptions.
  std::ostringstream err2;
  rc = session.Run({"-bogus-command"}, out, err2);
  CM_CHECK_EQ(rc, 2);
  CM_CHECK(err2.str().find("unknown command") != std::string::npos);

  std::printf("SessionTest passed (ConvertMesh %s)\n",
              CONVERTMESH_VERSION_FULL);
  return 0;
}
