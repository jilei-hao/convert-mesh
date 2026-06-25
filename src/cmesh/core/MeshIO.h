#ifndef CONVERTMESH_CORE_MESH_IO_H
#define CONVERTMESH_CORE_MESH_IO_H

#include <vtkPolyData.h>
#include <vtkSmartPointer.h>
#include <vtkUnstructuredGrid.h>

#include <string>

/**
 * Format-agnostic mesh I/O. Supports VTK (.vtk), VTP (.vtp), STL (.stl),
 * OBJ (.obj), PLY (.ply), and BYU (.byu/.y).
 *
 * All functions throw cmesh::IOError on failure.
 */
namespace cmesh
{

// Lowercase extension (no leading dot). Empty if the filename has none.
std::string GetExtension(const std::string &filename);

vtkSmartPointer<vtkPolyData>         ReadPolyData(const std::string &filename);
vtkSmartPointer<vtkUnstructuredGrid> ReadUnstructuredGrid(const std::string &filename);

void WritePolyData(vtkPolyData *mesh,
                   const std::string &filename,
                   bool binary = true);

void WriteUnstructuredGrid(vtkUnstructuredGrid *mesh,
                           const std::string &filename,
                           bool binary = true);

} // namespace cmesh

#endif
