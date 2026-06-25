#include "cmesh/cli/Run.h"

#include <iostream>

int main(int argc, char *argv[])
{
  if(argc < 2)
    return cmesh::cli::Run(0, nullptr, std::cout, std::cerr);
  return cmesh::cli::Run(argc - 1, argv + 1, std::cout, std::cerr);
}
