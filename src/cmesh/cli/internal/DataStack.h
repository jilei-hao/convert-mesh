#ifndef CONVERTMESH_CLI_DATA_STACK_H
#define CONVERTMESH_CLI_DATA_STACK_H

#include "cmesh/cli/internal/DataItem.h"
#include "cmesh/core/Error.h"

#include <vector>

namespace cmesh
{
namespace cli
{

class DataStack
{
public:
  using StorageType = std::vector<DataItem>;
  using iterator = StorageType::iterator;
  using const_iterator = StorageType::const_iterator;

  void push(const DataItem &d) { m_Stack.push_back(d); }
  void pop()
  {
    if(m_Stack.empty()) throw StackError();
    m_Stack.pop_back();
  }
  const DataItem &back() const
  {
    if(m_Stack.empty()) throw StackError();
    return m_Stack.back();
  }
  DataItem &back()
  {
    if(m_Stack.empty()) throw StackError();
    return m_Stack.back();
  }

  DataItem::MeshPtr PopMesh()
  {
    if(m_Stack.empty()) throw StackError();
    const DataItem &top = m_Stack.back();
    if(!top.IsMesh()) throw TypeError("mesh", top.KindName());
    DataItem::MeshPtr out = top.mesh;
    m_Stack.pop_back();
    return out;
  }
  DataItem::ImagePtr PopImage()
  {
    if(m_Stack.empty()) throw StackError();
    const DataItem &top = m_Stack.back();
    if(!top.IsImage()) throw TypeError("image", top.KindName());
    DataItem::ImagePtr out = top.image;
    m_Stack.pop_back();
    return out;
  }
  DataItem::UGridPtr PopUGrid()
  {
    if(m_Stack.empty()) throw StackError();
    const DataItem &top = m_Stack.back();
    if(!top.IsUGrid()) throw TypeError("ugrid", top.KindName());
    DataItem::UGridPtr out = top.ugrid;
    m_Stack.pop_back();
    return out;
  }

  void PushMesh(vtkPolyData *m)          { m_Stack.push_back(DataItem::FromMesh(m)); }
  void PushUGrid(vtkUnstructuredGrid *g) { m_Stack.push_back(DataItem::FromUGrid(g)); }
  void PushImage(itk::ImageBase<3> *i)   { m_Stack.push_back(DataItem::FromImage(i)); }

  std::size_t size() const { return m_Stack.size(); }
  bool        empty() const { return m_Stack.empty(); }
  void        clear() { m_Stack.clear(); }

  DataItem       &operator[](std::size_t i)
  {
    if(i >= m_Stack.size()) throw StackError();
    return m_Stack[i];
  }
  const DataItem &operator[](std::size_t i) const
  {
    if(i >= m_Stack.size()) throw StackError();
    return m_Stack[i];
  }

  iterator       begin()       { return m_Stack.begin(); }
  const_iterator begin() const { return m_Stack.begin(); }
  iterator       end()         { return m_Stack.end(); }
  const_iterator end()   const { return m_Stack.end(); }

private:
  StorageType m_Stack;
};

} // namespace cli
} // namespace cmesh

#endif
