#ifndef CONVERTMESH_CORE_PIXEL_DISPATCH_H
#define CONVERTMESH_CORE_PIXEL_DISPATCH_H

#include "core/Error.h"

#include <itkImage.h>
#include <itkImageBase.h>

namespace cmesh
{

/**
 * Dispatch a callable on the runtime pixel type of an itk::ImageBase<3>.
 * The callable is invoked once with a typed `itk::Image<T,3>*` argument.
 *
 * Supported pixel types form a closed list; extending it requires adding
 * one branch here and one explicit instantiation in each templated core
 * function that participates in dispatch.
 *
 * Throws cmesh::TypeError if the runtime image type is not in the list.
 */
template <class F>
decltype(auto) WithPixelType(itk::ImageBase<3> *base, F &&f)
{
  if(auto *p = dynamic_cast<itk::Image<unsigned char, 3> *>(base)) return f(p);
  if(auto *p = dynamic_cast<itk::Image<char, 3> *>(base))          return f(p);
  if(auto *p = dynamic_cast<itk::Image<short, 3> *>(base))         return f(p);
  if(auto *p = dynamic_cast<itk::Image<unsigned short, 3> *>(base))return f(p);
  if(auto *p = dynamic_cast<itk::Image<int, 3> *>(base))           return f(p);
  if(auto *p = dynamic_cast<itk::Image<unsigned int, 3> *>(base))  return f(p);
  if(auto *p = dynamic_cast<itk::Image<float, 3> *>(base))         return f(p);
  if(auto *p = dynamic_cast<itk::Image<double, 3> *>(base))        return f(p);
  throw TypeError("unsupported pixel type on itk::ImageBase<3>");
}

} // namespace cmesh

#endif
