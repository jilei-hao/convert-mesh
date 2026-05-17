#ifndef CONVERTMESH_CLI_RUN_H
#define CONVERTMESH_CLI_RUN_H

#include <iosfwd>
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

} // namespace cli
} // namespace cmesh

#endif
