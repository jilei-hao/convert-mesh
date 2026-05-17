#include "core/RasterizeMesh.h"

#include "core/Error.h"

#include <itkImage.h>
#include <itkImageRegionIterator.h>

#include <vtkImageData.h>
#include <vtkImageStencil.h>
#include <vtkMatrix4x4.h>
#include <vtkNew.h>
#include <vtkPolyDataToImageStencil.h>
#include <vtkTransform.h>
#include <vtkTransformPolyDataFilter.h>
#include <vtkTriangleFilter.h>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace cmesh
{

namespace
{
template <class TImage>
void CopyVTKImageToITK(vtkImageData *src,
                       TImage *dst,
                       typename TImage::PixelType inside,
                       typename TImage::PixelType outside)
{
  const unsigned char *buf = static_cast<const unsigned char *>(src->GetScalarPointer());
  itk::ImageRegionIterator<TImage> it(dst, dst->GetBufferedRegion());
  size_t i = 0;
  for(; !it.IsAtEnd(); ++it, ++i)
    it.Set(buf[i] > 0 ? inside : outside);
}
} // namespace


template <class TOut>
itk::SmartPointer<itk::Image<TOut, 3>>
RasterizeMesh(vtkPolyData *mesh,
              const RasterizeParams<TOut> &p,
              ProgressFn /*progress*/,
              AbortToken abort)
{
  using ImageType = itk::Image<TOut, 3>;

  if(!mesh)
    throw AlgorithmError("RasterizeMesh: null input mesh");
  abort.ThrowIfRequested();

  double spacing[3] = { p.spacing[0], p.spacing[1], p.spacing[2] };
  double origin[3]  = { 0.0, 0.0, 0.0 };
  int    extent[6]  = { 0, 0, 0, 0, 0, 0 };
  typename ImageType::DirectionType direction;
  direction.SetIdentity();

  if(p.reference)
  {
    auto refSpacing = p.reference->GetSpacing();
    auto refOrigin  = p.reference->GetOrigin();
    auto refRegion  = p.reference->GetLargestPossibleRegion();
    auto refSize    = refRegion.GetSize();
    const auto &refDir = p.reference->GetDirection();
    for(unsigned int i = 0; i < 3; ++i)
    {
      spacing[i] = refSpacing[i];
      origin[i]  = refOrigin[i];
      extent[2*i]     = 0;
      extent[2*i + 1] = static_cast<int>(refSize[i]) - 1;
      for(unsigned int j = 0; j < 3; ++j)
        direction(i, j) = refDir(i, j);
    }
    for(unsigned int r = 0; r < 3; ++r)
      for(unsigned int col = 0; col < 3; ++col)
      {
        double expect = (r == col ? 1.0 : 0.0);
        if(std::fabs(direction(r, col) - expect) > 1e-6)
          throw AlgorithmError(
            "RasterizeMesh: reference images with non-identity direction "
            "cosines are not supported yet");
      }
  }
  else
  {
    vtkNew<vtkMatrix4x4> ras_to_lps;
    ras_to_lps->Identity();
    ras_to_lps->SetElement(0, 0, -1.0);
    ras_to_lps->SetElement(1, 1, -1.0);
    vtkNew<vtkTransform> xf;
    xf->SetMatrix(ras_to_lps);
    vtkNew<vtkTransformPolyDataFilter> tmp;
    tmp->SetInputData(mesh);
    tmp->SetTransform(xf);
    tmp->Update();

    double b[6];
    tmp->GetOutput()->GetBounds(b);
    for(unsigned int i = 0; i < 3; ++i)
    {
      double lo = b[2*i]   - p.margin;
      double hi = b[2*i+1] + p.margin;
      origin[i] = lo;
      int n = static_cast<int>(std::ceil((hi - lo) / spacing[i]));
      extent[2*i]     = 0;
      extent[2*i + 1] = std::max(1, n) - 1;
    }
  }

  vtkNew<vtkMatrix4x4> ras_to_lps;
  ras_to_lps->Identity();
  ras_to_lps->SetElement(0, 0, -1.0);
  ras_to_lps->SetElement(1, 1, -1.0);
  vtkNew<vtkTransform> ras2lps;
  ras2lps->SetMatrix(ras_to_lps);

  vtkNew<vtkTransformPolyDataFilter> toLps;
  toLps->SetInputData(mesh);
  toLps->SetTransform(ras2lps);
  toLps->Update();

  vtkNew<vtkTriangleFilter> tri;
  tri->SetInputConnection(toLps->GetOutputPort());
  tri->PassLinesOff();
  tri->PassVertsOff();
  tri->Update();

  vtkNew<vtkPolyDataToImageStencil> stencil;
  stencil->SetInputConnection(tri->GetOutputPort());
  stencil->SetOutputOrigin(origin);
  stencil->SetOutputSpacing(spacing);
  stencil->SetOutputWholeExtent(extent);
  stencil->Update();

  vtkNew<vtkImageData> raster;
  raster->SetOrigin(origin);
  raster->SetSpacing(spacing);
  raster->SetExtent(extent);
  raster->AllocateScalars(VTK_UNSIGNED_CHAR, 1);
  std::memset(raster->GetScalarPointer(), 0,
              sizeof(unsigned char) * raster->GetNumberOfPoints());

  vtkNew<vtkImageStencil> apply;
  apply->SetInputData(raster);
  apply->SetStencilConnection(stencil->GetOutputPort());
  apply->ReverseStencilOn();
  apply->SetBackgroundValue(255.0);
  apply->Update();

  abort.ThrowIfRequested();

  typename ImageType::Pointer out = ImageType::New();
  typename ImageType::SizeType sz;
  typename ImageType::IndexType ix;
  for(unsigned int i = 0; i < 3; ++i)
  {
    ix[i] = 0;
    sz[i] = extent[2*i + 1] - extent[2*i] + 1;
  }
  out->SetRegions(typename ImageType::RegionType(ix, sz));
  typename ImageType::SpacingType itkSpacing;
  typename ImageType::PointType   itkOrigin;
  for(unsigned int i = 0; i < 3; ++i)
  {
    itkSpacing[i] = spacing[i];
    itkOrigin[i]  = origin[i];
  }
  out->SetSpacing(itkSpacing);
  out->SetOrigin(itkOrigin);
  out->SetDirection(direction);
  out->Allocate();
  out->FillBuffer(p.outside_value);

  CopyVTKImageToITK<ImageType>(apply->GetOutput(), out,
                               p.inside_value, p.outside_value);
  return out;
}

// Explicit instantiations — match PixelDispatch.h's closed list.
template itk::SmartPointer<itk::Image<unsigned char,  3>> RasterizeMesh(vtkPolyData*, const RasterizeParams<unsigned char>&,  ProgressFn, AbortToken);
template itk::SmartPointer<itk::Image<char,           3>> RasterizeMesh(vtkPolyData*, const RasterizeParams<char>&,           ProgressFn, AbortToken);
template itk::SmartPointer<itk::Image<short,          3>> RasterizeMesh(vtkPolyData*, const RasterizeParams<short>&,          ProgressFn, AbortToken);
template itk::SmartPointer<itk::Image<unsigned short, 3>> RasterizeMesh(vtkPolyData*, const RasterizeParams<unsigned short>&, ProgressFn, AbortToken);
template itk::SmartPointer<itk::Image<int,            3>> RasterizeMesh(vtkPolyData*, const RasterizeParams<int>&,            ProgressFn, AbortToken);
template itk::SmartPointer<itk::Image<unsigned int,   3>> RasterizeMesh(vtkPolyData*, const RasterizeParams<unsigned int>&,   ProgressFn, AbortToken);
template itk::SmartPointer<itk::Image<float,          3>> RasterizeMesh(vtkPolyData*, const RasterizeParams<float>&,          ProgressFn, AbortToken);
template itk::SmartPointer<itk::Image<double,         3>> RasterizeMesh(vtkPolyData*, const RasterizeParams<double>&,         ProgressFn, AbortToken);

} // namespace cmesh
