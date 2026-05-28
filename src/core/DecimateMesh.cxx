#include "core/DecimateMesh.h"

#include "core/Error.h"

#include <vtkDecimatePro.h>
#include <vtkNew.h>
#include <vtkTriangleFilter.h>

#ifdef CONVERTMESH_HAVE_VCG
#  include "impl/vcg/VCGTriMesh.h"
#endif

namespace cmesh
{

namespace
{

vtkSmartPointer<vtkPolyData>
DecimateVTK(vtkPolyData *mesh, const DecimateParams &p)
{
  vtkNew<vtkTriangleFilter> tri;
  tri->SetInputData(mesh);
  tri->PassLinesOff();
  tri->PassVertsOff();
  tri->Update();

  vtkNew<vtkDecimatePro> deci;
  deci->SetInputConnection(tri->GetOutputPort());
  deci->SetTargetReduction(p.reduction);
  deci->SetFeatureAngle(p.feature_angle);
  if(p.preserve_topology) deci->PreserveTopologyOn();
  else                    deci->PreserveTopologyOff();
  deci->SetBoundaryVertexDeletion(p.boundary_deletion);
  deci->Update();

  auto out = vtkSmartPointer<vtkPolyData>::New();
  out->DeepCopy(deci->GetOutput());
  return out;
}

#ifdef CONVERTMESH_HAVE_VCG
vtkSmartPointer<vtkPolyData>
DecimateVCG(vtkPolyData *mesh, double reduction)
{
  cmesh_vcg::VCGTriMesh tri_mesh;
  tri_mesh.ImportFromVTK(mesh);
  tri_mesh.CleanMesh();

  vcg::tri::TriEdgeCollapseQuadricParameter qparams =
      cmesh_vcg::VCGTriMesh::GetDefaultQuadricEdgeCollapseRemeshingParameters();

  double keep_fraction = 1.0 - reduction;
  if(keep_fraction < 0.0) keep_fraction = 0.0;
  if(keep_fraction > 1.0) keep_fraction = 1.0;

  tri_mesh.QuadricEdgeCollapseRemeshing(keep_fraction, qparams);
  tri_mesh.CleanMesh();
  tri_mesh.RecomputeNormals();

  auto out = vtkSmartPointer<vtkPolyData>::New();
  tri_mesh.ExportToVTK(out);
  return out;
}
#endif

} // namespace


vtkSmartPointer<vtkPolyData>
DecimateMesh(vtkPolyData *in, const DecimateParams &p,
             ProgressFn /*progress*/, AbortToken abort)
{
  if(!in)
    throw AlgorithmError("DecimateMesh: null input mesh");
  abort.ThrowIfRequested();

#ifdef CONVERTMESH_HAVE_VCG
  if(p.backend == Backend::VCG)
  {
    auto out = DecimateVCG(in, p.reduction);
    abort.ThrowIfRequested();
    return out;
  }
#endif

  auto out = DecimateVTK(in, p);
  abort.ThrowIfRequested();
  return out;
}

} // namespace cmesh
