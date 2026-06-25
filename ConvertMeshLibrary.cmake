# Sources + library targets for ConvertMesh.
#
# Two targets (also exposed as ConvertMesh::cmesh_core / ConvertMesh::cmesh_cli
# ALIAS targets so subproject and installed-package consumers link the same
# names):
#   cmesh_core  — public library: pure functions, throws cmesh::Error.
#                 Installed; downstream projects link against this.
#   cmesh_cli   — CLI library: parser + stack + glue adapters. One public
#                 header (cmesh/cli/Run.h) — every other header is internal.
#                 Used by the cmesh binary and by the Python wrapper to
#                 share a single parser implementation.

set(CONVERTMESH_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}/generated)

# Generated version header (#include "cmesh/core/Version.h"); generated/ is
# on the include path above. (Not directly under the binary dir, where the
# cmesh executable occupies the "cmesh" name.)
configure_file(
  ${CMAKE_CURRENT_SOURCE_DIR}/src/cmesh/core/Version.h.in
  ${CMAKE_CURRENT_BINARY_DIR}/generated/cmesh/core/Version.h
  @ONLY)

# ---------------------------------------------------------------------------
# Core: pure functions
# ---------------------------------------------------------------------------
set(CMESH_CORE_SOURCES
  src/cmesh/core/MeshIO.cxx
  src/cmesh/core/ImageIO.cxx
  src/cmesh/core/FlipPolyFacesFilter.cxx
  src/cmesh/core/SmoothMesh.cxx
  src/cmesh/core/DecimateMesh.cxx
  src/cmesh/core/ComputeNormals.cxx
  src/cmesh/core/FlipNormals.cxx
  src/cmesh/core/MeshDiff.cxx
  src/cmesh/core/ExtractIsoSurface.cxx
  src/cmesh/core/RasterizeMesh.cxx
  src/cmesh/core/WarpMesh.cxx
  src/cmesh/core/SampleImageAtMesh.cxx
  src/cmesh/core/MergeArrays.cxx)

# ---------------------------------------------------------------------------
# Optional VCG backend (decimation parity with cmrep's mesh_decimate_vcg)
# ---------------------------------------------------------------------------
if(CONVERTMESH_BUILD_VCG)
  if(NOT VCGLIB_DIR)
    find_path(VCGLIB_DIR
      NAMES vcg/simplex/vertex/base.h
      DOC "Path to a vcglib checkout (root that contains vcg/, wrap/, eigenlib/)")
  endif()
  if(NOT VCGLIB_DIR OR NOT EXISTS "${VCGLIB_DIR}/vcg/simplex/vertex/base.h")
    message(FATAL_ERROR
      "CONVERTMESH_BUILD_VCG=ON but VCGLIB_DIR is not set to a valid vcglib "
      "checkout. Pass -DVCGLIB_DIR=/path/to/vcglib (the same path cmrep uses; "
      "scripts/build-cmrep.sh clones it to lib/vcglib).")
  endif()
  message(STATUS "ConvertMesh: VCG support enabled, VCGLIB_DIR=${VCGLIB_DIR}")
  list(APPEND CMESH_CORE_SOURCES src/cmesh/impl/vcg/VCGTriMesh.cxx)
endif()

# STATIC: no symbol-export story for shared builds yet, and the planned
# Python wheel wants statically-linked ITK/VTK components anyway.
add_library(cmesh_core STATIC ${CMESH_CORE_SOURCES})
add_library(ConvertMesh::cmesh_core ALIAS cmesh_core)
foreach(_dir IN LISTS CONVERTMESH_INCLUDE_DIRS)
  target_include_directories(cmesh_core PUBLIC $<BUILD_INTERFACE:${_dir}>)
endforeach()
target_include_directories(cmesh_core INTERFACE $<INSTALL_INTERFACE:include>)
target_link_libraries(cmesh_core PUBLIC ${ITK_LIBRARIES} ${VTK_LIBRARIES})

if(CONVERTMESH_BUILD_VCG)
  # vcglib pulls in its own bundled Eigen via <Eigen/...> includes at
  # ${VCGLIB_DIR}/eigenlib. Kept PRIVATE so downstream consumers are
  # unaffected.
  target_include_directories(cmesh_core PRIVATE
    ${VCGLIB_DIR}
    ${VCGLIB_DIR}/eigenlib)
  target_compile_definitions(cmesh_core PUBLIC CONVERTMESH_HAVE_VCG)
endif()

# ---------------------------------------------------------------------------
# CLI: parser + glue. One public header (cmesh/cli/Run.h); internals private.
# ---------------------------------------------------------------------------
set(CMESH_CLI_SOURCES
  src/cmesh/cli/Run.cxx
  src/cmesh/cli/internal/Driver.cxx
  src/cmesh/cli/internal/Adapters.cxx)

add_library(cmesh_cli STATIC ${CMESH_CLI_SOURCES})
add_library(ConvertMesh::cmesh_cli ALIAS cmesh_cli)
foreach(_dir IN LISTS CONVERTMESH_INCLUDE_DIRS)
  target_include_directories(cmesh_cli PUBLIC $<BUILD_INTERFACE:${_dir}>)
endforeach()
target_include_directories(cmesh_cli INTERFACE $<INSTALL_INTERFACE:include>)
target_link_libraries(cmesh_cli PUBLIC cmesh_core)

# VTK factory auto-initialization: required so factory-based VTK mechanisms
# (IO factories, future rendering) work in any TU of these libraries.
if(COMMAND vtk_module_autoinit)
  vtk_module_autoinit(TARGETS cmesh_core cmesh_cli MODULES ${VTK_LIBRARIES})
endif()

# Convenience variable for downstream (mirrors c3d's C3D_LINK_LIBRARIES).
set(CMESH_LINK_LIBRARIES cmesh_cli cmesh_core ${ITK_LIBRARIES} ${VTK_LIBRARIES})
