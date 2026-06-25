#include "cmesh/cli/internal/Driver.h"

#include "cmesh/cli/internal/Adapters.h"
#include "cmesh/cli/internal/ParseUtil.h"
#include "cmesh/core/Error.h"
#include "cmesh/core/Version.h"

#include <cstring>
#include <string>

namespace cmesh
{
namespace cli
{

namespace
{

Interpolation ParseInterp(const std::string &raw)
{
  std::string s = ToLower(raw);
  if(s == "linear") return Interpolation::Linear;
  if(s == "nn" || s == "nearest" || s == "nearestneighbor")
    return Interpolation::NearestNeighbor;
  if(s == "bspline") return Interpolation::BSpline;
  throw ParseError("-int: unknown interpolation mode '" + raw
                   + "' (expected linear, nn, or bspline)");
}

} // namespace


const DataItem &Driver::GetVariable(const std::string &name) const
{
  auto it = m_Variables.find(name);
  if(it == m_Variables.end())
    throw ParseError("undefined variable: " + name);
  return it->second;
}

void Driver::ParseAndRun(int argc, const char *const *argv)
{
  for(int i = 0; i < argc;)
  {
    const char *tok = argv[i];
    if(tok[0] == '-')
    {
      int consumed = ProcessCommand(argc - i, argv + i);
      i += 1 + consumed;
    }
    else
    {
      std::string fn = tok;
      if(IsMeshFilename(fn))       ReadMesh(*this, fn);
      else if(IsImageFilename(fn)) ReadImage(*this, fn);
      else
        throw ParseError("cannot determine kind from filename extension: " + fn);
      i += 1;
    }
  }
}

int Driver::ProcessCommand(int argc, const char *const *argv)
{
  std::string cmd = argv[0];

  // -------------------- meta --------------------
  if(cmd == "-h" || cmd == "--help" || cmd == "-help")
  {
    PrintUsage(*m_Out);
    return 0;
  }
  if(cmd == "--version" || cmd == "-version")
  {
    PrintVersion(*m_Out);
    return 0;
  }
  if(cmd == "-verbose")     { m_Verbose = true; return 0; }
  if(cmd == "-no-warn")     { m_WarnOnDataLoss = false; return 0; }

  // -------------------- sticky --------------------
  if(cmd == "-use-vtk")     { m_Backend = Backend::VTK; return 0; }
  if(cmd == "-use-vcg")     { m_Backend = Backend::VCG; return 0; }
  if(cmd == "-use-gpu")     { m_Backend = Backend::GPU; return 0; }
  if(cmd == "-int" || cmd == "-interpolation")
  {
    if(argc < 2) throw ParseError("-int requires an argument");
    m_Interpolation = ParseInterp(argv[1]);
    return 1;
  }
  if(cmd == "-discard-data") { m_DiscardData = true; return 0; }

  // -------------------- I/O --------------------
  if(cmd == "-o")
  {
    if(argc < 2) throw ParseError("-o requires a filename");
    WriteTopAutoDetect(*this, argv[1]);
    return 1;
  }
  if(cmd == "-omesh")
  {
    if(argc < 2) throw ParseError("-omesh requires a filename");
    WriteMesh(*this, argv[1]);
    return 1;
  }
  if(cmd == "-oimage")
  {
    if(argc < 2) throw ParseError("-oimage requires a filename");
    WriteImage(*this, argv[1]);
    return 1;
  }
  if(cmd == "-push-mesh")
  {
    if(argc < 2) throw ParseError("-push-mesh requires a filename");
    ReadMesh(*this, argv[1]);
    return 1;
  }
  if(cmd == "-push-image")
  {
    if(argc < 2) throw ParseError("-push-image requires a filename");
    ReadImage(*this, argv[1]);
    return 1;
  }

  // -------------------- stack ops --------------------
  if(cmd == "-pop")   { StackPop(*this);   return 0; }
  if(cmd == "-dup")   { StackDup(*this);   return 0; }
  if(cmd == "-swap")  { StackSwap(*this);  return 0; }
  if(cmd == "-clear") { StackClear(*this); return 0; }
  if(cmd == "-as")
  {
    if(argc < 2) throw ParseError("-as requires a variable name");
    StackAs(*this, argv[1]);
    return 1;
  }
  if(cmd == "-popas")
  {
    if(argc < 2) throw ParseError("-popas requires a variable name");
    StackPopAs(*this, argv[1]);
    return 1;
  }
  if(cmd == "-push")
  {
    if(argc < 2) throw ParseError("-push requires a variable name");
    StackPush(*this, argv[1]);
    return 1;
  }

  // -------------------- mesh + image ops --------------------
  if(cmd == "-smooth-mesh")          return CmdSmoothMesh(*this, argc, argv);
  if(cmd == "-decimate")             return CmdDecimateMesh(*this, argc, argv);
  if(cmd == "-compute-normals" || cmd == "-normals")
                                     return CmdComputeNormals(*this, argc, argv);
  if(cmd == "-flip-normals")         return CmdFlipNormals(*this);
  if(cmd == "-meshdiff")             return CmdMeshDiff(*this, argc, argv);
  if(cmd == "-extract-isosurface" || cmd == "-isosurface")
                                     return CmdExtractIsoSurface(*this, argc, argv);
  if(cmd == "-rasterize")            return CmdRasterizeMesh(*this, argc, argv);
  if(cmd == "-warp-mesh")            return CmdWarpMesh(*this, argc, argv);
  if(cmd == "-sample-image")         return CmdSampleImageAtMesh(*this, argc, argv);
  if(cmd == "-merge-array" || cmd == "-merge-arrays")
                                     return CmdMergeArrays(*this, argc, argv);

  throw ParseError("unknown command: " + cmd);
}

void Driver::PrintVersion(std::ostream &out) const
{
  out << "cmesh (ConvertMesh) version " << CONVERTMESH_VERSION_FULL
      << std::endl;
}

void Driver::PrintUsage(std::ostream &out) const
{
  out <<
    "cmesh - stack-based mesh and image processing pipeline.\n"
    "\n"
    "Usage: cmesh [ARG|-COMMAND [ARGS...]]...\n"
    "\n"
    "I/O:\n"
    "  FILE                Auto-detect (by extension); push mesh or image onto the stack.\n"
    "  -push-mesh FILE     Read a mesh and push onto the stack.\n"
    "  -push-image FILE    Read an image and push onto the stack.\n"
    "  -o FILE             Write top of stack to FILE (kind inferred from extension).\n"
    "  -omesh FILE         Write top mesh to FILE.\n"
    "  -oimage FILE        Write top image to FILE.\n"
    "\n"
    "Stack ops:\n"
    "  -pop                Discard top of stack.\n"
    "  -dup                Duplicate top of stack.\n"
    "  -swap               Swap top two stack items.\n"
    "  -clear              Empty the stack.\n"
    "  -as NAME            Assign top of stack to variable NAME (keeps on stack).\n"
    "  -popas NAME         Assign top of stack to variable NAME and pop.\n"
    "  -push NAME          Push variable NAME onto the stack.\n"
    "\n"
    "Backend / mode (sticky):\n"
    "  -use-vtk            Prefer VTK-backed implementations (default).\n"
    "  -use-vcg            Prefer VCG-backed implementations.\n"
    "  -use-gpu            Prefer GPU-backed implementations (reserved).\n"
    "  -int MODE           Interpolation mode: linear (default), nn, bspline.\n"
    "  -discard-data       Acknowledge that subsequent ops may drop vtkPolyData\n"
    "                      arrays; suppresses dropped-array warnings.\n"
    "  -verbose            Print per-operation progress.\n"
    "  -no-warn            Silence data-loss warnings.\n"
    "\n"
    "Mesh operations:\n"
    "  (In-command options use a double dash, e.g. --clean, to distinguish them\n"
    "   from top-level commands like -decimate.)\n"
    "  -extract-isosurface T [modifiers]\n"
    "                      Pop image, push iso-surface at threshold T.\n"
    "                      Modifiers: --method NAME, --clean, --smooth-pre SIGMA,\n"
    "                                 --decimate-post FRAC.\n"
    "                      NAME: marching-cubes (default), flying-edges,\n"
    "                            discrete-marching-cubes, discrete-flying-edges,\n"
    "                            surface-nets. The discrete/surface-nets flavors\n"
    "                            extract one surface per label >= T.\n"
    "                      --smooth-pre applies to continuous methods only.\n"
    "  -smooth-mesh N [RELAX]\n"
    "                      Laplacian smooth top-of-stack mesh (N iterations).\n"
    "  -decimate FRAC      Reduce polygon count by FRAC (0..1).\n"
    "  -compute-normals [--auto-orient]\n"
    "                      Compute polydata normals.\n"
    "  -flip-normals       Reverse triangle winding (array-preserving).\n"
    "  -meshdiff [REF.vtp] Add 'Distance' array of top mesh vs. reference.\n"
    "                      With no argument, pops the reference from the stack\n"
    "                      instead: [ ..., source, reference (top) ].\n"
    "\n"
    "Image / mesh interop:\n"
    "  -rasterize [--ref REF | --spacing SX SY SZ] [--margin M] [--inside V]\n"
    "                      Pop mesh, push a binary image covering its interior.\n"
    "  -warp-mesh WARP     Displace top mesh by an ITK vector warp field.\n"
    "  -sample-image NAME  Pop image, annotate mesh below with named scalar array\n"
    "                      sampled via the sticky -int interpolation mode.\n"
    "  -merge-array [SRC] NAME [--cell] [--rename NEW]\n"
    "                      Copy named point/cell array from SRC mesh onto top mesh.\n"
    "                      Without SRC, pops the source mesh from the stack\n"
    "                      instead: [ ..., destination, source (top) ].\n"
    "\n"
    "Meta:\n"
    "  -h, --help          Print this message.\n"
    "  --version           Print version.\n";
}

} // namespace cli
} // namespace cmesh
