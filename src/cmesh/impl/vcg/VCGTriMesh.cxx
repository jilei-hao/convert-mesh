// VCGTriMesh — ported verbatim from cmrep/src/util/VCGTriMesh.cxx.
// Algorithmic flow (Import / Clean / QuadricEdgeCollapseRemeshing / Recompute
// Normals / Export) is identical so the parity test against cmrep's
// mesh_decimate_vcg holds tightly.
//
// The only deliberate change is wrapping the implementation in the
// cmesh_vcg namespace (cmrep keeps its class at file scope).

#include "cmesh/impl/vcg/VCGTriMesh.h"

#include <vtkPolyData.h>
#include <vtkCellArray.h>
#include <vtkCellArrayIterator.h>
#include <vtkPointData.h>
#include <vtkFloatArray.h>
#include <vtkNew.h>
#include <vtkPoints.h>

#include <vcg/complex/algorithms/clean.h>

#include <ctime>
#include <cstdio>
#include <iostream>
#include <limits>
#include <vector>

namespace cmesh_vcg
{

using namespace vcg;
using std::vector;

void VCGTriMesh::ImportFromVTK(vtkPolyData *pd)
{
  int np = pd->GetNumberOfPoints();

  m_Mesh.Clear();

  Mesh::VertexIterator vi =
      vcg::tri::Allocator<Mesh>::AddVertices(m_Mesh, np);
  Mesh::FaceIterator fi =
      vcg::tri::Allocator<Mesh>::AddFaces(m_Mesh, pd->GetPolys()->GetNumberOfCells());

  vector<Mesh::VertexPointer> ivp(np);
  for(unsigned int i = 0; i < np; i++)
  {
    ivp[i] = &*vi;
    auto *p = pd->GetPoint(i);
    vi->P() = Mesh::CoordType(p[0], p[1], p[2]);
    ++vi;
  }

  int bad_faces = 0;
  auto iter = vtk::TakeSmartPointer(pd->GetPolys()->NewIterator());
  for(iter->GoToFirstCell(); !iter->IsDoneWithTraversal(); iter->GoToNextCell())
  {
    const vtkIdType *ids;
    vtkIdType npts;
    iter->GetCurrentCell(npts, ids);
    if(npts != 3)
    {
      ++bad_faces;
      continue;
    }
    fi->V(0) = ivp[ids[0]];
    fi->V(1) = ivp[ids[1]];
    fi->V(2) = ivp[ids[2]];
    fi++;
  }

  if(bad_faces > 0)
    std::cerr << "VCGTriMesh: skipped " << bad_faces
              << " non-triangular faces during VTK import" << std::endl;
}

void VCGTriMesh::ExportToVTK(vtkPolyData *pd)
{
  vtkNew<vtkPoints> pts;
  vtkNew<vtkFloatArray> normals;
  normals->SetNumberOfComponents(3);

  std::vector<int> VertexId(m_Mesh.vert.size());
  int numvert = 0;
  for(auto vi = m_Mesh.vert.begin(); vi != m_Mesh.vert.end(); ++vi)
  {
    if(!(*vi).IsD())
    {
      const auto &p = vi->cP();
      const auto &n = vi->cN();
      pts->InsertNextPoint(p[0], p[1], p[2]);
      normals->InsertNextTuple3(n[0], n[1], n[2]);
      VertexId[vi - m_Mesh.vert.begin()] = numvert++;
    }
  }

  pd->SetPoints(pts);
  pd->GetPointData()->SetNormals(normals);

  vtkNew<vtkCellArray> faces;
  for(auto fi = m_Mesh.face.begin(); fi != m_Mesh.face.end(); ++fi)
  {
    if(!(*fi).IsD())
    {
      faces->InsertNextCell(fi->VN());
      for(int k = 0; k < fi->VN(); k++)
        faces->InsertCellPoint(VertexId[tri::Index(m_Mesh, fi->V(k))]);
    }
  }

  pd->SetPolys(faces);
}

void VCGTriMesh::CleanMesh()
{
  tri::Clean<Mesh>::RemoveDuplicateVertex(m_Mesh);
  tri::Clean<Mesh>::RemoveUnreferencedVertex(m_Mesh);
  tri::Clean<Mesh>::RemoveDuplicateFace(m_Mesh);
  tri::Clean<Mesh>::RemoveDegenerateFace(m_Mesh);
  tri::Clean<Mesh>::RemoveZeroAreaFace(m_Mesh);

  tri::Allocator<Mesh>::CompactEveryVector(m_Mesh);
  tri::UpdateTopology<Mesh>::VertexFace(m_Mesh);
  tri::UpdateBounding<Mesh>::Box(m_Mesh);
  tri::UpdateFlags<Mesh>::FaceBorderFromVF(m_Mesh);
  tri::UpdateFlags<Mesh>::VertexBorderFromFaceBorder(m_Mesh);
}

void VCGTriMesh::RecomputeNormals()
{
  tri::UpdateBounding<Mesh>::Box(m_Mesh);
  if(m_Mesh.fn > 0)
  {
    tri::UpdateNormal<Mesh>::PerFaceNormalized(m_Mesh);
    tri::UpdateNormal<Mesh>::PerVertexAngleWeighted(m_Mesh);
  }
  tri::UpdateNormal<Mesh>::NormalizePerFace(m_Mesh);
  tri::UpdateNormal<Mesh>::PerVertexFromCurrentFaceNormal(m_Mesh);
  tri::UpdateNormal<Mesh>::NormalizePerVertex(m_Mesh);
}

tri::TriEdgeCollapseQuadricParameter
VCGTriMesh::GetDefaultQuadricEdgeCollapseRemeshingParameters()
{
  vcg::tri::TriEdgeCollapseQuadricParameter qparams;
  qparams.QualityThr = 0.3;
  qparams.PreserveBoundary = true;
  qparams.BoundaryQuadricWeight = 1.0;
  qparams.PreserveTopology = true;
  qparams.QualityWeight = false;
  qparams.NormalCheck = true;
  qparams.OptimalPlacement = true;
  qparams.QualityQuadric = true;
  qparams.QualityQuadricWeight = 0.001;
  return qparams;
}

void VCGTriMesh::QuadricEdgeCollapseRemeshing(
    double reduction_factor,
    vcg::tri::TriEdgeCollapseQuadricParameter qparams)
{
  vcg::LocalOptimization<VCGTriMesh::Mesh> DeciSession(m_Mesh, &qparams);

  typedef vcg::tri::BasicVertexPair<VCGTriMesh::Vertex> VertexPair;
  class MyTriEdgeCollapse : public vcg::tri::TriEdgeCollapseQuadric<
      VCGTriMesh::Mesh, VertexPair, MyTriEdgeCollapse,
      vcg::tri::QInfoStandard<VCGTriMesh::Vertex>>
  {
  public:
    typedef vcg::tri::TriEdgeCollapseQuadric<
        VCGTriMesh::Mesh, VertexPair, MyTriEdgeCollapse,
        vcg::tri::QInfoStandard<VCGTriMesh::Vertex>> TECQ;
    typedef VCGTriMesh::Mesh::VertexType::EdgeType EdgeType;
    inline MyTriEdgeCollapse(const VertexPair &p, int i,
                             vcg::BaseParameterClass *pp)
      : TECQ(p, i, pp) {}
  };

  int n_target = reduction_factor < 1.0
                   ? (int)(m_Mesh.FN() * reduction_factor)
                   : (int) reduction_factor;

  DeciSession.Init<MyTriEdgeCollapse>();
  DeciSession.SetTargetSimplices(n_target);
  DeciSession.SetTimeBudget(2.0f);

  double TargetError = std::numeric_limits<double>::max();
  while(DeciSession.DoOptimization() &&
        m_Mesh.FN() > n_target &&
        DeciSession.currMetric < TargetError)
  {
    // Loop body intentionally empty — cmrep prints progress here; we stay
    // quiet by default and let the calling adapter route a verbose summary.
  }

  DeciSession.Finalize<MyTriEdgeCollapse>();
}

} // namespace cmesh_vcg
