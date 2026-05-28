#include "core/SampleImageAtMesh.h"

#include "core/Error.h"
#include "core/PixelDispatch.h"
#include "core/VTKToITKBridge.h"

#include <itkBSplineInterpolateImageFunction.h>
#include <itkInterpolateImageFunction.h>
#include <itkLinearInterpolateImageFunction.h>
#include <itkNearestNeighborInterpolateImageFunction.h>

#include <vtkFloatArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>

namespace cmesh
{

namespace
{
template <class ImageType>
typename itk::InterpolateImageFunction<ImageType, double>::Pointer
MakeInterpolator(Interpolation mode, ImageType *image)
{
  using Base = itk::InterpolateImageFunction<ImageType, double>;
  typename Base::Pointer interp;
  switch(mode)
  {
    case Interpolation::NearestNeighbor:
    {
      auto nn = itk::NearestNeighborInterpolateImageFunction<ImageType, double>::New();
      interp = nn.GetPointer();
      break;
    }
    case Interpolation::BSpline:
    {
      auto b = itk::BSplineInterpolateImageFunction<ImageType, double>::New();
      b->SetSplineOrder(3);
      interp = b.GetPointer();
      break;
    }
    case Interpolation::Linear:
    default:
    {
      auto lin = itk::LinearInterpolateImageFunction<ImageType, double>::New();
      interp = lin.GetPointer();
      break;
    }
  }
  interp->SetInputImage(image);
  return interp;
}
} // namespace


template <class T>
vtkSmartPointer<vtkPolyData>
SampleImageAtMesh(vtkPolyData *mesh, itk::Image<T, 3> *image,
                  const SampleParams &p, SampleStats *out_stats,
                  ProgressFn /*progress*/, AbortToken abort)
{
  if(!mesh)  throw AlgorithmError("SampleImageAtMesh: null mesh");
  if(!image) throw AlgorithmError("SampleImageAtMesh: null image");
  abort.ThrowIfRequested();

  using ImageType = itk::Image<T, 3>;
  auto interp = MakeInterpolator<ImageType>(p.interpolation, image);

  vtkIdType n = mesh->GetNumberOfPoints();
  vtkNew<vtkFloatArray> arr;
  arr->SetName(p.array_name.c_str());
  arr->SetNumberOfComponents(1);
  arr->SetNumberOfTuples(n);

  vtkIdType n_outside = 0;
  for(vtkIdType i = 0; i < n; ++i)
  {
    if((i & 0x3fff) == 0) abort.ThrowIfRequested();

    double pt_ras[3];
    mesh->GetPoint(i, pt_ras);
    typename ImageType::PointType P;
    bridge::RasToLpsPoint<typename ImageType::PointType>(pt_ras, P);

    double v;
    if(interp->IsInsideBuffer(P))
      v = interp->Evaluate(P);
    else
    {
      v = p.background_value;
      ++n_outside;
    }
    arr->SetTuple1(i, static_cast<float>(v));
  }

  auto out = vtkSmartPointer<vtkPolyData>::New();
  out->ShallowCopy(mesh);
  out->GetPointData()->AddArray(arr);

  if(out_stats) out_stats->n_outside = n_outside;
  return out;
}

vtkSmartPointer<vtkPolyData>
SampleImageAtMesh(vtkPolyData *mesh, itk::ImageBase<3> *image,
                  const SampleParams &p, SampleStats *out_stats,
                  ProgressFn progress, AbortToken abort)
{
  if(!image) throw AlgorithmError("SampleImageAtMesh: null image");
  return WithPixelType(image, [&](auto *typed) {
    return SampleImageAtMesh(mesh, typed, p, out_stats, progress, abort);
  });
}

template vtkSmartPointer<vtkPolyData> SampleImageAtMesh(vtkPolyData*, itk::Image<unsigned char,  3>*, const SampleParams&, SampleStats*, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> SampleImageAtMesh(vtkPolyData*, itk::Image<char,           3>*, const SampleParams&, SampleStats*, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> SampleImageAtMesh(vtkPolyData*, itk::Image<short,          3>*, const SampleParams&, SampleStats*, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> SampleImageAtMesh(vtkPolyData*, itk::Image<unsigned short, 3>*, const SampleParams&, SampleStats*, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> SampleImageAtMesh(vtkPolyData*, itk::Image<int,            3>*, const SampleParams&, SampleStats*, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> SampleImageAtMesh(vtkPolyData*, itk::Image<unsigned int,   3>*, const SampleParams&, SampleStats*, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> SampleImageAtMesh(vtkPolyData*, itk::Image<float,          3>*, const SampleParams&, SampleStats*, ProgressFn, AbortToken);
template vtkSmartPointer<vtkPolyData> SampleImageAtMesh(vtkPolyData*, itk::Image<double,         3>*, const SampleParams&, SampleStats*, ProgressFn, AbortToken);

} // namespace cmesh
