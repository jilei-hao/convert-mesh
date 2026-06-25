// CmrepParityTest — exercise each cmesh adapter against pre-generated cmrep
// reference outputs (Testing/Fixtures/cmrep-truth/) and verify that the
// results match within an algorithm-appropriate tolerance.
//
// The reference fixtures are produced offline by
//   scripts/generate-cmrep-ground-truth.sh
// against a 32^3 synthetic sphere image. We deliberately keep the inputs
// small so the fixtures can live in the source tree.

#include "TestHarness.h"

#include "cmesh/core/Backend.h"
#include "cmesh/core/DecimateMesh.h"
#include "cmesh/core/ExtractIsoSurface.h"
#include "cmesh/core/ImageIO.h"
#include "cmesh/core/MergeArrays.h"
#include "cmesh/core/MeshIO.h"
#include "cmesh/core/RasterizeMesh.h"
#include "cmesh/core/SampleImageAtMesh.h"
#include "cmesh/core/SmoothMesh.h"

#include <itkImage.h>
#include <itkImageRegionConstIterator.h>

#include <vtkCellLocator.h>
#include <vtkDataArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkTriangleFilter.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

using ImageType = itk::Image<float, 3>;

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Return the directed Hausdorff distance from `src` to `ref` (max over
// vertices of `src` of the closest-point distance to a triangle of `ref`).
// Mirrors what cmrep's meshdiff reports as "hausdorff(source->ref)" and what
// cmesh's MeshDiff adapter computes internally — we duplicate the logic here
// rather than depend on the adapter's stack-based interface, since this test
// already needs to call closest-point on raw vtkPolyData many times.
static double DirectedHausdorff(vtkPolyData *src, vtkPolyData *ref,
                                double *mean_out = nullptr,
                                double *rms_out  = nullptr)
{
  vtkNew<vtkTriangleFilter> tri;
  tri->SetInputData(ref);
  tri->PassLinesOff();
  tri->PassVertsOff();
  tri->Update();

  vtkNew<vtkCellLocator> loc;
  loc->SetDataSet(tri->GetOutput());
  loc->BuildLocator();

  double dmax = 0.0, sum = 0.0, sum_sq = 0.0;
  vtkIdType n = src->GetNumberOfPoints();
  for(vtkIdType i = 0; i < n; ++i)
  {
    double pt[3]; src->GetPoint(i, pt);
    double cp[3]; vtkIdType cid; int sid; double d2 = 0.0;
    loc->FindClosestPoint(pt, cp, cid, sid, d2);
    double d = std::sqrt(d2);
    if(d > dmax) dmax = d;
    sum    += d;
    sum_sq += d * d;
  }
  if(mean_out) *mean_out = (n > 0) ? sum / n : 0.0;
  if(rms_out)  *rms_out  = (n > 0) ? std::sqrt(sum_sq / n) : 0.0;
  return dmax;
}

// Symmetric Hausdorff: max of the two directed Hausdorff distances. This
// is the value cmrep's meshdiff prints in its symmetric mode and the most
// honest "are these the same surface" metric.
static double SymmetricHausdorff(vtkPolyData *a, vtkPolyData *b)
{
  return std::max(DirectedHausdorff(a, b), DirectedHausdorff(b, a));
}

// Dice coefficient between two binary images of identical geometry.
// Treats any nonzero voxel as "inside".
static double Dice(ImageType *a, ImageType *b)
{
  size_t na = a->GetBufferedRegion().GetNumberOfPixels();
  size_t nb = b->GetBufferedRegion().GetNumberOfPixels();
  CM_CHECK_EQ(na, nb);
  itk::ImageRegionConstIterator<ImageType> ita(a, a->GetBufferedRegion());
  itk::ImageRegionConstIterator<ImageType> itb(b, b->GetBufferedRegion());
  size_t inter = 0, sum_a = 0, sum_b = 0;
  for(; !ita.IsAtEnd(); ++ita, ++itb)
  {
    bool ia = ita.Get() > 0.5f;
    bool ib = itb.Get() > 0.5f;
    if(ia)        ++sum_a;
    if(ib)        ++sum_b;
    if(ia && ib)  ++inter;
  }
  if(sum_a + sum_b == 0) return 1.0;
  return (2.0 * inter) / static_cast<double>(sum_a + sum_b);
}

// Total physical volume of "inside" voxels (value > 0.5) in a binary image.
// Geometry-agnostic — useful for comparing two rasterizations that live in
// different ITK frames (e.g. cmrep mesh2img writes (-1,-1,1) direction
// while cmesh writes identity).
static double InsideVolume(ImageType *img)
{
  auto sp = img->GetSpacing();
  double vox_vol = sp[0] * sp[1] * sp[2];
  size_t n_inside = 0;
  itk::ImageRegionConstIterator<ImageType> it(img, img->GetBufferedRegion());
  for(; !it.IsAtEnd(); ++it) if(it.Get() > 0.5f) ++n_inside;
  return n_inside * vox_vol;
}


// ---------------------------------------------------------------------------
// Parity cases
// ---------------------------------------------------------------------------

static void ExtractIsoSurfaceParity(const std::string &dir)
{
  // cmrep: vtklevelset -k -f sphere.nii.gz sphere-mesh.vtk 0.5
  // cmesh: ExtractIsoSurface(threshold=0.5, clean=true, compute_normals=true)
  // Both pipelines share vtkMarchingCubes + vtkCleanPolyData so we expect
  // a very tight match.
  auto img = cmesh::ReadImageAs<float>(dir + "/sphere.nii.gz");

  cmesh::IsoSurfaceParams p;
  p.threshold = 0.5;
  p.clean = true;
  p.compute_normals = true;
  auto cmesh_mesh = cmesh::ExtractIsoSurface(img.GetPointer(), p);

  auto cmrep_mesh = cmesh::ReadPolyData(dir + "/sphere-mesh.vtk");

  double mean = 0.0, rms = 0.0;
  double haus = SymmetricHausdorff(cmesh_mesh, cmrep_mesh);
  DirectedHausdorff(cmesh_mesh, cmrep_mesh, &mean, &rms);

  std::printf("[iso-surface]   cmesh:%lld pts cmrep:%lld pts  "
              "haus=%.4f mean=%.4f rms=%.4f\n",
              (long long)cmesh_mesh->GetNumberOfPoints(),
              (long long)cmrep_mesh->GetNumberOfPoints(),
              haus, mean, rms);

  // Sub-voxel agreement between the two pipelines.
  CM_CHECK(haus < 0.5);
  CM_CHECK(mean < 0.05);
}

static void SampleImageParity(const std::string &dir)
{
  // cmrep: mesh_image_sample -i 1 sphere-mesh.vtk sphere.nii.gz
  //                          sphere-sampled.vtk Intensity
  // cmesh: SampleImageAtMesh(array_name="Intensity") with linear interp
  auto mesh = cmesh::ReadPolyData(dir + "/sphere-mesh.vtk");
  auto img  = cmesh::ReadImageAs<float>(dir + "/sphere.nii.gz");

  cmesh::SampleParams p;
  p.array_name    = "Intensity";
  p.interpolation = cmesh::Interpolation::Linear;
  auto cmesh_mesh = cmesh::SampleImageAtMesh(mesh, img.GetPointer(), p);

  auto cmrep_mesh = cmesh::ReadPolyData(dir + "/sphere-sampled.vtk");

  auto a_cmesh = cmesh_mesh->GetPointData()->GetArray("Intensity");
  auto a_cmrep = cmrep_mesh->GetPointData()->GetArray("Intensity");
  CM_CHECK(a_cmesh != nullptr);
  CM_CHECK(a_cmrep != nullptr);
  CM_CHECK_EQ(a_cmesh->GetNumberOfTuples(), a_cmrep->GetNumberOfTuples());

  double max_abs = 0.0, sum_abs = 0.0;
  vtkIdType n = a_cmesh->GetNumberOfTuples();
  for(vtkIdType i = 0; i < n; ++i)
  {
    double v_cmesh = a_cmesh->GetTuple1(i);
    double v_cmrep = a_cmrep->GetTuple1(i);
    double d = std::fabs(v_cmesh - v_cmrep);
    if(d > max_abs) max_abs = d;
    sum_abs += d;
  }
  std::printf("[sample-image]  N=%lld  max|Δ|=%.6f mean|Δ|=%.6f\n",
              (long long)n, max_abs, sum_abs / n);

  // Linear interpolation should be identical up to floating-point error.
  CM_CHECK(max_abs < 1e-3);
}

static void RasterizeParity(const std::string &dir)
{
  // cmesh: RasterizeMesh with ref=sphere.nii.gz (identity direction).
  // cmrep: mesh2img -a 1 1 1 0 — auto-bbox mode, which writes a (-1,-1,1)
  // direction matrix that cmesh's RasterizeMesh refuses (non-identity ref).
  // We can't put the two outputs on the same voxel grid, but we *can*
  // compare them by the physical volume of the rasterized solid, plus
  // confirm cmesh's output is a faithful rasterization of the original
  // sphere image (Dice on the same grid).
  auto mesh = cmesh::ReadPolyData(dir + "/sphere-mesh.vtk");
  auto ref  = cmesh::ReadImageAs<float>(dir + "/sphere.nii.gz");

  cmesh::RasterizeParams<float> p;
  p.reference = ref.GetPointer();
  auto cmesh_img = cmesh::RasterizeMesh<float>(mesh, p);

  auto cmrep_img   = cmesh::ReadImageAs<float>(dir + "/sphere-rasterized.nii.gz");
  auto sphere_img  = cmesh::ReadImageAs<float>(dir + "/sphere.nii.gz");

  double dice_self = Dice(cmesh_img, sphere_img);
  double v_cmesh   = InsideVolume(cmesh_img);
  double v_cmrep   = InsideVolume(cmrep_img);
  double v_sphere  = InsideVolume(sphere_img);
  double rel_diff  = std::fabs(v_cmesh - v_cmrep) /
                     std::max(1.0, (v_cmesh + v_cmrep) * 0.5);
  std::printf("[rasterize]     dice(cmesh,sphere)=%.4f  "
              "vol cmesh=%.1f cmrep=%.1f sphere=%.1f  rel|Δ|=%.4f\n",
              dice_self, v_cmesh, v_cmrep, v_sphere, rel_diff);

  // Primary check: cmesh's rasterize should be a near-perfect inverse of
  // the iso-surface extraction, so it should agree with the original sphere
  // image at near-100% Dice.
  CM_CHECK(dice_self > 0.95);
  // Cross-check vs cmrep is necessarily loose: cmrep's mesh2img scan-fill
  // (drawBinaryTrianglesFilled) and cmesh's vtkPolyDataToImageStencil treat
  // surface voxels differently, and on a 32^3 sphere the surface-cell
  // ambiguity is a sizable fraction of the total. We require only that the
  // two volumes are in the same ballpark.
  CM_CHECK(rel_diff < 0.5);
}

static void MergeArraysParity(const std::string &dir)
{
  // cmrep: mesh_merge_arrays -r sphere-mesh.vtk sphere-merged.vtk Tag sphere-tagged.vtk
  // cmesh: MergeArrays(source=sphere-tagged.vtk, name="Tag")
  auto dest   = cmesh::ReadPolyData(dir + "/sphere-mesh.vtk");
  auto source = cmesh::ReadPolyData(dir + "/sphere-tagged.vtk");

  cmesh::MergeArraysParams p;
  p.array_name = "Tag";
  auto cmesh_mesh = cmesh::MergeArrays(dest, source, p);

  auto cmrep_mesh = cmesh::ReadPolyData(dir + "/sphere-merged.vtk");

  auto a_cmesh = cmesh_mesh->GetPointData()->GetArray("Tag");
  auto a_cmrep = cmrep_mesh->GetPointData()->GetArray("Tag");
  CM_CHECK(a_cmesh != nullptr);
  CM_CHECK(a_cmrep != nullptr);
  CM_CHECK_EQ(a_cmesh->GetNumberOfTuples(), a_cmrep->GetNumberOfTuples());

  double max_abs = 0.0;
  vtkIdType n = a_cmesh->GetNumberOfTuples();
  for(vtkIdType i = 0; i < n; ++i)
  {
    double d = std::fabs(a_cmesh->GetTuple1(i) - a_cmrep->GetTuple1(i));
    if(d > max_abs) max_abs = d;
  }
  std::printf("[merge-array]   N=%lld  max|Δ|=%.6f\n", (long long)n, max_abs);

  // Tag is integer-valued and copied verbatim.
  CM_CHECK(max_abs < 1e-6);
}

static void SmoothMeshParity(const std::string &dir)
{
  // cmrep: mesh_smooth_curv -iter 50 -mu -0.51 -lambda 0.5 (Taubin)
  // cmesh: SmoothMesh (VTK Laplacian) — different algorithm, so we apply a
  // generous tolerance. Both shrink/relax a sphere and should still trace
  // out essentially the same surface.
  auto in = cmesh::ReadPolyData(dir + "/sphere-mesh.vtk");
  cmesh::SmoothParams p;
  p.iterations = 50;
  p.relaxation_factor = 0.1;
  auto cmesh_mesh = cmesh::SmoothMesh(in, p);

  auto cmrep_mesh = cmesh::ReadPolyData(dir + "/sphere-smoothed-vcg.vtk");

  double mean = 0.0, rms = 0.0;
  double haus = SymmetricHausdorff(cmesh_mesh, cmrep_mesh);
  DirectedHausdorff(cmesh_mesh, cmrep_mesh, &mean, &rms);
  std::printf("[smooth-mesh]   cmesh:%lld cmrep:%lld  "
              "haus=%.4f mean=%.4f rms=%.4f (Laplacian vs Taubin)\n",
              (long long)cmesh_mesh->GetNumberOfPoints(),
              (long long)cmrep_mesh->GetNumberOfPoints(),
              haus, mean, rms);

  // Laplacian shrinks more aggressively than Taubin — accept up to ~3 voxels
  // Hausdorff and 1 voxel mean. The point of the test is to flag a
  // regression that drives one implementation off the surface entirely.
  CM_CHECK(haus < 3.0);
  CM_CHECK(mean < 1.0);
}

static void DecimateMeshParity(const std::string &dir)
{
  // cmrep: mesh_decimate_vcg sphere-mesh.vtk sphere-decimated-vcg.vtk 0.5
  //        (reduce face count by 50% via VCG quadric edge collapse)
  // cmesh: DecimateMesh with -use-vcg — same algorithm and parameters, so
  // outputs should be identical (modulo deterministic VCG ordering).

  // ----- VTK backend: keep the loose ballpark check we used before -----
  {
    auto in = cmesh::ReadPolyData(dir + "/sphere-mesh.vtk");
    cmesh::DecimateParams p;
    p.reduction = 0.5;
    p.preserve_topology = true;
    auto cmesh_mesh = cmesh::DecimateMesh(in, p);

    auto cmrep_mesh = cmesh::ReadPolyData(dir + "/sphere-decimated-vcg.vtk");

    double haus = SymmetricHausdorff(cmesh_mesh, cmrep_mesh);
    double mean = 0.0, rms = 0.0;
    DirectedHausdorff(cmesh_mesh, cmrep_mesh, &mean, &rms);
    std::printf("[decimate-vtk]  cmesh:%lld cmrep:%lld  "
                "haus=%.4f mean=%.4f rms=%.4f (VTK vs VCG)\n",
                (long long)cmesh_mesh->GetNumberOfPoints(),
                (long long)cmrep_mesh->GetNumberOfPoints(),
                haus, mean, rms);

    // Different algorithms — accept fraction-of-voxel agreement only.
    CM_CHECK(haus < 1.5);
    CM_CHECK(mean < 0.3);
  }

#ifdef CONVERTMESH_HAVE_VCG
  // ----- VCG backend: should match cmrep to floating-point precision -----
  {
    auto in = cmesh::ReadPolyData(dir + "/sphere-mesh.vtk");
    cmesh::DecimateParams p;
    p.reduction = 0.5;
    p.preserve_topology = true;
    p.backend = cmesh::Backend::VCG;
    auto cmesh_mesh = cmesh::DecimateMesh(in, p);

    auto cmrep_mesh = cmesh::ReadPolyData(dir + "/sphere-decimated-vcg.vtk");

    double haus = SymmetricHausdorff(cmesh_mesh, cmrep_mesh);
    double mean = 0.0, rms = 0.0;
    DirectedHausdorff(cmesh_mesh, cmrep_mesh, &mean, &rms);
    std::printf("[decimate-vcg]  cmesh:%lld cmrep:%lld  "
                "haus=%.6f mean=%.6f rms=%.6f (VCG vs VCG)\n",
                (long long)cmesh_mesh->GetNumberOfPoints(),
                (long long)cmrep_mesh->GetNumberOfPoints(),
                haus, mean, rms);

    // cmesh and cmrep run the same VCG quadric edge collapse with the same
    // parameters, so the surfaces should match within float round-off.
    CM_CHECK_EQ(cmesh_mesh->GetNumberOfPoints(),
                cmrep_mesh->GetNumberOfPoints());
    CM_CHECK(haus < 1e-4);
    CM_CHECK(mean < 1e-5);
  }
#else
  std::printf("[decimate-vcg]  skipped — built without CONVERTMESH_BUILD_VCG\n");
#endif
}


int main(int argc, char *argv[])
{
  if(argc < 2)
  {
    std::fprintf(stderr,
                 "Usage: %s <cmrep-truth-dir>\n"
                 "  Pass the path to ConvertMesh/Testing/Fixtures/cmrep-truth/\n",
                 argv[0]);
    return 1;
  }
  const std::string dir = argv[1];

  ExtractIsoSurfaceParity(dir);
  SampleImageParity(dir);
  RasterizeParity(dir);
  MergeArraysParity(dir);
  SmoothMeshParity(dir);
  DecimateMeshParity(dir);

  std::printf("CmrepParityTest passed\n");
  return 0;
}
