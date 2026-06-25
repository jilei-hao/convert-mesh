#include "cmesh/core/MeshIO.h"

#include "cmesh/core/Error.h"

#include <vtkBYUReader.h>
#include <vtkBYUWriter.h>
#include <vtkNew.h>
#include <vtkOBJReader.h>
#include <vtkOBJWriter.h>
#include <vtkPLYReader.h>
#include <vtkPLYWriter.h>
#include <vtkPolyDataReader.h>
#include <vtkPolyDataWriter.h>
#include <vtkSTLReader.h>
#include <vtkSTLWriter.h>
#include <vtkTriangleFilter.h>
#include <vtkUnstructuredGridReader.h>
#include <vtkUnstructuredGridWriter.h>
#include <vtkXMLPolyDataReader.h>
#include <vtkXMLPolyDataWriter.h>
#include <vtkXMLUnstructuredGridReader.h>
#include <vtkXMLUnstructuredGridWriter.h>

#include <algorithm>
#include <cctype>

namespace cmesh
{

namespace
{
std::string ToLower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}
} // namespace

std::string GetExtension(const std::string &filename)
{
  auto pos = filename.find_last_of('.');
  if(pos == std::string::npos) return "";
  return ToLower(filename.substr(pos + 1));
}

vtkSmartPointer<vtkPolyData> ReadPolyData(const std::string &filename)
{
  std::string ext = GetExtension(filename);
  vtkSmartPointer<vtkPolyData> out;

  if(ext == "vtk")
  {
    vtkNew<vtkPolyDataReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();
    out = reader->GetOutput();
  }
  else if(ext == "vtp")
  {
    vtkNew<vtkXMLPolyDataReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();
    out = reader->GetOutput();
  }
  else if(ext == "stl")
  {
    vtkNew<vtkSTLReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();
    out = reader->GetOutput();
  }
  else if(ext == "obj")
  {
    vtkNew<vtkOBJReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();
    out = reader->GetOutput();
  }
  else if(ext == "ply")
  {
    vtkNew<vtkPLYReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();
    out = reader->GetOutput();
  }
  else if(ext == "byu" || ext == "y")
  {
    vtkNew<vtkBYUReader> reader;
    reader->SetGeometryFileName(filename.c_str());
    reader->Update();
    out = reader->GetOutput();
  }
  else
  {
    throw IOError("unsupported mesh format: ." + ext + " (file: " + filename + ")");
  }

  if(!out || out->GetNumberOfPoints() == 0)
    throw IOError("mesh read returned empty polydata: " + filename);
  return out;
}

vtkSmartPointer<vtkUnstructuredGrid> ReadUnstructuredGrid(const std::string &filename)
{
  std::string ext = GetExtension(filename);
  vtkSmartPointer<vtkUnstructuredGrid> out;

  if(ext == "vtk")
  {
    vtkNew<vtkUnstructuredGridReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();
    out = reader->GetOutput();
  }
  else if(ext == "vtu")
  {
    vtkNew<vtkXMLUnstructuredGridReader> reader;
    reader->SetFileName(filename.c_str());
    reader->Update();
    out = reader->GetOutput();
  }
  else
  {
    throw IOError("unsupported ugrid format: ." + ext + " (file: " + filename + ")");
  }

  if(!out)
    throw IOError("ugrid read returned null: " + filename);
  return out;
}

void WritePolyData(vtkPolyData *mesh, const std::string &filename, bool binary)
{
  if(!mesh)
    throw IOError("WritePolyData: null input mesh");

  std::string ext = GetExtension(filename);

  if(ext == "vtk")
  {
    vtkNew<vtkPolyDataWriter> writer;
    writer->SetFileName(filename.c_str());
    writer->SetInputData(mesh);
    if(binary) writer->SetFileTypeToBinary();
    writer->Update();
  }
  else if(ext == "vtp")
  {
    vtkNew<vtkXMLPolyDataWriter> writer;
    writer->SetFileName(filename.c_str());
    writer->SetInputData(mesh);
    writer->Update();
  }
  else if(ext == "stl")
  {
    vtkNew<vtkTriangleFilter> tri;
    tri->SetInputData(mesh);
    tri->PassVertsOff();
    tri->PassLinesOff();
    tri->Update();

    vtkNew<vtkSTLWriter> writer;
    writer->SetFileName(filename.c_str());
    writer->SetInputData(tri->GetOutput());
    if(binary) writer->SetFileTypeToBinary();
    writer->Update();
  }
  else if(ext == "obj")
  {
    vtkNew<vtkOBJWriter> writer;
    writer->SetFileName(filename.c_str());
    writer->SetInputData(mesh);
    writer->Update();
  }
  else if(ext == "ply")
  {
    vtkNew<vtkPLYWriter> writer;
    writer->SetFileName(filename.c_str());
    writer->SetInputData(mesh);
    if(binary) writer->SetFileTypeToBinary();
    writer->Update();
  }
  else if(ext == "byu" || ext == "y")
  {
    vtkNew<vtkTriangleFilter> tri;
    tri->SetInputData(mesh);
    tri->Update();

    vtkNew<vtkBYUWriter> writer;
    writer->SetGeometryFileName(filename.c_str());
    writer->SetInputData(tri->GetOutput());
    writer->Update();
  }
  else
  {
    throw IOError("unsupported mesh format: ." + ext + " (file: " + filename + ")");
  }
}

void WriteUnstructuredGrid(vtkUnstructuredGrid *mesh,
                           const std::string &filename,
                           bool binary)
{
  if(!mesh)
    throw IOError("WriteUnstructuredGrid: null input");

  std::string ext = GetExtension(filename);
  if(ext == "vtk")
  {
    vtkNew<vtkUnstructuredGridWriter> writer;
    writer->SetFileName(filename.c_str());
    writer->SetInputData(mesh);
    if(binary) writer->SetFileTypeToBinary();
    writer->Update();
  }
  else if(ext == "vtu")
  {
    vtkNew<vtkXMLUnstructuredGridWriter> writer;
    writer->SetFileName(filename.c_str());
    writer->SetInputData(mesh);
    writer->Update();
  }
  else
  {
    throw IOError("unsupported ugrid format: ." + ext + " (file: " + filename + ")");
  }
}

} // namespace cmesh
