#ifndef CONVERTMESH_CLI_RUN_H
#define CONVERTMESH_CLI_RUN_H

#include <itkImageBase.h>
#include <itkSmartPointer.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace cmesh
{
namespace cli
{

/**
 * Run the cmesh CLI parser against an argv-style argument list. Argv[0]
 * (the program name) should NOT be included — pass only the command tokens.
 *
 * Returns 0 on success, non-zero on failure. All exceptions thrown by the
 * core layer or by the parser are caught and reported on `err`; the
 * non-zero return code corresponds to the exception category:
 *   2  parse or usage error
 *   3  I/O error
 *   4  algorithm error
 *   5  abort
 *   1  unclassified
 *
 * The library form is the same parser that the cmesh binary uses; bind it
 * from Python (or any other host) to get bit-identical CLI behavior.
 */
int Run(const std::vector<std::string> &args,
        std::ostream &out,
        std::ostream &err);

int Run(int argc, const char *const *argv,
        std::ostream &out,
        std::ostream &err);

/**
 * A persistent CLI session for hosts that need to pass data in memory
 * instead of through the filesystem (Python bindings, GUI integrations).
 *
 * A Session owns a driver whose named variables, stack, and sticky state
 * survive across Run() calls. Seed inputs with SetMeshVariable /
 * SetImageVariable, reference them in the pipeline with `-push NAME`,
 * capture outputs with `-as NAME` / `-popas NAME`, and read them back with
 * GetMeshVariable / GetImageVariable:
 *
 *   cmesh::cli::Session session;
 *   session.SetImageVariable("seg", segImage);
 *   int rc = session.Run({"-push", "seg",
 *                         "-extract-isosurface", "0.5", "--clean",
 *                         "-popas", "surf"},
 *                        out, err);
 *   if(rc == 0)
 *     vtkSmartPointer<vtkPolyData> surf = session.GetMeshVariable("surf");
 *
 * Run() reports errors with the same exit-code mapping as the free Run()
 * functions. The getters throw cmesh::Error subclasses instead: ParseError
 * for an undefined variable, TypeError for a kind mismatch.
 */
class Session
{
public:
  Session();
  ~Session();
  Session(Session &&) noexcept;
  Session &operator=(Session &&) noexcept;
  Session(const Session &) = delete;
  Session &operator=(const Session &) = delete;

  /** Seed a named variable with an in-memory mesh (in NIFTI RAS space). */
  void SetMeshVariable(const std::string &name, vtkPolyData *mesh);

  /** Seed a named variable with an in-memory ITK image. */
  void SetImageVariable(const std::string &name, itk::ImageBase<3> *image);

  bool HasVariable(const std::string &name) const;

  /** Retrieve a mesh variable (seeded, or assigned via -as / -popas). */
  vtkSmartPointer<vtkPolyData> GetMeshVariable(const std::string &name) const;

  /** Retrieve an image variable (seeded, or assigned via -as / -popas). */
  itk::SmartPointer<itk::ImageBase<3>>
  GetImageVariable(const std::string &name) const;

  /**
   * Run command tokens against this session's driver. Same semantics and
   * exit codes as the free Run(); state persists for the next call.
   */
  int Run(const std::vector<std::string> &args,
          std::ostream &out,
          std::ostream &err);

private:
  class Impl;
  std::unique_ptr<Impl> m_Impl;
};

} // namespace cli
} // namespace cmesh

#endif
