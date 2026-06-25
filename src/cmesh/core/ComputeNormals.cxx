#include "cmesh/core/ComputeNormals.h"

#include "cmesh/core/Error.h"

#include <vtkNew.h>
#include <vtkPolyDataNormals.h>

namespace cmesh
{

vtkSmartPointer<vtkPolyData>
ComputeNormals(vtkPolyData *in, const NormalsParams &p,
               ProgressFn /*progress*/, AbortToken abort)
{
  if(!in)
    throw AlgorithmError("ComputeNormals: null input mesh");
  abort.ThrowIfRequested();

  vtkNew<vtkPolyDataNormals> normals;
  normals->SetInputData(in);
  normals->SetFeatureAngle(p.feature_angle);
  normals->SetSplitting(p.splitting);
  normals->SetConsistency(p.consistency);
  normals->SetAutoOrientNormals(p.auto_orient);
  normals->Update();

  abort.ThrowIfRequested();
  return normals->GetOutput();
}

} // namespace cmesh
