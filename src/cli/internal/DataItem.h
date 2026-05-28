#ifndef CONVERTMESH_CLI_DATA_ITEM_H
#define CONVERTMESH_CLI_DATA_ITEM_H

#include <itkImageBase.h>
#include <itkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

namespace cmesh
{
namespace cli
{

class DataItem
{
public:
  enum Kind { NONE = 0, MESH, UGRID, IMAGE };

  using MeshPtr   = vtkSmartPointer<vtkPolyData>;
  using UGridPtr  = vtkSmartPointer<vtkUnstructuredGrid>;
  using ImagePtr  = itk::SmartPointer<itk::ImageBase<3>>;

  DataItem() = default;

  static DataItem FromMesh(vtkPolyData *m)
  {
    DataItem d; d.kind = MESH; d.mesh = m; return d;
  }
  static DataItem FromUGrid(vtkUnstructuredGrid *g)
  {
    DataItem d; d.kind = UGRID; d.ugrid = g; return d;
  }
  static DataItem FromImage(itk::ImageBase<3> *img)
  {
    DataItem d; d.kind = IMAGE; d.image = img; return d;
  }

  bool IsMesh()  const { return kind == MESH; }
  bool IsUGrid() const { return kind == UGRID; }
  bool IsImage() const { return kind == IMAGE; }
  bool IsEmpty() const { return kind == NONE; }

  const char *KindName() const
  {
    switch(kind) {
    case MESH:  return "mesh";
    case UGRID: return "ugrid";
    case IMAGE: return "image";
    default:    return "none";
    }
  }

  Kind     kind = NONE;
  MeshPtr  mesh;
  UGridPtr ugrid;
  ImagePtr image;
};

} // namespace cli
} // namespace cmesh

#endif
