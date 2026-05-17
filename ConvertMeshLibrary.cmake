# Sources + library targets for ConvertMesh.
#
# Two targets:
#   cmesh_core  — public library: pure functions, throws cmesh::Error.
#                 Installed; downstream projects link against this.
#   cmesh_cli   — CLI library: parser + stack + glue adapters. One public
#                 header (cli/Run.h) — every other header is internal.
#                 Used by the cmesh binary and by the Python wrapper to
#                 share a single parser implementation.

set(CONVERTMESH_INCLUDE_DIRS
  ${CMAKE_CURRENT_SOURCE_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
  ${CMAKE_CURRENT_BINARY_DIR})

# ---------------------------------------------------------------------------
# Core: pure functions
# ---------------------------------------------------------------------------
set(CMESH_CORE_SOURCES
  src/core/MeshIO.cxx
  src/core/ImageIO.cxx
  src/core/FlipPolyFacesFilter.cxx
  src/core/SmoothMesh.cxx
  src/core/DecimateMesh.cxx
  src/core/ComputeNormals.cxx
  src/core/FlipNormals.cxx
  src/core/MeshDiff.cxx
  src/core/ExtractIsoSurface.cxx
  src/core/RasterizeMesh.cxx
  src/core/WarpMesh.cxx
  src/core/SampleImageAtMesh.cxx
  src/core/MergeArrays.cxx)

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
  list(APPEND CMESH_CORE_SOURCES src/impl/vcg/VCGTriMesh.cxx)
endif()

add_library(cmesh_core ${CMESH_CORE_SOURCES})
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
# CLI: parser + glue. One public header (cli/Run.h); internals are private.
# ---------------------------------------------------------------------------
set(CMESH_CLI_SOURCES
  src/cli/Run.cxx
  src/cli/internal/Driver.cxx
  src/cli/internal/Adapters.cxx)

add_library(cmesh_cli ${CMESH_CLI_SOURCES})
foreach(_dir IN LISTS CONVERTMESH_INCLUDE_DIRS)
  target_include_directories(cmesh_cli PUBLIC $<BUILD_INTERFACE:${_dir}>)
endforeach()
target_include_directories(cmesh_cli INTERFACE $<INSTALL_INTERFACE:include>)
target_link_libraries(cmesh_cli PUBLIC cmesh_core)

# Convenience variable for downstream (mirrors c3d's C3D_LINK_LIBRARIES).
set(CMESH_LINK_LIBRARIES cmesh_cli cmesh_core ${ITK_LIBRARIES} ${VTK_LIBRARIES})
