#ifndef CONVERTMESH_CORE_BACKEND_H
#define CONVERTMESH_CORE_BACKEND_H

namespace cmesh
{

enum class Backend
{
  VTK,
  VCG,
  GPU
};

enum class Interpolation
{
  Linear,
  NearestNeighbor,
  BSpline
};

} // namespace cmesh

#endif
