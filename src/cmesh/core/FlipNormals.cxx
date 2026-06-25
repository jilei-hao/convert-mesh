#include "cmesh/core/FlipNormals.h"

#include "cmesh/core/Error.h"
#include "cmesh/core/FlipPolyFacesFilter.h"

#include <vtkSmartPointer.h>

namespace cmesh
{

vtkSmartPointer<vtkPolyData>
FlipNormals(vtkPolyData *in, ProgressFn /*progress*/, AbortToken abort)
{
  if(!in)
    throw AlgorithmError("FlipNormals: null input mesh");
  abort.ThrowIfRequested();

  auto flip = vtkSmartPointer<vtkFlipPolyFaces>::New();
  flip->SetInputData(in);
  flip->FlipFacesOn();
  flip->Update();

  abort.ThrowIfRequested();
  return flip->GetOutput();
}

} // namespace cmesh
