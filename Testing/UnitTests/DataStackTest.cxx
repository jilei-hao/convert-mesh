// DataStackTest exercises the CLI-internal stack (the public core API
// has no stack; the stack lives only inside cmesh_cli, backing the parser).

#include "TestHarness.h"

#include "cmesh/cli/internal/DataStack.h"
#include "cmesh/core/Error.h"

#include <itkImage.h>
#include <vtkNew.h>
#include <vtkPolyData.h>

using cmesh::cli::DataStack;
using cmesh::StackError;
using cmesh::TypeError;

int main()
{
  DataStack s;
  CM_CHECK(s.empty());
  CM_CHECK_EQ(s.size(), 0u);

  vtkNew<vtkPolyData> pd1;
  vtkNew<vtkPolyData> pd2;
  s.PushMesh(pd1);
  s.PushMesh(pd2);
  CM_CHECK_EQ(s.size(), 2u);
  CM_CHECK(s.back().IsMesh());
  CM_CHECK_EQ(s.back().mesh.GetPointer(), pd2.GetPointer());

  auto popped = s.PopMesh();
  CM_CHECK_EQ(popped.GetPointer(), pd2.GetPointer());
  CM_CHECK_EQ(s.size(), 1u);

  using ImageType = itk::Image<float, 3>;
  ImageType::Pointer img = ImageType::New();
  s.PushImage(img.GetPointer());
  CM_CHECK_EQ(s.size(), 2u);
  CM_CHECK(s.back().IsImage());
  CM_CHECK_THROWS(s.PopMesh(), TypeError);

  auto popped_img = s.PopImage();
  CM_CHECK(popped_img.IsNotNull());
  CM_CHECK_EQ(s.size(), 1u);

  s.clear();
  CM_CHECK(s.empty());
  CM_CHECK_THROWS(s.pop(), StackError);
  CM_CHECK_THROWS(s.PopMesh(), StackError);
  CM_CHECK_THROWS(s.back(), StackError);

  return 0;
}
