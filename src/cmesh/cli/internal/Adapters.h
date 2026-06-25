#ifndef CONVERTMESH_CLI_ADAPTERS_H
#define CONVERTMESH_CLI_ADAPTERS_H

#include <string>

namespace cmesh
{
namespace cli
{

class Driver;

// I/O
void ReadMesh(Driver &d, const std::string &filename);
void ReadImage(Driver &d, const std::string &filename);
void WriteMesh(Driver &d, const std::string &filename);
void WriteImage(Driver &d, const std::string &filename);
void WriteTopAutoDetect(Driver &d, const std::string &filename);

// Stack ops
void StackPop(Driver &d);
void StackDup(Driver &d);
void StackSwap(Driver &d);
void StackClear(Driver &d);
void StackAs(Driver &d, const std::string &name);
void StackPopAs(Driver &d, const std::string &name);
void StackPush(Driver &d, const std::string &name);

// Mesh ops — argv is the tail starting at the cmd token. Returns the count
// of additional argument tokens consumed (excluding the cmd token).
int CmdSmoothMesh(Driver &d, int argc, const char *const *argv);
int CmdDecimateMesh(Driver &d, int argc, const char *const *argv);
int CmdComputeNormals(Driver &d, int argc, const char *const *argv);
int CmdFlipNormals(Driver &d);
int CmdMeshDiff(Driver &d, int argc, const char *const *argv);
int CmdExtractIsoSurface(Driver &d, int argc, const char *const *argv);
int CmdRasterizeMesh(Driver &d, int argc, const char *const *argv);
int CmdWarpMesh(Driver &d, int argc, const char *const *argv);
int CmdSampleImageAtMesh(Driver &d, int argc, const char *const *argv);
int CmdMergeArrays(Driver &d, int argc, const char *const *argv);

} // namespace cli
} // namespace cmesh

#endif
