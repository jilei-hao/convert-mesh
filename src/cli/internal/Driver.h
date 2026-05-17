#ifndef CONVERTMESH_CLI_DRIVER_H
#define CONVERTMESH_CLI_DRIVER_H

#include "cli/internal/DataItem.h"
#include "cli/internal/DataStack.h"
#include "core/Backend.h"

#include <iostream>
#include <map>
#include <sstream>
#include <streambuf>
#include <string>

namespace cmesh
{
namespace cli
{

/**
 * CLI driver: owns the stack, sticky state, named variables, and the
 * output streams. Parsing lives in `ParseAndRun`.
 *
 * Each cmesh CLI command corresponds to a small adapter function (see
 * cli/internal/adapters.cxx) that pops from the stack, calls the matching
 * cmesh:: core function, and pushes the result.
 */
class Driver
{
public:
  Driver() : m_Out(&std::cout), m_Err(&std::cerr) {}

  // Parse + dispatch a command list (argv WITHOUT argv[0]). Throws on error.
  void ParseAndRun(int argc, const char *const *argv);

  // Single command. Returns the number of argument tokens consumed after
  // argv[0]. Throws on error.
  int  ProcessCommand(int argc, const char *const *argv);

  // Variables (-as / -popas / -push).
  void SetVariable(const std::string &name, const DataItem &item) { m_Variables[name] = item; }
  const DataItem &GetVariable(const std::string &name) const;

  void RedirectOutput(std::ostream &out, std::ostream &err)
  {
    m_Out = &out; m_Err = &err;
  }
  std::ostream &Out() { return *m_Out; }
  std::ostream &Err() { return *m_Err; }
  std::ostream &Verbose() { return m_Verbose ? *m_Out : m_Null; }

  void PrintUsage(std::ostream &out) const;
  void PrintVersion(std::ostream &out) const;

  DataStack m_Stack;
  std::map<std::string, DataItem> m_Variables;

  // Sticky state
  Backend       m_Backend       = Backend::VTK;
  Interpolation m_Interpolation = Interpolation::Linear;
  bool          m_DiscardData   = false;
  bool          m_Verbose       = false;
  bool          m_WarnOnDataLoss = true;

private:
  std::ostream *m_Out;
  std::ostream *m_Err;
  class NullStream : public std::ostream
  {
  public:
    NullStream() : std::ostream(&m_Buf) {}
  private:
    class NullBuf : public std::streambuf
    {
    protected:
      int overflow(int c) override { return c; }
    } m_Buf;
  };
  NullStream m_Null;
};

} // namespace cli
} // namespace cmesh

#endif
