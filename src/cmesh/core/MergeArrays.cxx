#include "cmesh/core/MergeArrays.h"

#include "cmesh/core/Error.h"

#include <vtkCellData.h>
#include <vtkDataArray.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

#include <sstream>

namespace cmesh
{

vtkSmartPointer<vtkPolyData>
MergeArrays(vtkPolyData *dest, vtkPolyData *source,
            const MergeArraysParams &p,
            ProgressFn /*progress*/, AbortToken abort)
{
  if(!dest)   throw AlgorithmError("MergeArrays: null destination mesh");
  if(!source) throw AlgorithmError("MergeArrays: null source mesh");
  abort.ThrowIfRequested();

  vtkDataSetAttributes *src_attr = p.cell_data
      ? static_cast<vtkDataSetAttributes *>(source->GetCellData())
      : static_cast<vtkDataSetAttributes *>(source->GetPointData());

  vtkDataArray *src_arr = src_attr->GetArray(p.array_name.c_str());
  if(!src_arr)
    throw AlgorithmError("MergeArrays: '" + p.array_name + "' not found in source mesh");

  vtkIdType expected = p.cell_data ? dest->GetNumberOfCells()
                                   : dest->GetNumberOfPoints();
  if(src_arr->GetNumberOfTuples() != expected)
  {
    std::ostringstream os;
    os << "MergeArrays: '" << p.array_name << "' has "
       << src_arr->GetNumberOfTuples()
       << " tuples but destination has " << expected
       << (p.cell_data ? " cells" : " points");
    throw AlgorithmError(os.str());
  }

  auto copy = vtkSmartPointer<vtkDataArray>::Take(src_arr->NewInstance());
  copy->DeepCopy(src_arr);
  if(!p.rename_to.empty())
    copy->SetName(p.rename_to.c_str());

  auto out = vtkSmartPointer<vtkPolyData>::New();
  out->ShallowCopy(dest);
  vtkDataSetAttributes *dst_attr = p.cell_data
      ? static_cast<vtkDataSetAttributes *>(out->GetCellData())
      : static_cast<vtkDataSetAttributes *>(out->GetPointData());
  dst_attr->AddArray(copy);

  return out;
}

} // namespace cmesh
