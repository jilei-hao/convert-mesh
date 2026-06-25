#include "cmesh/core/FlipPolyFacesFilter.h"

#include <vtkCellArray.h>
#include <vtkCellArrayIterator.h>
#include <vtkCellData.h>
#include <vtkIdList.h>
#include <vtkInformation.h>
#include <vtkInformationVector.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkSmartPointer.h>

vtkStandardNewMacro(vtkFlipPolyFaces);

vtkFlipPolyFaces::vtkFlipPolyFaces()
{
  this->FlipFaces = 0;
}

void vtkFlipPolyFaces::PrintSelf(std::ostream &os, vtkIndent indent)
{
  this->Superclass::PrintSelf(os, indent);
  os << indent << "Flip Faces: " << (this->FlipFaces ? "On\n" : "Off\n");
}

int vtkFlipPolyFaces::RequestData(vtkInformation *,
                                   vtkInformationVector **inputVector,
                                   vtkInformationVector *outputVector)
{
  vtkInformation *inInfo  = inputVector[0]->GetInformationObject(0);
  vtkInformation *outInfo = outputVector->GetInformationObject(0);
  vtkPolyData *input  = vtkPolyData::SafeDownCast(inInfo->Get(vtkDataObject::DATA_OBJECT()));
  vtkPolyData *output = vtkPolyData::SafeDownCast(outInfo->Get(vtkDataObject::DATA_OBJECT()));

  output->CopyStructure(input);
  output->GetPointData()->PassData(input->GetPointData());

  if(!this->FlipFaces)
  {
    output->GetCellData()->PassData(input->GetCellData());
    return 1;
  }

  auto idList = vtkSmartPointer<vtkIdList>::New();
  auto trg    = vtkSmartPointer<vtkCellArray>::New();
  vtkCellArray *src = input->GetPolys();
  trg->AllocateEstimate(src->GetNumberOfCells(), src->GetMaxCellSize());

  for(auto it = src->NewIterator(); !it->IsDoneWithTraversal(); it->GoToNextCell())
  {
    it->GetCurrentCell(idList);
    vtkIdType n = idList->GetNumberOfIds();
    if(n >= 3)
    {
      for(vtkIdType i = 0; i < n / 2; ++i)
      {
        vtkIdType tmp = idList->GetId(i);
        idList->SetId(i, idList->GetId(n - i - 1));
        idList->SetId(n - i - 1, tmp);
      }
    }
    trg->InsertNextCell(idList);
  }
  output->SetPolys(trg);
  output->GetCellData()->PassData(input->GetCellData());
  return 1;
}
