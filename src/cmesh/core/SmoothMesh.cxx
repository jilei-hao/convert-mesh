#include "cmesh/core/SmoothMesh.h"

#include "cmesh/core/Error.h"

#include <vtkNew.h>
#include <vtkSmoothPolyDataFilter.h>

namespace cmesh
{

vtkSmartPointer<vtkPolyData>
SmoothMesh(vtkPolyData *in, const SmoothParams &p,
           ProgressFn /*progress*/, AbortToken abort)
{
  if(!in)
    throw AlgorithmError("SmoothMesh: null input mesh");
  abort.ThrowIfRequested();

  vtkNew<vtkSmoothPolyDataFilter> smooth;
  smooth->SetInputData(in);
  smooth->SetNumberOfIterations(p.iterations);
  smooth->SetRelaxationFactor(p.relaxation_factor);
  smooth->SetFeatureAngle(p.feature_angle);
  smooth->SetBoundarySmoothing(p.boundary_smoothing);
  smooth->SetFeatureEdgeSmoothing(p.feature_edge_smoothing);
  smooth->Update();

  abort.ThrowIfRequested();
  return smooth->GetOutput();
}

} // namespace cmesh
