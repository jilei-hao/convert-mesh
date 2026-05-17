#include "cli/internal/Adapters.h"

#include "cli/internal/Driver.h"
#include "core/ComputeNormals.h"
#include "core/DecimateMesh.h"
#include "core/Error.h"
#include "core/ExtractIsoSurface.h"
#include "core/FlipNormals.h"
#include "core/ImageIO.h"
#include "core/MergeArrays.h"
#include "core/MeshDiff.h"
#include "core/MeshIO.h"
#include "core/RasterizeMesh.h"
#include "core/SampleImageAtMesh.h"
#include "core/SmoothMesh.h"
#include "core/WarpMesh.h"

#include <itkImageFileReader.h>

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDataSetAttributes.h>
#include <vtkPointData.h>

#include <cstdlib>
#include <cstring>

namespace cmesh
{
namespace cli
{

// ---------------------------------------------------------------------------
// I/O
// ---------------------------------------------------------------------------
void ReadMesh(Driver &d, const std::string &filename)
{
  auto mesh = cmesh::ReadPolyData(filename);
  d.Verbose() << "Reading mesh #" << (d.m_Stack.size() + 1) << " from "
              << filename << " (" << mesh->GetNumberOfPoints() << " points, "
              << mesh->GetNumberOfCells() << " cells)" << std::endl;
  d.m_Stack.PushMesh(mesh);
}

void ReadImage(Driver &d, const std::string &filename)
{
  auto img = cmesh::ReadImage(filename);
  auto size = img->GetLargestPossibleRegion().GetSize();
  d.Verbose() << "Reading image #" << (d.m_Stack.size() + 1) << " from "
              << filename << " (size " << size << ")" << std::endl;
  d.m_Stack.PushImage(img);
}

void WriteMesh(Driver &d, const std::string &filename)
{
  if(d.m_Stack.empty()) throw StackError();
  const auto &top = d.m_Stack.back();
  if(top.IsMesh())
  {
    d.Verbose() << "Writing mesh #" << d.m_Stack.size() << " to "
                << filename << std::endl;
    cmesh::WritePolyData(top.mesh, filename);
  }
  else if(top.IsUGrid())
  {
    d.Verbose() << "Writing ugrid #" << d.m_Stack.size() << " to "
                << filename << std::endl;
    cmesh::WriteUnstructuredGrid(top.ugrid, filename);
  }
  else
  {
    throw TypeError("mesh or ugrid", top.KindName());
  }
}

void WriteImage(Driver &d, const std::string &filename)
{
  if(d.m_Stack.empty()) throw StackError();
  const auto &top = d.m_Stack.back();
  if(!top.IsImage()) throw TypeError("image", top.KindName());
  d.Verbose() << "Writing image #" << d.m_Stack.size() << " to "
              << filename << std::endl;
  cmesh::WriteImage(top.image, filename);
}

void WriteTopAutoDetect(Driver &d, const std::string &filename)
{
  if(d.m_Stack.empty()) throw StackError();
  const auto &top = d.m_Stack.back();
  if(top.IsMesh() || top.IsUGrid()) WriteMesh(d, filename);
  else if(top.IsImage())            WriteImage(d, filename);
  else                              throw TypeError("stack top has no kind");
}

// ---------------------------------------------------------------------------
// Stack ops
// ---------------------------------------------------------------------------
void StackPop(Driver &d)   { d.m_Stack.pop(); }

void StackDup(Driver &d)
{
  if(d.m_Stack.empty()) throw StackError();
  DataItem copy = d.m_Stack.back();
  d.m_Stack.push(copy);
}

void StackSwap(Driver &d)
{
  if(d.m_Stack.size() < 2)
    throw StackError("-swap requires at least two items on the stack");
  DataItem a = d.m_Stack.back(); d.m_Stack.pop();
  DataItem b = d.m_Stack.back(); d.m_Stack.pop();
  d.m_Stack.push(a);
  d.m_Stack.push(b);
}

void StackClear(Driver &d) { d.m_Stack.clear(); }

void StackAs(Driver &d, const std::string &name)
{
  if(d.m_Stack.empty()) throw StackError();
  d.SetVariable(name, d.m_Stack.back());
}

void StackPopAs(Driver &d, const std::string &name)
{
  if(d.m_Stack.empty()) throw StackError();
  d.SetVariable(name, d.m_Stack.back());
  d.m_Stack.pop();
}

void StackPush(Driver &d, const std::string &name)
{
  d.m_Stack.push(d.GetVariable(name));
}

// ---------------------------------------------------------------------------
// Mesh ops
// ---------------------------------------------------------------------------
int CmdSmoothMesh(Driver &d, int argc, const char *const *argv)
{
  if(argc < 2) throw ParseError("-smooth-mesh requires iterations");
  cmesh::SmoothParams p;
  p.iterations = std::atoi(argv[1]);
  p.backend    = d.m_Backend;
  int consumed = 1;
  if(argc >= 3 && argv[2][0] != '-')
  {
    p.relaxation_factor = std::atof(argv[2]);
    consumed = 2;
  }
  auto out = cmesh::SmoothMesh(d.m_Stack.PopMesh(), p);
  d.Verbose() << "SmoothMesh: " << p.iterations << " iterations, "
              << "relax=" << p.relaxation_factor << std::endl;
  d.m_Stack.PushMesh(out);
  return consumed;
}

int CmdDecimateMesh(Driver &d, int argc, const char *const *argv)
{
  if(argc < 2) throw ParseError("-decimate requires a reduction factor");
  cmesh::DecimateParams p;
  p.reduction = std::atof(argv[1]);
  p.backend   = d.m_Backend;
  auto in = d.m_Stack.PopMesh();
  auto out = cmesh::DecimateMesh(in, p);
  d.Verbose() << "DecimateMesh: reduction=" << p.reduction
              << ", out points=" << out->GetNumberOfPoints()
              << ", out cells=" << out->GetNumberOfCells() << std::endl;
  d.m_Stack.PushMesh(out);
  return 1;
}

int CmdComputeNormals(Driver &d, int argc, const char *const *argv)
{
  cmesh::NormalsParams p;
  int consumed = 0;
  if(argc >= 2 && std::string(argv[1]) == "-auto-orient")
  {
    p.auto_orient = true;
    consumed = 1;
  }
  auto out = cmesh::ComputeNormals(d.m_Stack.PopMesh(), p);
  d.Verbose() << "ComputeNormals: feature_angle=" << p.feature_angle
              << (p.auto_orient ? ", auto-oriented" : "") << std::endl;
  d.m_Stack.PushMesh(out);
  return consumed;
}

int CmdFlipNormals(Driver &d)
{
  auto in  = d.m_Stack.PopMesh();
  auto out = cmesh::FlipNormals(in);
  d.Verbose() << "FlipNormals: reversed winding for "
              << in->GetNumberOfCells() << " cells" << std::endl;
  d.m_Stack.PushMesh(out);
  return 0;
}

int CmdMeshDiff(Driver &d, int argc, const char *const *argv)
{
  if(argc < 2) throw ParseError("-meshdiff requires a reference mesh filename");
  cmesh::MeshDiffParams p;
  std::string ref_path = argv[1];
  auto ref = cmesh::ReadPolyData(ref_path);
  auto source = d.m_Stack.PopMesh();
  cmesh::MeshDiffStats stats;
  auto annotated = cmesh::MeshDiff(source, ref, p, &stats);
  d.Out() << "MeshDiff: " << ref_path << std::endl
          << "  N=" << stats.n_points
          << "  mean=" << stats.mean
          << "  rms="  << stats.rms
          << "  hausdorff(source->ref)=" << stats.hausdorff << std::endl;
  d.m_Stack.PushMesh(annotated);
  return 1;
}

int CmdExtractIsoSurface(Driver &d, int argc, const char *const *argv)
{
  if(argc < 2) throw ParseError(std::string(argv[0]) + " requires a threshold value");
  cmesh::IsoSurfaceParams p;
  p.threshold = std::atof(argv[1]);
  p.backend   = d.m_Backend;
  int consumed = 1;
  while(consumed + 1 < argc)
  {
    std::string sub = argv[consumed + 1];
    if(sub == "-multi-label") { p.multi_label = true; consumed += 1; }
    else if(sub == "-clean")  { p.clean = true; consumed += 1; }
    else if(sub == "-smooth-pre")
    {
      if(consumed + 2 >= argc) throw ParseError("-smooth-pre needs a value");
      p.smooth_pre = std::atof(argv[consumed + 2]);
      consumed += 2;
    }
    else if(sub == "-decimate-post")
    {
      if(consumed + 2 >= argc) throw ParseError("-decimate-post needs a value");
      p.decimate = std::atof(argv[consumed + 2]);
      consumed += 2;
    }
    else break;
  }
  auto img = d.m_Stack.PopImage();
  auto mesh = cmesh::ExtractIsoSurface(img.GetPointer(), p);
  d.Verbose() << "ExtractIsoSurface: produced "
              << mesh->GetNumberOfPoints() << " points, "
              << mesh->GetNumberOfCells() << " cells" << std::endl;
  d.m_Stack.PushMesh(mesh);
  return consumed;
}

int CmdRasterizeMesh(Driver &d, int argc, const char *const *argv)
{
  // CLI rasterization defaults to float pixel type — matches prior CLI
  // behavior. Programmatic callers can pick any TOut directly via the
  // core API.
  cmesh::RasterizeParams<float> p;
  std::string ref_path;
  int consumed = 0;
  while(consumed + 1 < argc && argv[consumed + 1][0] == '-')
  {
    std::string sub = argv[consumed + 1];
    if(sub == "-ref")
    {
      if(consumed + 2 >= argc) throw ParseError("-ref needs a filename");
      ref_path = argv[consumed + 2];
      consumed += 2;
    }
    else if(sub == "-spacing")
    {
      if(consumed + 4 >= argc) throw ParseError("-spacing needs sx sy sz");
      p.spacing[0] = std::atof(argv[consumed + 2]);
      p.spacing[1] = std::atof(argv[consumed + 3]);
      p.spacing[2] = std::atof(argv[consumed + 4]);
      consumed += 4;
    }
    else if(sub == "-margin")
    {
      if(consumed + 2 >= argc) throw ParseError("-margin needs a value");
      p.margin = std::atof(argv[consumed + 2]);
      consumed += 2;
    }
    else if(sub == "-inside")
    {
      if(consumed + 2 >= argc) throw ParseError("-inside needs a value");
      p.inside_value = static_cast<float>(std::atof(argv[consumed + 2]));
      consumed += 2;
    }
    else break;
  }

  itk::SmartPointer<itk::ImageBase<3>> ref;
  if(!ref_path.empty())
  {
    ref = cmesh::ReadImage(ref_path);
    p.reference = ref.GetPointer();
  }

  auto mesh = d.m_Stack.PopMesh();
  auto out  = cmesh::RasterizeMesh<float>(mesh, p);
  auto sz   = out->GetBufferedRegion().GetSize();
  d.Verbose() << "RasterizeMesh: " << sz[0] << "x" << sz[1] << "x" << sz[2]
              << std::endl;
  d.m_Stack.PushImage(out.GetPointer());
  return consumed;
}

int CmdWarpMesh(Driver &d, int argc, const char *const *argv)
{
  if(argc < 2) throw ParseError("-warp-mesh requires a warp field filename");
  std::string warp_path = argv[1];

  using ReaderType = itk::ImageFileReader<cmesh::WarpField>;
  ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(warp_path);
  try { reader->Update(); }
  catch(itk::ExceptionObject &e)
  {
    throw cmesh::IOError("failed to read warp field '" + warp_path + "': "
                         + e.GetDescription());
  }

  cmesh::WarpMeshParams p;
  cmesh::WarpMeshStats stats;
  auto mesh = d.m_Stack.PopMesh();
  auto out  = cmesh::WarpMesh(mesh, reader->GetOutput(), p, &stats);
  d.Verbose() << "WarpMesh: " << warp_path
              << " (" << out->GetNumberOfPoints() << " points, "
              << stats.n_outside << " outside field)" << std::endl;
  if(stats.n_outside > 0)
    d.Err() << "warning: " << stats.n_outside
            << " mesh vertices fell outside the warp field extent "
            << "and were left unchanged." << std::endl;
  d.m_Stack.PushMesh(out);
  return 1;
}

int CmdSampleImageAtMesh(Driver &d, int argc, const char *const *argv)
{
  if(argc < 2) throw ParseError("-sample-image requires an array name");
  cmesh::SampleParams p;
  p.array_name    = argv[1];
  p.interpolation = d.m_Interpolation;

  // Stack: [ ..., mesh, image (top) ]
  auto image = d.m_Stack.PopImage();
  if(d.m_Stack.empty() || !d.m_Stack.back().IsMesh())
    throw TypeError("mesh",
                    d.m_Stack.empty() ? "(empty)"
                                      : d.m_Stack.back().KindName());
  auto mesh = d.m_Stack.back().mesh;
  d.m_Stack.pop();

  cmesh::SampleStats stats;
  auto out = cmesh::SampleImageAtMesh(mesh, image.GetPointer(), p, &stats);
  d.Verbose() << "SampleImageAtMesh: array='" << p.array_name
              << "' (" << out->GetNumberOfPoints() << " pts, "
              << stats.n_outside << " outside image)" << std::endl;
  d.m_Stack.PushMesh(out);
  return 1;
}

int CmdMergeArrays(Driver &d, int argc, const char *const *argv)
{
  if(argc < 3) throw ParseError(std::string(argv[0])
                                + " requires SOURCE_MESH and ARRAY_NAME");
  cmesh::MergeArraysParams p;
  std::string src_path = argv[1];
  p.array_name = argv[2];
  int consumed = 2;
  while(consumed + 1 < argc && argv[consumed + 1][0] == '-')
  {
    std::string sub = argv[consumed + 1];
    if(sub == "-cell") { p.cell_data = true; consumed += 1; }
    else if(sub == "-rename")
    {
      if(consumed + 2 >= argc) throw ParseError("-rename needs a name");
      p.rename_to = argv[consumed + 2];
      consumed += 2;
    }
    else break;
  }

  if(d.m_Stack.empty() || !d.m_Stack.back().IsMesh())
    throw TypeError("mesh",
                    d.m_Stack.empty() ? "(empty)"
                                      : d.m_Stack.back().KindName());
  auto dest = d.m_Stack.back().mesh;
  d.m_Stack.pop();

  auto source = cmesh::ReadPolyData(src_path);
  auto out = cmesh::MergeArrays(dest, source, p);

  vtkDataSetAttributes *out_attr = p.cell_data
      ? static_cast<vtkDataSetAttributes *>(out->GetCellData())
      : static_cast<vtkDataSetAttributes *>(out->GetPointData());
  auto *arr = out_attr->GetArray(p.rename_to.empty() ? p.array_name.c_str()
                                                     : p.rename_to.c_str());
  d.Verbose() << "MergeArrays: '" << p.array_name << "' ("
              << (arr ? arr->GetNumberOfComponents() : 0) << " components, "
              << (arr ? arr->GetNumberOfTuples() : 0) << " tuples) copied from "
              << src_path << std::endl;
  d.m_Stack.PushMesh(out);
  return consumed;
}

} // namespace cli
} // namespace cmesh
