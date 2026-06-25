#include "cmesh/cli/internal/Adapters.h"

#include "cmesh/cli/internal/Driver.h"
#include "cmesh/cli/internal/ParseUtil.h"
#include "cmesh/core/ComputeNormals.h"
#include "cmesh/core/DecimateMesh.h"
#include "cmesh/core/Error.h"
#include "cmesh/core/ExtractIsoSurface.h"
#include "cmesh/core/FlipNormals.h"
#include "cmesh/core/ImageIO.h"
#include "cmesh/core/MergeArrays.h"
#include "cmesh/core/MeshDiff.h"
#include "cmesh/core/MeshIO.h"
#include "cmesh/core/RasterizeMesh.h"
#include "cmesh/core/SampleImageAtMesh.h"
#include "cmesh/core/SmoothMesh.h"
#include "cmesh/core/WarpMesh.h"

#include <itkImageFileReader.h>

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkDataSetAttributes.h>
#include <vtkPointData.h>

#include <cstdlib>
#include <cstring>
#include <vector>

namespace cmesh
{
namespace cli
{

namespace
{

void CollectArrayNames(vtkDataSetAttributes *attr, const char *prefix,
                       std::vector<std::string> &names)
{
  if(!attr) return;
  for(int i = 0; i < attr->GetNumberOfArrays(); ++i)
  {
    const char *name = attr->GetArrayName(i);
    names.push_back(std::string(prefix) + (name ? name : "(unnamed)"));
  }
}

std::vector<std::string> ArrayNames(vtkPolyData *pd)
{
  std::vector<std::string> names;
  if(pd)
  {
    CollectArrayNames(pd->GetPointData(), "point:", names);
    CollectArrayNames(pd->GetCellData(), "cell:", names);
  }
  return names;
}

// Sticky data-loss policy: ops that drop polydata arrays warn unless the
// user acknowledged the loss with -discard-data or silenced warnings with
// -no-warn.
void EnforceDataLossPolicy(Driver &d, const std::vector<std::string> &before,
                           vtkPolyData *after, const char *cmd)
{
  if(d.m_DiscardData || !d.m_WarnOnDataLoss) return;
  std::vector<std::string> after_names = ArrayNames(after);
  std::string dropped;
  for(const auto &name : before)
  {
    if(std::find(after_names.begin(), after_names.end(), name)
       == after_names.end())
      dropped += (dropped.empty() ? "" : ", ") + name;
  }
  if(!dropped.empty())
    d.Err() << "warning: " << cmd << " dropped data arrays [" << dropped
            << "]; pass -discard-data to acknowledge or -no-warn to silence."
            << std::endl;
}

} // namespace

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
  p.iterations = ParseInt(argv[0], "iterations", argv[1]);
  p.backend    = d.m_Backend;
  int consumed = 1;
  // RELAX is optional; consume the next token only if it is a number, so a
  // following filename operand is left for the driver.
  double relax;
  if(argc >= 3 && TryParseDouble(argv[2], relax))
  {
    p.relaxation_factor = relax;
    consumed = 2;
  }
  auto in  = d.m_Stack.PopMesh();
  auto before = ArrayNames(in);
  auto out = cmesh::SmoothMesh(in, p);
  d.Verbose() << "SmoothMesh: " << p.iterations << " iterations, "
              << "relax=" << p.relaxation_factor << std::endl;
  EnforceDataLossPolicy(d, before, out, argv[0]);
  d.m_Stack.PushMesh(out);
  return consumed;
}

int CmdDecimateMesh(Driver &d, int argc, const char *const *argv)
{
  if(argc < 2) throw ParseError("-decimate requires a reduction factor");
  cmesh::DecimateParams p;
  p.reduction = ParseDouble(argv[0], "reduction factor", argv[1]);
  if(p.reduction <= 0.0 || p.reduction >= 1.0)
    throw ParseError("-decimate: reduction factor must be in (0, 1), got "
                     + std::string(argv[1]));
  p.backend   = d.m_Backend;
  auto in = d.m_Stack.PopMesh();
  auto before = ArrayNames(in);
  auto out = cmesh::DecimateMesh(in, p);
  d.Verbose() << "DecimateMesh: reduction=" << p.reduction
              << ", out points=" << out->GetNumberOfPoints()
              << ", out cells=" << out->GetNumberOfCells() << std::endl;
  EnforceDataLossPolicy(d, before, out, argv[0]);
  d.m_Stack.PushMesh(out);
  return 1;
}

int CmdComputeNormals(Driver &d, int argc, const char *const *argv)
{
  cmesh::NormalsParams p;
  int consumed = 0;
  while(consumed + 1 < argc && IsSubOption(argv[consumed + 1]))
  {
    std::string sub = argv[consumed + 1];
    if(sub == "--auto-orient") { p.auto_orient = true; consumed += 1; }
    else ThrowUnknownSubOption(argv[0], sub);
  }
  auto in  = d.m_Stack.PopMesh();
  auto before = ArrayNames(in);
  auto out = cmesh::ComputeNormals(in, p);
  d.Verbose() << "ComputeNormals: feature_angle=" << p.feature_angle
              << (p.auto_orient ? ", auto-oriented" : "") << std::endl;
  EnforceDataLossPolicy(d, before, out, argv[0]);
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
  // Two forms:
  //   -meshdiff REF.vtp   reference read from file
  //   -meshdiff           reference popped from the stack;
  //                       layout [ ..., source, reference (top) ]
  cmesh::MeshDiffParams p;
  vtkSmartPointer<vtkPolyData> ref;
  std::string ref_label;
  int consumed = 0;
  if(argc >= 2 && argv[1][0] != '-')
  {
    ref_label = argv[1];
    ref = cmesh::ReadPolyData(ref_label);
    consumed = 1;
  }
  else
  {
    ref = d.m_Stack.PopMesh();
    ref_label = "(stack)";
  }
  auto source = d.m_Stack.PopMesh();
  cmesh::MeshDiffStats stats;
  auto annotated = cmesh::MeshDiff(source, ref, p, &stats);
  d.Out() << "MeshDiff: " << ref_label << std::endl
          << "  N=" << stats.n_points
          << "  mean=" << stats.mean
          << "  rms="  << stats.rms
          << "  hausdorff(source->ref)=" << stats.hausdorff << std::endl;
  d.m_Stack.PushMesh(annotated);
  return consumed;
}

int CmdExtractIsoSurface(Driver &d, int argc, const char *const *argv)
{
  if(argc < 2) throw ParseError(std::string(argv[0]) + " requires a threshold value");
  cmesh::IsoSurfaceParams p;
  p.threshold = ParseDouble(argv[0], "threshold", argv[1]);
  p.backend   = d.m_Backend;
  bool explicit_smooth_pre = false;
  int consumed = 1;
  while(consumed + 1 < argc && IsSubOption(argv[consumed + 1]))
  {
    std::string sub = argv[consumed + 1];
    if(sub == "--method")
    {
      if(consumed + 2 >= argc) throw ParseError("--method needs a name");
      std::string name = argv[consumed + 2];
      if(name == "marching-cubes")          p.method = cmesh::IsoMethod::MarchingCubes;
      else if(name == "flying-edges")       p.method = cmesh::IsoMethod::FlyingEdges;
      else if(name == "discrete-marching-cubes") p.method = cmesh::IsoMethod::DiscreteMarchingCubes;
      else if(name == "discrete-flying-edges")   p.method = cmesh::IsoMethod::DiscreteFlyingEdges;
      else if(name == "surface-nets")       p.method = cmesh::IsoMethod::SurfaceNets;
      else throw ParseError("--method: unknown algorithm '" + name + "'");
      consumed += 2;
    }
    else if(sub == "--clean")  { p.clean = true; consumed += 1; }
    else if(sub == "--smooth-pre")
    {
      if(consumed + 2 >= argc) throw ParseError("--smooth-pre needs a value");
      p.smooth_pre = ParseDouble(argv[0], "--smooth-pre", argv[consumed + 2]);
      explicit_smooth_pre = true;
      consumed += 2;
    }
    else if(sub == "--decimate-post")
    {
      if(consumed + 2 >= argc) throw ParseError("--decimate-post needs a value");
      p.decimate = ParseDouble(argv[0], "--decimate-post", argv[consumed + 2]);
      consumed += 2;
    }
    else ThrowUnknownSubOption(argv[0], sub);
  }

  // Reject explicitly-given options that the chosen method ignores or
  // misuses, before any work happens. The core validates too, but here we
  // can attribute the error to the user's command line.
  if(explicit_smooth_pre && cmesh::IsDiscrete(p.method))
    throw ParseError(std::string(argv[0])
                     + ": --smooth-pre does not apply to discrete methods "
                       "(it would blend label values); use -smooth-mesh on "
                       "the extracted surface instead");
  cmesh::ValidateIsoSurfaceParams(p);

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
  bool explicit_spacing = false;
  int consumed = 0;
  while(consumed + 1 < argc && IsSubOption(argv[consumed + 1]))
  {
    std::string sub = argv[consumed + 1];
    if(sub == "--ref")
    {
      if(consumed + 2 >= argc) throw ParseError("--ref needs a filename");
      ref_path = argv[consumed + 2];
      consumed += 2;
    }
    else if(sub == "--spacing")
    {
      if(consumed + 4 >= argc) throw ParseError("--spacing needs sx sy sz");
      p.spacing[0] = ParseDouble(argv[0], "--spacing sx", argv[consumed + 2]);
      p.spacing[1] = ParseDouble(argv[0], "--spacing sy", argv[consumed + 3]);
      p.spacing[2] = ParseDouble(argv[0], "--spacing sz", argv[consumed + 4]);
      explicit_spacing = true;
      consumed += 4;
    }
    else if(sub == "--margin")
    {
      if(consumed + 2 >= argc) throw ParseError("--margin needs a value");
      p.margin = ParseDouble(argv[0], "--margin", argv[consumed + 2]);
      consumed += 2;
    }
    else if(sub == "--inside")
    {
      if(consumed + 2 >= argc) throw ParseError("--inside needs a value");
      p.inside_value = static_cast<float>(
          ParseDouble(argv[0], "--inside", argv[consumed + 2]));
      consumed += 2;
    }
    else ThrowUnknownSubOption(argv[0], sub);
  }

  if(!ref_path.empty() && explicit_spacing)
    throw ParseError(std::string(argv[0])
                     + ": --ref and --spacing are mutually exclusive "
                       "(the reference image already defines the grid)");

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
  if(stats.n_outside > 0 && d.m_WarnOnDataLoss)
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
  // Two forms, disambiguated by whether the first argument looks like a
  // mesh filename (array names with mesh extensions need the file form):
  //   -merge-array SRC NAME ...   source mesh read from file
  //   -merge-array NAME ...       source mesh popped from the stack;
  //                               layout [ ..., destination, source (top) ]
  if(argc < 2) throw ParseError(std::string(argv[0])
                                + " requires an ARRAY_NAME (and optionally "
                                  "a SOURCE_MESH file before it)");
  cmesh::MergeArraysParams p;
  std::string src_path;
  vtkSmartPointer<vtkPolyData> source;
  int consumed;
  if(IsMeshFilename(argv[1]))
  {
    if(argc < 3 || IsSubOption(argv[2]))
      throw ParseError(std::string(argv[0])
                       + ": ARRAY_NAME is required after the source mesh");
    src_path = argv[1];
    p.array_name = argv[2];
    consumed = 2;
  }
  else
  {
    p.array_name = argv[1];
    consumed = 1;
  }
  while(consumed + 1 < argc && IsSubOption(argv[consumed + 1]))
  {
    std::string sub = argv[consumed + 1];
    if(sub == "--cell") { p.cell_data = true; consumed += 1; }
    else if(sub == "--rename")
    {
      if(consumed + 2 >= argc) throw ParseError("--rename needs a name");
      p.rename_to = argv[consumed + 2];
      consumed += 2;
    }
    else ThrowUnknownSubOption(argv[0], sub);
  }

  if(src_path.empty())
    source = d.m_Stack.PopMesh();
  else
    source = cmesh::ReadPolyData(src_path);

  if(d.m_Stack.empty() || !d.m_Stack.back().IsMesh())
    throw TypeError("mesh",
                    d.m_Stack.empty() ? "(empty)"
                                      : d.m_Stack.back().KindName());
  auto dest = d.m_Stack.back().mesh;
  d.m_Stack.pop();

  auto out = cmesh::MergeArrays(dest, source, p);

  vtkDataSetAttributes *out_attr = p.cell_data
      ? static_cast<vtkDataSetAttributes *>(out->GetCellData())
      : static_cast<vtkDataSetAttributes *>(out->GetPointData());
  auto *arr = out_attr->GetArray(p.rename_to.empty() ? p.array_name.c_str()
                                                     : p.rename_to.c_str());
  d.Verbose() << "MergeArrays: '" << p.array_name << "' ("
              << (arr ? arr->GetNumberOfComponents() : 0) << " components, "
              << (arr ? arr->GetNumberOfTuples() : 0) << " tuples) copied from "
              << (src_path.empty() ? "(stack)" : src_path) << std::endl;
  d.m_Stack.PushMesh(out);
  return consumed;
}

} // namespace cli
} // namespace cmesh
