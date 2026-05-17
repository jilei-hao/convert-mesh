#ifndef CONVERTMESH_CORE_SAMPLE_IMAGE_AT_MESH_H
#define CONVERTMESH_CORE_SAMPLE_IMAGE_AT_MESH_H

#include "core/Backend.h"
#include "core/Progress.h"

#include <itkImage.h>
#include <itkImageBase.h>

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <string>

namespace cmesh
{

struct SampleParams
{
  std::string   array_name       = "Sample";
  Interpolation interpolation    = Interpolation::Linear;
  double        background_value = 0.0;
};

struct SampleStats
{
  vtkIdType n_outside = 0;
};

/**
 * Sample an image at the vertex positions of a mesh and store the result
 * as a named point-data array. Returns a shallow copy of `mesh` with the
 * array attached; the input is not modified.
 *
 * Meshes are assumed to live in NIFTI RAS space; ITK images in LPS.
 */
template <class T>
vtkSmartPointer<vtkPolyData>
SampleImageAtMesh(vtkPolyData *mesh,
                  itk::Image<T, 3> *image,
                  const SampleParams &p,
                  SampleStats *out_stats = nullptr,
                  ProgressFn progress = {},
                  AbortToken abort = {});

vtkSmartPointer<vtkPolyData>
SampleImageAtMesh(vtkPolyData *mesh,
                  itk::ImageBase<3> *image,
                  const SampleParams &p,
                  SampleStats *out_stats = nullptr,
                  ProgressFn progress = {},
                  AbortToken abort = {});

} // namespace cmesh

#endif
