#ifndef CONVERTMESH_CORE_ERROR_H
#define CONVERTMESH_CORE_ERROR_H

#include <stdexcept>
#include <string>

namespace cmesh
{

class Error : public std::runtime_error
{
public:
  explicit Error(const std::string &what) : std::runtime_error(what) {}
};

class IOError : public Error
{
public:
  explicit IOError(const std::string &what) : Error(what) {}
};

class TypeError : public Error
{
public:
  TypeError(const std::string &expected, const std::string &actual)
      : Error("expected '" + expected + "', got '" + actual + "'") {}
  explicit TypeError(const std::string &what) : Error(what) {}
};

class AlgorithmError : public Error
{
public:
  explicit AlgorithmError(const std::string &what) : Error(what) {}
};

class AbortError : public Error
{
public:
  AbortError() : Error("operation aborted") {}
  explicit AbortError(const std::string &what) : Error(what) {}
};

class StackError : public Error
{
public:
  StackError() : Error("attempt to access empty cmesh stack") {}
  explicit StackError(const std::string &what) : Error(what) {}
};

class ParseError : public Error
{
public:
  explicit ParseError(const std::string &what) : Error(what) {}
};

} // namespace cmesh

#endif
