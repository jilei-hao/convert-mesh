#include "core/ExtractIsoSurface.h"

#include "core/Error.h"
#include "core/PixelDispatch.h"
#include "core/VTKToITKBridge.h"

#include <itkVTKImageExport.h>

#include <vtkAppendPolyData.h>
#include <vtkCleanPolyData.h>
#include <vtkDecimatePro.h>
#include <vtkDiscreteMarchingCubes.h>
#include <vtkImageGaussianSmooth.h>
#include <vtkImageImport.h>
#include <vtkMarchingCubes.h>
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

template <class T>
vtkSmartPointer<vtkPolyData>
ExtractIsoSurface(itk::Image<T, 3> *image, const IsoSurfaceParams &p,
                  ProgressFn /*progress*/, AbortToken abort)
{
  if(!image)
    throw AlgorithmError("ExtractIsoSurface: null input image");
  abort.ThrowIfRequested();

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
    smoother->SetInputConnection(importer->GetOutputPort());
    smoother->SetStandardDeviations(p.smooth_pre, p.smooth_pre, p.smooth_pre);
    smoother->Update();
    mc_input = smoother->GetOutputPort();
  }

  abort.ThrowIfRequested();
  vtkSmartPointer<vtkPolyData> mesh;

  if(p.multi_label)
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

    vtkNew<vtkAppendPolyData> append;
    double start = std::max<double>(p.threshold, static_cast<double>(imin));
    for(double lbl = std::floor(start); lbl <= static_cast<double>(imax); lbl += 1.0)
    {
      abort.ThrowIfRequested();
      vtkNew<vtkDiscreteMarchingCubes> dmc;
      dmc->SetInputConnection(mc_input);
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
    mesh = append->GetOutput();
  }
  else
  {
    vtkNew<vtkMarchingCubes> mc;
    mc->SetInputConnection(mc_input);
    mc->ComputeScalarsOff();
    mc->ComputeGradientsOff();
    mc->ComputeNormalsOn();
    mc->SetNumberOfContours(1);
    mc->SetValue(0, p.threshold);
    mc->Update();
    mesh = mc->GetOutput();
  }

  if(!mesh || mesh->GetNumberOfPoints() == 0)
    throw AlgorithmError("ExtractIsoSurface: no surface produced "
                         "(threshold/range may not intersect the image)");

  if(p.clean)
  {
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
    vtkNew<vtkDecimatePro> deci;
    deci->SetInputData(mesh);
    deci->SetTargetReduction(p.decimate);
    deci->PreserveTopologyOn();
    deci->Update();
    mesh = deci->GetOutput();
  }

  abort.ThrowIfRequested();
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
