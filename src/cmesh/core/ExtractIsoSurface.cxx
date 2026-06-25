#include "cmesh/core/ExtractIsoSurface.h"

#include "cmesh/core/Error.h"
#include "cmesh/core/PixelDispatch.h"
#include "cmesh/core/VTKToITKBridge.h"

#include <itkVTKImageExport.h>

#include <vtkAppendPolyData.h>
#include <vtkCleanPolyData.h>
#include <vtkDecimatePro.h>
#include <vtkDiscreteFlyingEdges3D.h>
#include <vtkDiscreteMarchingCubes.h>
#include <vtkFlyingEdges3D.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkImageImport.h>
#include <vtkMarchingCubes.h>
#include <vtkSurfaceNets3D.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyDataNormals.h>
#include <vtkSmartPointer.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTriangleFilter.h>
#include <vtkUnsignedShortArray.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace cmesh
{

namespace
{

// --- Iso-contour strategies -------------------------------------------------
// Each takes a VTK image input port and returns raw polydata; the caller owns
// the shared post-processing (RAS transform, normals, decimate, clean). The
// strategy boundary is "image in -> labeled mesh out", which lets each flavor
// own its own per-label looping and labeling.

// Continuous, single iso-value at `thr`.
vtkSmartPointer<vtkPolyData>
ContourMarchingCubes(vtkAlgorithmOutput *in, double thr)
{
  vtkNew<vtkMarchingCubes> mc;
  mc->SetInputConnection(in);
  mc->ComputeScalarsOff();
  mc->ComputeGradientsOff();
  mc->ComputeNormalsOn();
  mc->SetNumberOfContours(1);
  mc->SetValue(0, thr);
  mc->Update();
  return mc->GetOutput();
}

vtkSmartPointer<vtkPolyData>
ContourFlyingEdges(vtkAlgorithmOutput *in, double thr)
{
  vtkNew<vtkFlyingEdges3D> fe;
  fe->SetInputConnection(in);
  fe->ComputeScalarsOff();
  fe->ComputeGradientsOff();
  fe->ComputeNormalsOn();
  fe->SetNumberOfContours(1);
  fe->SetValue(0, thr);
  fe->Update();
  return fe->GetOutput();
}

// Per-label: one surface per integer label in [lo, hi], "Label" point scalar.
vtkSmartPointer<vtkPolyData>
ContourDiscreteMarchingCubes(vtkAlgorithmOutput *in, double lo, double hi,
                             AbortToken &abort, const ProgressFn &progress)
{
  vtkNew<vtkAppendPolyData> append;
  const double n_labels = std::max(1.0, hi - lo + 1.0);
  for(double lbl = lo; lbl <= hi; lbl += 1.0)
  {
    abort.ThrowIfRequested();
    if(progress)
      progress(0.2 + 0.5 * (lbl - lo) / n_labels, "contouring labels");
    vtkNew<vtkDiscreteMarchingCubes> dmc;
    dmc->SetInputConnection(in);
    dmc->ComputeGradientsOff();
    dmc->ComputeScalarsOff();
    dmc->ComputeNormalsOn();
    dmc->SetNumberOfContours(1);
    dmc->SetValue(0, lbl);
    dmc->Update();

    vtkPolyData *labelMesh = dmc->GetOutput();
    if(labelMesh->GetNumberOfPoints() == 0) continue;

    vtkNew<vtkUnsignedShortArray> scalar;
    scalar->SetName("Label");
    scalar->SetNumberOfComponents(1);
    scalar->SetNumberOfTuples(labelMesh->GetNumberOfPoints());
    for(vtkIdType i = 0; i < labelMesh->GetNumberOfPoints(); ++i)
      scalar->SetTuple1(i, static_cast<unsigned short>(lbl));
    labelMesh->GetPointData()->SetScalars(scalar);
    append->AddInputData(labelMesh);
  }
  append->Update();
  return append->GetOutput();
}

vtkSmartPointer<vtkPolyData>
ContourDiscreteFlyingEdges(vtkAlgorithmOutput *in, double lo, double hi)
{
  int n = static_cast<int>(std::lround(hi - lo)) + 1;
  vtkNew<vtkDiscreteFlyingEdges3D> dfe;
  dfe->SetInputConnection(in);
  dfe->ComputeGradientsOff();
  dfe->ComputeNormalsOff();
  dfe->ComputeScalarsOn();
  dfe->GenerateValues(n, lo, hi);
  dfe->Update();

  vtkSmartPointer<vtkPolyData> out = dfe->GetOutput();
  if(out->GetPointData()->GetScalars())
    out->GetPointData()->GetScalars()->SetName("Label");
  return out;
}

vtkSmartPointer<vtkPolyData>
ContourSurfaceNets(vtkAlgorithmOutput *in, double lo, double hi)
{
  int n = static_cast<int>(std::lround(hi - lo)) + 1;
  vtkNew<vtkSurfaceNets3D> sn;
  sn->SetInputConnection(in);
  sn->GenerateValues(n, lo, hi);
  sn->SetOutputMeshTypeToTriangles();
  sn->Update();
  return sn->GetOutput();
}

} // namespace

void ValidateIsoSurfaceParams(const IsoSurfaceParams &p)
{
  if(p.smooth_pre < 0.0)
    throw AlgorithmError("ExtractIsoSurface: smooth_pre must be >= 0");
  if(p.decimate < 0.0 || p.decimate >= 1.0)
    throw AlgorithmError("ExtractIsoSurface: decimate must be in [0, 1)");
  if(p.smooth_pre > 0.0 && IsDiscrete(p.method))
    throw AlgorithmError(
        "ExtractIsoSurface: smooth_pre does not apply to discrete methods "
        "(Gaussian-smoothing a label map blends adjacent labels into "
        "spurious intermediate values); smooth the output mesh instead");
}

template <class T>
vtkSmartPointer<vtkPolyData>
ExtractIsoSurface(itk::Image<T, 3> *image, const IsoSurfaceParams &p,
                  ProgressFn progress, AbortToken abort)
{
  if(!image)
    throw AlgorithmError("ExtractIsoSurface: null input image");
  ValidateIsoSurfaceParams(p);
  abort.ThrowIfRequested();
  if(progress) progress(0.0, "importing image");

  using ImageType = itk::Image<T, 3>;
  using ExporterType = itk::VTKImageExport<ImageType>;
  typename ExporterType::Pointer exporter = ExporterType::New();
  exporter->SetInput(image);
  vtkNew<vtkImageImport> importer;
  bridge::Connect(exporter.GetPointer(), importer.GetPointer());

  vtkAlgorithmOutput *mc_input = importer->GetOutputPort();
  vtkNew<vtkImageGaussianSmooth> smoother;
  if(p.smooth_pre > 0.0)
  {
    if(progress) progress(0.05, "pre-smoothing");
    smoother->SetInputConnection(importer->GetOutputPort());
    smoother->SetStandardDeviations(p.smooth_pre, p.smooth_pre, p.smooth_pre);
    smoother->Update();
    mc_input = smoother->GetOutputPort();
  }

  abort.ThrowIfRequested();
  if(progress) progress(0.2, "contouring");
  vtkSmartPointer<vtkPolyData> mesh;

  // Discrete flavors operate per integer label in [lo, hi]; lo is the first
  // label at or above the threshold, hi the max label present in the image.
  double lo = 0.0, hi = 0.0;
  if(IsDiscrete(p.method))
  {
    T imin = std::numeric_limits<T>::max();
    T imax = std::numeric_limits<T>::lowest();
    const T *buf = image->GetBufferPointer();
    size_t n = image->GetBufferedRegion().GetNumberOfPixels();
    for(size_t i = 0; i < n; ++i)
    {
      if(buf[i] < imin) imin = buf[i];
      if(buf[i] > imax) imax = buf[i];
    }
    lo = std::floor(std::max<double>(p.threshold, static_cast<double>(imin)));
    hi = static_cast<double>(imax);
  }

  switch(p.method)
  {
    case IsoMethod::MarchingCubes:
      mesh = ContourMarchingCubes(mc_input, p.threshold);
      break;
    case IsoMethod::FlyingEdges:
      mesh = ContourFlyingEdges(mc_input, p.threshold);
      break;
    case IsoMethod::DiscreteMarchingCubes:
      mesh = ContourDiscreteMarchingCubes(mc_input, lo, hi, abort, progress);
      break;
    case IsoMethod::DiscreteFlyingEdges:
      mesh = ContourDiscreteFlyingEdges(mc_input, lo, hi);
      break;
    case IsoMethod::SurfaceNets:
      mesh = ContourSurfaceNets(mc_input, lo, hi);
      break;
  }

  if(!mesh || mesh->GetNumberOfPoints() == 0)
    throw AlgorithmError("ExtractIsoSurface: no surface produced "
                         "(threshold/range may not intersect the image)");

  if(p.clean)
  {
    if(progress) progress(0.7, "cleaning");
    vtkNew<vtkTriangleFilter> tri1;
    tri1->SetInputData(mesh);
    tri1->PassLinesOff();
    tri1->PassVertsOff();
    tri1->Update();

    vtkNew<vtkCleanPolyData> clean;
    clean->SetInputConnection(tri1->GetOutputPort());
    clean->PointMergingOn();
    clean->SetTolerance(0.0);
    clean->Update();

    vtkNew<vtkTriangleFilter> tri2;
    tri2->SetInputConnection(clean->GetOutputPort());
    tri2->PassLinesOff();
    tri2->PassVertsOff();
    tri2->Update();

    mesh = tri2->GetOutput();
  }

  // VTK-output space -> NIFTI RAS.
  if(progress) progress(0.8, "transforming to RAS");
  auto mat = bridge::VtkToRasMatrix<ImageType>(image);
  vtkNew<vtkTransform> xform;
  xform->SetMatrix(mat);

  vtkNew<vtkTransformPolyDataFilter> txFilter;
  txFilter->SetInputData(mesh);
  txFilter->SetTransform(xform);
  txFilter->Update();
  mesh = txFilter->GetOutput();

  bool flip = (xform->GetMatrix()->Determinant() < 0);
  if(p.compute_normals || flip)
  {
    if(progress) progress(0.85, "computing normals");
    vtkNew<vtkPolyDataNormals> normals;
    normals->SetInputData(mesh);
    if(flip) normals->FlipNormalsOn();
    normals->SplittingOff();
    normals->ConsistencyOn();
    normals->Update();
    mesh = normals->GetOutput();
  }

  if(p.decimate > 0.0)
  {
    if(progress) progress(0.9, "decimating");
    vtkNew<vtkDecimatePro> deci;
    deci->SetInputData(mesh);
    deci->SetTargetReduction(p.decimate);
    deci->PreserveTopologyOn();
    deci->Update();
    mesh = deci->GetOutput();
  }

  abort.ThrowIfRequested();
  if(progress) progress(1.0, "done");
  return mesh;
}

vtkSmartPointer<vtkPolyData>
ExtractIsoSurface(itk::ImageBase<3> *image, const IsoSurfaceParams &p,
                  ProgressFn progress, AbortToken abort)
{
  if(!image)
    throw AlgorithmError("ExtractIsoSurface: null input image");
  return WithPixelType(image, [&](auto *typed) {
    return ExtractIsoSurface(typed, p, progress, abort);
  });
}

// Explicit instantiations — match the closed list in PixelDispatch.h.
template vtkSmartPointer<vtkPolyData> ExtractIsoSurface(itk::Image<unsigned char,  3>*, const IsoSurfaceParams&, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> ExtractIsoSurface(itk::Image<char,           3>*, const IsoSurfaceParams&, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> ExtractIsoSurface(itk::Image<short,          3>*, const IsoSurfaceParams&, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> ExtractIsoSurface(itk::Image<unsigned short, 3>*, const IsoSurfaceParams&, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> ExtractIsoSurface(itk::Image<int,            3>*, const IsoSurfaceParams&, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> ExtractIsoSurface(itk::Image<unsigned int,   3>*, const IsoSurfaceParams&, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> ExtractIsoSurface(itk::Image<float,          3>*, const IsoSurfaceParams&, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> ExtractIsoSurface(itk::Image<double,         3>*, const IsoSurfaceParams&, ProgressFn, AbortToken);

} // namespace cmesh
