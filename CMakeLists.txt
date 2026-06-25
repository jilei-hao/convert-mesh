cmake_minimum_required(VERSION 3.15)
cmake_policy(SET CMP0077 NEW)

project(ConvertMesh VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Versioning — derived from project(VERSION); Version.h is configured from
# these in ConvertMeshLibrary.cmake.
set(CONVERTMESH_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CONVERTMESH_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CONVERTMESH_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CONVERTMESH_VERSION_FULL  ${PROJECT_VERSION})

# ---------------------------------------------------------------------------
# Dependency component lists — single source of truth, reused by the
# standalone find_package calls below and by the installed package config
# (find_dependency in Config.cmake.in).
# ---------------------------------------------------------------------------
set(CONVERTMESH_ITK_COMPONENTS
  ITKCommon
  ITKIOImageBase
  ITKIONIFTI
  ITKIONRRD
  ITKIOGIPL
  ITKIOMeta
  ITKIOPNG
  ITKIOJPEG
  ITKIOTIFF
  ITKIOVTK
  ITKImageGrid
  ITKTransform
  ITKVTK)

set(CONVERTMESH_VTK_COMPONENTS
  CommonCore
  CommonDataModel
  CommonExecutionModel
  CommonTransforms
  FiltersCore
  FiltersGeneral
  FiltersGeometry
  FiltersModeling
  FiltersSources
  IOGeometry
  IOImage
  IOLegacy
  IOPLY
  IOXML
  ImagingCore
  ImagingGeneral
  ImagingStencil)

# ---------------------------------------------------------------------------
# Build-as-subproject support (mirrors c3d / cmrep conventions)
# ---------------------------------------------------------------------------
option(CONVERTMESH_BUILD_AS_SUBPROJECT
       "Build ConvertMesh as a sub-project of a larger project" OFF)
mark_as_advanced(CONVERTMESH_BUILD_AS_SUBPROJECT)

option(CONVERTMESH_BUILD_VCG "Build VCG-backed adapters" OFF)
option(CONVERTMESH_BUILD_TETGEN "Build TetGen-backed adapters" OFF)
option(CONVERTMESH_BUILD_TESTING "Build the ConvertMesh test suite" ON)

if(NOT CONVERTMESH_BUILD_AS_SUBPROJECT)
  # Top-level build: find ITK + VTK ourselves
  find_package(ITK 5.4 REQUIRED COMPONENTS ${CONVERTMESH_ITK_COMPONENTS})
  include(${ITK_USE_FILE})

  find_package(VTK 9 REQUIRED COMPONENTS ${CONVERTMESH_VTK_COMPONENTS})

  set(CONVERTMESH_BUILD_CLI ON)
  set(CONVERTMESH_INSTALL_CLI ON)
else()
  option(CONVERTMESH_SUBPROJECT_BUILD_CLI "Build cmesh executable inside subproject" OFF)
  set(CONVERTMESH_BUILD_CLI ${CONVERTMESH_SUBPROJECT_BUILD_CLI})
  set(CONVERTMESH_INSTALL_CLI OFF)
endif()

# ---------------------------------------------------------------------------
# Library target(s)
# ---------------------------------------------------------------------------
include(${CMAKE_CURRENT_SOURCE_DIR}/ConvertMeshLibrary.cmake)

# ---------------------------------------------------------------------------
# CLI executable
# ---------------------------------------------------------------------------
if(CONVERTMESH_BUILD_CLI)
  add_executable(cmesh ConvertMeshMain.cxx)
  target_include_directories(cmesh PRIVATE ${CONVERTMESH_INCLUDE_DIRS})
  target_link_libraries(cmesh PRIVATE ConvertMesh::cmesh_cli)
  if(COMMAND vtk_module_autoinit)
    vtk_module_autoinit(TARGETS cmesh MODULES ${VTK_LIBRARIES})
  endif()
endif()

# ---------------------------------------------------------------------------
# Install + export (top-level builds only)
# ---------------------------------------------------------------------------
if(NOT CONVERTMESH_BUILD_AS_SUBPROJECT AND CONVERTMESH_INSTALL_CLI)
  include(GNUInstallDirs)
  set(CLI_INSTALL_PATH ${CMAKE_INSTALL_BINDIR})
  set(LIB_INSTALL_PATH ${CMAKE_INSTALL_LIBDIR})
  set(INC_INSTALL_PATH ${CMAKE_INSTALL_INCLUDEDIR})

  install(TARGETS cmesh DESTINATION ${CLI_INSTALL_PATH} COMPONENT Runtime)
  install(TARGETS cmesh_core cmesh_cli
          DESTINATION ${LIB_INSTALL_PATH}
          EXPORT ConvertMeshTargets)
  # Public core headers, installed under a cmesh/ prefix so downstream
  # include paths are namespaced: #include "cmesh/core/MeshIO.h"
  install(DIRECTORY src/cmesh/core/
          DESTINATION ${INC_INSTALL_PATH}/cmesh/core
          FILES_MATCHING PATTERN "*.h")
  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/cmesh/core/Version.h
          DESTINATION ${INC_INSTALL_PATH}/cmesh/core)
  # cli/Run.h is the ONLY public header from cmesh_cli
  install(FILES src/cmesh/cli/Run.h DESTINATION ${INC_INSTALL_PATH}/cmesh/cli)

  install(EXPORT ConvertMeshTargets
          FILE ConvertMeshTargets.cmake
          NAMESPACE ConvertMesh::
          DESTINATION ${LIB_INSTALL_PATH}/cmake/ConvertMesh)

  include(CMakePackageConfigHelpers)
  set(ConvertMesh_INCLUDE_DIRS "include")
  # find_dependency in Config.cmake.in needs space-separated component lists
  string(REPLACE ";" " " CONVERTMESH_ITK_COMPONENTS_STR "${CONVERTMESH_ITK_COMPONENTS}")
  string(REPLACE ";" " " CONVERTMESH_VTK_COMPONENTS_STR "${CONVERTMESH_VTK_COMPONENTS}")
  configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/Config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/ConvertMeshConfig.cmake"
    INSTALL_DESTINATION ${LIB_INSTALL_PATH}/cmake/ConvertMesh
    PATH_VARS ConvertMesh_INCLUDE_DIRS)
  write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/ConvertMeshConfigVersion.cmake"
    VERSION "${CONVERTMESH_VERSION_FULL}"
    COMPATIBILITY AnyNewerVersion)
  install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/ConvertMeshConfig.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/ConvertMeshConfigVersion.cmake
    DESTINATION ${LIB_INSTALL_PATH}/cmake/ConvertMesh)
endif()

# ---------------------------------------------------------------------------
# Tests
# ---------------------------------------------------------------------------
if(NOT CONVERTMESH_BUILD_AS_SUBPROJECT AND CONVERTMESH_BUILD_TESTING)
  enable_testing()
  include(CTest)
  add_subdirectory(Testing)
endif()
