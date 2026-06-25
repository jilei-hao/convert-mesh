#ifndef CONVERTMESH_CORE_FLIP_POLY_FACES_FILTER_H
#define CONVERTMESH_CORE_FLIP_POLY_FACES_FILTER_H

#include <vtkPolyDataAlgorithm.h>

/**
 * Reverse triangle winding while preserving point/cell data arrays. Lifted
 * from ITK-SNAP's VTKMeshPipeline.cxx so cmesh can share the implementation.
 *
 * Faster than vtkPolyDataNormals(FlipNormals=On) because no normals are
 * touched; if you also need normal vectors recomputed, run a normals filter
 * afterward.
 */
class vtkFlipPolyFaces : public vtkPolyDataAlgorithm
{
public:
  vtkTypeMacro(vtkFlipPolyFaces, vtkPolyDataAlgorithm);
  void PrintSelf(std::ostream &os, vtkIndent indent) override;

  static vtkFlipPolyFaces *New();

  vtkSetMacro(FlipFaces, vtkTypeBool);
  vtkGetMacro(FlipFaces, vtkTypeBool);
  vtkBooleanMacro(FlipFaces, vtkTypeBool);

protected:
  vtkFlipPolyFaces();
  ~vtkFlipPolyFaces() override = default;

  vtkTypeBool FlipFaces;

  int RequestData(vtkInformation        *request,
                  vtkInformationVector **inputVector,
                  vtkInformationVector  *outputVector) override;

private:
  vtkFlipPolyFaces(const vtkFlipPolyFaces &) = delete;
  void operator=(const vtkFlipPolyFaces &) = delete;
};

#endif
