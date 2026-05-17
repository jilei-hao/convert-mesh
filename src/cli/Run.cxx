#include "cli/Run.h"

#include "cli/internal/Driver.h"
#include "core/Error.h"

#include <iostream>
#include <vector>

namespace cmesh
{
namespace cli
{

int Run(int argc, const char *const *argv, std::ostream &out, std::ostream &err)
{
  Driver driver;
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

int Run(const std::vector<std::string> &args, std::ostream &out, std::ostream &err)
{
  std::vector<const char *> argv;
  argv.reserve(args.size());
  for(const auto &s : args) argv.push_back(s.c_str());
  return Run(static_cast<int>(argv.size()), argv.data(), out, err);
}

} // namespace cli
} // namespace cmesh
