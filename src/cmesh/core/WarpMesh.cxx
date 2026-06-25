#include "cmesh/core/WarpMesh.h"

#include "cmesh/core/Error.h"
#include "cmesh/core/VTKToITKBridge.h"

#include <itkVectorLinearInterpolateImageFunction.h>

#include <vtkPoints.h>
#include <vtkSmartPointer.h>

namespace cmesh
{

vtkSmartPointer<vtkPolyData>
WarpMesh(vtkPolyData *mesh, WarpField *warp,
         const WarpMeshParams &p, WarpMeshStats *out_stats,
         ProgressFn /*progress*/, AbortToken abort)
{
  if(!mesh) throw AlgorithmError("WarpMesh: null input mesh");
  if(!warp) throw AlgorithmError("WarpMesh: null warp field");
  abort.ThrowIfRequested();

  using VectorType = itk::Vector<float, 3>;
  using InterpType = itk::VectorLinearInterpolateImageFunction<WarpField>;
  typename InterpType::Pointer interp = InterpType::New();
  interp->SetInputImage(warp);

  auto pts = vtkSmartPointer<vtkPoints>::New();
  pts->DeepCopy(mesh->GetPoints());

  vtkIdType outside = 0;
  for(vtkIdType i = 0; i < pts->GetNumberOfPoints(); ++i)
  {
    if((i & 0x3fff) == 0) abort.ThrowIfRequested();

    double p_ras[3];
    pts->GetPoint(i, p_ras);
    typename WarpField::PointType P_lps;
    bridge::RasToLpsPoint<typename WarpField::PointType>(p_ras, P_lps);

    if(interp->IsInsideBuffer(P_lps))
    {
      VectorType d_lps = interp->Evaluate(P_lps);
      double d_ras[3];
      bridge::LpsToRasVector(d_lps, d_ras);
      double p_out[3] = { p_ras[0] + d_ras[0],
                          p_ras[1] + d_ras[1],
                          p_ras[2] + d_ras[2] };
      pts->SetPoint(i, p_out);
    }
    else
    {
      ++outside;
      if(!p.ignore_outside)
        throw AlgorithmError("WarpMesh: vertex outside warp field extent");
    }
  }

  auto warped = vtkSmartPointer<vtkPolyData>::New();
  warped->DeepCopy(mesh);
  warped->SetPoints(pts);

  if(out_stats) out_stats->n_outside = outside;
  return warped;
}

} // namespace cmesh
