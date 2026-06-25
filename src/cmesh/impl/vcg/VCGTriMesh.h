// VCGTriMesh — minimal VCG <-> VTK bridge used by VCG-backed adapters.
// Ported verbatim from cmrep/src/util/VCGTriMesh.h so the resulting outputs
// match cmrep's mesh_decimate_vcg / mesh_smooth_curv tools bit-for-bit.

#ifndef CONVERTMESH_VCG_TRIMESH_H
#define CONVERTMESH_VCG_TRIMESH_H

#include <vcg/complex/complex.h>
#include <vcg/complex/algorithms/local_optimization/tri_edge_collapse_quadric.h>

class vtkPolyData;

namespace cmesh_vcg
{

class VCGTriMesh
{
public:
  class Face;
  class Edge;
  class Vertex;
  struct UsedTypes : public vcg::UsedTypes<vcg::Use<Vertex>::AsVertexType,
                                           vcg::Use<Edge>::AsEdgeType,
                                           vcg::Use<Face>::AsFaceType>{};
  class Vertex : public vcg::Vertex<UsedTypes,
                                    vcg::vertex::VFAdj, vcg::vertex::Coord3f,
                                    vcg::vertex::Mark, vcg::vertex::Qualityf,
                                    vcg::vertex::Normal3f, vcg::vertex::BitFlags>
  {
  public:
    vcg::math::Quadric<double> &Qd() { return q; }
  private:
    vcg::math::Quadric<double> q;
  };

  class Edge : public vcg::Edge<UsedTypes> {};
  class Face : public vcg::Face<UsedTypes,
                                vcg::face::VFAdj, vcg::face::Normal3f,
                                vcg::face::VertexRef, vcg::face::BitFlags> {};
  class Mesh : public vcg::tri::TriMesh<std::vector<Vertex>,
                                        std::vector<Face>> {};

  void ImportFromVTK(vtkPolyData *pd);
  void ExportToVTK(vtkPolyData *pd);
  void CleanMesh();
  void RecomputeNormals();

  static vcg::tri::TriEdgeCollapseQuadricParameter
  GetDefaultQuadricEdgeCollapseRemeshingParameters();

  void QuadricEdgeCollapseRemeshing(
      double reduction_factor = 0.1,
      vcg::tri::TriEdgeCollapseQuadricParameter params =
          GetDefaultQuadricEdgeCollapseRemeshingParameters());

  Mesh &GetMesh() { return m_Mesh; }

protected:
  Mesh m_Mesh;
};

} // namespace cmesh_vcg

#endif
