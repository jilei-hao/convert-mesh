#include "cmesh/core/MeshDiff.h"

#include "cmesh/core/Error.h"

#include <vtkCellLocator.h>
#include <vtkFloatArray.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkTriangleFilter.h>

#include <cmath>

namespace cmesh
{

vtkSmartPointer<vtkPolyData>
MeshDiff(vtkPolyData *source, vtkPolyData *reference,
         const MeshDiffParams &p, MeshDiffStats *out_stats,
         ProgressFn /*progress*/, AbortToken abort)
{
  if(!source)    throw AlgorithmError("MeshDiff: null source mesh");
  if(!reference) throw AlgorithmError("MeshDiff: null reference mesh");
  abort.ThrowIfRequested();

  vtkNew<vtkTriangleFilter> tri_ref;
  tri_ref->SetInputData(reference);
  tri_ref->PassLinesOff();
  tri_ref->PassVertsOff();
  tri_ref->Update();

  vtkNew<vtkCellLocator> locator;
  locator->SetDataSet(tri_ref->GetOutput());
  locator->BuildLocator();

  vtkIdType n = source->GetNumberOfPoints();
  vtkNew<vtkFloatArray> dist;
  dist->SetName(p.array_name.c_str());
  dist->SetNumberOfComponents(1);
  dist->SetNumberOfTuples(n);

  double sum = 0.0, sum_sq = 0.0, dmax = 0.0;
  for(vtkIdType i = 0; i < n; ++i)
  {
    if((i & 0x3fff) == 0) abort.ThrowIfRequested();

    double src_pt[3];
    source->GetPoint(i, src_pt);
    double closest[3];
    vtkIdType cell_id;
    int sub_id;
    double d2 = 0.0;
    locator->FindClosestPoint(src_pt, closest, cell_id, sub_id, d2);
    double d = std::sqrt(d2);
    dist->SetTuple1(i, static_cast<float>(d));
    sum    += d;
    sum_sq += d * d;
    if(d > dmax) dmax = d;
  }

  auto out = vtkSmartPointer<vtkPolyData>::New();
  out->ShallowCopy(source);
  out->GetPointData()->AddArray(dist);

  if(out_stats)
  {
    out_stats->n_points  = static_cast<std::size_t>(n);
    out_stats->mean      = (n > 0) ? sum / n : 0.0;
    out_stats->rms       = (n > 0) ? std::sqrt(sum_sq / n) : 0.0;
    out_stats->hausdorff = dmax;
  }
  return out;
}

} // namespace cmesh
