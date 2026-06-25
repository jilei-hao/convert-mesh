#ifndef CONVERTMESH_CLI_PARSE_UTIL_H
#define CONVERTMESH_CLI_PARSE_UTIL_H

#include "cmesh/core/Error.h"
#include "cmesh/core/MeshIO.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

namespace cmesh
{
namespace cli
{

inline std::string ToLower(std::string s)
{
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return s;
}

inline bool IsMeshFilename(const std::string &fn)
{
  std::string ext = ToLower(cmesh::GetExtension(fn));
  return ext == "vtk" || ext == "vtp" || ext == "stl" || ext == "obj"
      || ext == "ply" || ext == "byu" || ext == "y";
}

inline bool IsImageFilename(const std::string &fn)
{
  std::string ext = ToLower(cmesh::GetExtension(fn));
  return ext == "nii" || ext == "gz" || ext == "mha" || ext == "mhd"
      || ext == "nrrd" || ext == "nhdr" || ext == "gipl" || ext == "img"
      || ext == "hdr" || ext == "png" || ext == "jpg" || ext == "jpeg"
      || ext == "tif" || ext == "tiff";
}

// In-command options use a double dash (e.g. --clean) so they are never
// confused with top-level commands (single dash, e.g. -decimate).
inline bool IsSubOption(const char *tok)
{
  return tok && tok[0] == '-' && tok[1] == '-';
}

// Strict numeric parsing: the whole token must be a number. atof/atoi-style
// silent zeroes on typos are the most expensive kind of bug in scripted
// pipelines, so anything else is a ParseError attributed to `cmd`.
inline bool TryParseDouble(const char *tok, double &value)
{
  if(!tok || !*tok) return false;
  char *end = nullptr;
  value = std::strtod(tok, &end);
  return end && *end == '\0';
}

inline double ParseDouble(const char *cmd, const char *what, const char *tok)
{
  double v;
  if(!TryParseDouble(tok, v))
    throw ParseError(std::string(cmd) + ": expected a number for " + what
                     + ", got '" + (tok ? tok : "") + "'");
  return v;
}

inline int ParseInt(const char *cmd, const char *what, const char *tok)
{
  if(!tok || !*tok)
    throw ParseError(std::string(cmd) + ": expected an integer for "
                     + std::string(what));
  char *end = nullptr;
  long v = std::strtol(tok, &end, 10);
  if(!end || *end != '\0')
    throw ParseError(std::string(cmd) + ": expected an integer for " + what
                     + ", got '" + tok + "'");
  return static_cast<int>(v);
}

// Throw for a token that looks like a sub-option but is not recognized by
// the command, so the error names the command instead of surfacing as an
// "unknown command" at the top level.
inline void ThrowUnknownSubOption(const char *cmd, const std::string &sub)
{
  throw ParseError(std::string(cmd) + ": unknown option '" + sub + "'");
}

} // namespace cli
} // namespace cmesh

#endif
