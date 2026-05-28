cmake_minimum_required(VERSION 3.15)
if(COMMAND cmake_policy)
  cmake_policy(SET CMP0003 NEW)
  cmake_policy(SET CMP0077 NEW)
endif()

project(ConvertMesh)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Versioning
set(CONVERTMESH_VERSION_MAJOR 0)
set(CONVERTMESH_VERSION_MINOR 1)
set(CONVERTMESH_VERSION_PATCH 0)
set(CONVERTMESH_VERSION_FULL
    "${CONVERTMESH_VERSION_MAJOR}.${CONVERTMESH_VERSION_MINOR}.${CONVERTMESH_VERSION_PATCH}")

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
  find_package(ITK REQUIRED COMPONENTS
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
  include(${ITK_USE_FILE})

  find_package(VTK 9 REQUIRED COMPONENTS
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
  target_link_libraries(cmesh PRIVATE cmesh_cli)
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
  # Public core headers
  install(DIRECTORY src/core/
          DESTINATION ${INC_INSTALL_PATH}/core
          FILES_MATCHING PATTERN "*.h")
  # cli/Run.h is the ONLY public header from cmesh_cli
  install(FILES src/cli/Run.h DESTINATION ${INC_INSTALL_PATH}/cli)

  install(EXPORT ConvertMeshTargets
          FILE ConvertMeshTargets.cmake
          NAMESPACE ConvertMesh::
          DESTINATION ${LIB_INSTALL_PATH}/cmake/ConvertMesh)

  include(CMakePackageConfigHelpers)
  set(ConvertMesh_INCLUDE_DIRS "include")
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
