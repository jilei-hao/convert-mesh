#include "cmesh/cli/Run.h"

#include "cmesh/cli/internal/Driver.h"
#include "cmesh/core/Error.h"

#include <iostream>
#include <vector>

namespace cmesh
{
namespace cli
{

namespace
{

// Shared by the one-shot Run() and Session::Run(): exception category ->
// shell-style exit code, with the message reported on `err`.
int RunWithDriver(Driver &driver, int argc, const char *const *argv,
                  std::ostream &out, std::ostream &err)
{
  driver.RedirectOutput(out, err);
  try
  {
    if(argc <= 0)
    {
      driver.PrintUsage(out);
      return 0;
    }
    driver.ParseAndRun(argc, argv);
    return 0;
  }
  catch(ParseError &e)
  {
    err << "cmesh: parse error: " << e.what() << std::endl;
    return 2;
  }
  catch(IOError &e)
  {
    err << "cmesh: I/O error: " << e.what() << std::endl;
    return 3;
  }
  catch(AlgorithmError &e)
  {
    err << "cmesh: algorithm error: " << e.what() << std::endl;
    return 4;
  }
  catch(AbortError &e)
  {
    err << "cmesh: aborted: " << e.what() << std::endl;
    return 5;
  }
  catch(Error &e)
  {
    err << "cmesh: error: " << e.what() << std::endl;
    return 1;
  }
  catch(std::exception &e)
  {
    err << "cmesh: unexpected error: " << e.what() << std::endl;
    return 1;
  }
}

std::vector<const char *> ToArgv(const std::vector<std::string> &args)
{
  std::vector<const char *> argv;
  argv.reserve(args.size());
  for(const auto &s : args) argv.push_back(s.c_str());
  return argv;
}

} // namespace

int Run(int argc, const char *const *argv, std::ostream &out, std::ostream &err)
{
  Driver driver;
  return RunWithDriver(driver, argc, argv, out, err);
}

int Run(const std::vector<std::string> &args, std::ostream &out, std::ostream &err)
{
  std::vector<const char *> argv = ToArgv(args);
  return Run(static_cast<int>(argv.size()), argv.data(), out, err);
}

// ---------------------------------------------------------------------------
// Session
// ---------------------------------------------------------------------------

class Session::Impl
{
public:
  Driver driver;
};

Session::Session() : m_Impl(new Impl) {}
Session::~Session() = default;
Session::Session(Session &&) noexcept = default;
Session &Session::operator=(Session &&) noexcept = default;

void Session::SetMeshVariable(const std::string &name, vtkPolyData *mesh)
{
  if(!mesh) throw TypeError("mesh", "(null)");
  m_Impl->driver.SetVariable(name, DataItem::FromMesh(mesh));
}

void Session::SetImageVariable(const std::string &name, itk::ImageBase<3> *image)
{
  if(!image) throw TypeError("image", "(null)");
  m_Impl->driver.SetVariable(name, DataItem::FromImage(image));
}

bool Session::HasVariable(const std::string &name) const
{
  return m_Impl->driver.m_Variables.count(name) > 0;
}

vtkSmartPointer<vtkPolyData>
Session::GetMeshVariable(const std::string &name) const
{
  const DataItem &item = m_Impl->driver.GetVariable(name);
  if(!item.IsMesh()) throw TypeError("mesh", item.KindName());
  return item.mesh;
}

itk::SmartPointer<itk::ImageBase<3>>
Session::GetImageVariable(const std::string &name) const
{
  const DataItem &item = m_Impl->driver.GetVariable(name);
  if(!item.IsImage()) throw TypeError("image", item.KindName());
  return item.image;
}

int Session::Run(const std::vector<std::string> &args,
                 std::ostream &out, std::ostream &err)
{
  std::vector<const char *> argv = ToArgv(args);
  return RunWithDriver(m_Impl->driver, static_cast<int>(argv.size()),
                       argv.data(), out, err);
}

} // namespace cli
} // namespace cmesh
