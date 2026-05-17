#ifndef CONVERTMESH_CORE_IMAGE_IO_H
#define CONVERTMESH_CORE_IMAGE_IO_H

#include "core/Error.h"

#include <itkImage.h>
#include <itkImageBase.h>
#include <itkImageFileReader.h>
#include <itkImageFileWriter.h>
#include <itkImageIOBase.h>
#include <itkImageIOFactory.h>
#include <itkSmartPointer.h>

#include <string>

namespace cmesh
{

/**
 * Typed image read. Caller picks the in-memory pixel type; ITK does the
 * format-driven conversion.
 */
template <class T>
itk::SmartPointer<itk::Image<T, 3>> ReadImageAs(const std::string &filename)
{
  using ImageType = itk::Image<T, 3>;
  using ReaderType = itk::ImageFileReader<ImageType>;
  typename ReaderType::Pointer reader = ReaderType::New();
  reader->SetFileName(filename);
  try
  {
    reader->Update();
  }
  catch(itk::ExceptionObject &e)
  {
    throw IOError("failed to read image '" + filename + "': " + e.GetDescription());
  }
  return reader->GetOutput();
}

/**
 * Polymorphic image read. Returns the image as an itk::ImageBase<3> that
 * carries the on-disk pixel type. Use cmesh::WithPixelType to dispatch on
 * it, or hand it directly to any cmesh core function that accepts an
 * ImageBase overload.
 *
 * Implemented header-only so it can be inlined without an extra TU.
 */
itk::SmartPointer<itk::ImageBase<3>> ReadImage(const std::string &filename);

/**
 * Typed image write.
 */
template <class T>
void WriteImageAs(itk::Image<T, 3> *image, const std::string &filename,
                  bool use_compression = true)
{
  using ImageType = itk::Image<T, 3>;
  using WriterType = itk::ImageFileWriter<ImageType>;
  typename WriterType::Pointer writer = WriterType::New();
  writer->SetFileName(filename);
  writer->SetInput(image);
  writer->SetUseCompression(use_compression);
  try
  {
    writer->Update();
  }
  catch(itk::ExceptionObject &e)
  {
    throw IOError("failed to write image '" + filename + "': " + e.GetDescription());
  }
}

/**
 * Polymorphic image write — dispatches on the runtime pixel type held by
 * the base pointer and calls the corresponding typed writer.
 */
void WriteImage(itk::ImageBase<3> *image, const std::string &filename,
                bool use_compression = true);

} // namespace cmesh

#endif
