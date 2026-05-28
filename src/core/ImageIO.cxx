#include "core/ImageIO.h"

#include "core/PixelDispatch.h"

namespace cmesh
{

itk::SmartPointer<itk::ImageBase<3>> ReadImage(const std::string &filename)
{
  // Probe the on-disk pixel type via ITK's IO factory, then read into the
  // matching itk::Image<T,3>. This gives the caller a base pointer that
  // WithPixelType can dispatch on losslessly.
  itk::ImageIOBase::Pointer io = itk::ImageIOFactory::CreateImageIO(
      filename.c_str(), itk::ImageIOFactory::IOFileModeEnum::ReadMode);
  if(!io)
    throw IOError("no ITK image-IO factory can read: " + filename);
  io->SetFileName(filename);
  try
  {
    io->ReadImageInformation();
  }
  catch(itk::ExceptionObject &e)
  {
    throw IOError("failed to probe image '" + filename + "': " + e.GetDescription());
  }

  using IOComp = itk::IOComponentEnum;
  switch(io->GetComponentType())
  {
    case IOComp::UCHAR:  return ReadImageAs<unsigned char >(filename).GetPointer();
    case IOComp::CHAR:   return ReadImageAs<char          >(filename).GetPointer();
    case IOComp::USHORT: return ReadImageAs<unsigned short>(filename).GetPointer();
    case IOComp::SHORT:  return ReadImageAs<short         >(filename).GetPointer();
    case IOComp::UINT:   return ReadImageAs<unsigned int  >(filename).GetPointer();
    case IOComp::INT:    return ReadImageAs<int           >(filename).GetPointer();
    case IOComp::FLOAT:  return ReadImageAs<float         >(filename).GetPointer();
    case IOComp::DOUBLE: return ReadImageAs<double        >(filename).GetPointer();
    default:
      throw IOError("unsupported pixel component type in '" + filename + "'");
  }
}

void WriteImage(itk::ImageBase<3> *image, const std::string &filename,
                bool use_compression)
{
  if(!image)
    throw IOError("WriteImage: null input image");
  WithPixelType(image, [&](auto *typed) {
    using T = typename std::remove_pointer_t<decltype(typed)>::PixelType;
    WriteImageAs<T>(typed, filename, use_compression);
  });
}

} // namespace cmesh
