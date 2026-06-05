# ConvertMesh tests. Unit tests link the core library directly (the new
# public API). The CLI end-to-end tests drive the cmesh binary, which
# itself just calls cmesh::cli::Run.

# -----------------------------------------------------------------------
# Unit tests
# -----------------------------------------------------------------------
add_executable(DataStackTest UnitTests/DataStackTest.cxx)
target_include_directories(DataStackTest PRIVATE ${CONVERTMESH_INCLUDE_DIRS})
target_link_libraries(DataStackTest PRIVATE cmesh_cli cmesh_core)
add_test(NAME DataStackTest COMMAND DataStackTest)

add_executable(MeshIOTest UnitTests/MeshIOTest.cxx)
target_include_directories(MeshIOTest PRIVATE ${CONVERTMESH_INCLUDE_DIRS})
target_link_libraries(MeshIOTest PRIVATE cmesh_core)
add_test(NAME MeshIOTest COMMAND MeshIOTest ${CMAKE_CURRENT_BINARY_DIR})

add_executable(IsoSurfaceTest UnitTests/IsoSurfaceTest.cxx)
target_include_directories(IsoSurfaceTest PRIVATE ${CONVERTMESH_INCLUDE_DIRS})
target_link_libraries(IsoSurfaceTest PRIVATE cmesh_core)
add_test(NAME IsoSurfaceTest COMMAND IsoSurfaceTest ${CMAKE_CURRENT_BINARY_DIR})

add_executable(MeshOpsTest UnitTests/MeshOpsTest.cxx)
target_include_directories(MeshOpsTest PRIVATE ${CONVERTMESH_INCLUDE_DIRS})
target_link_libraries(MeshOpsTest PRIVATE cmesh_core)
add_test(NAME MeshOpsTest COMMAND MeshOpsTest ${CMAKE_CURRENT_BINARY_DIR})

add_executable(InteropTest UnitTests/InteropTest.cxx)
target_include_directories(InteropTest PRIVATE ${CONVERTMESH_INCLUDE_DIRS})
target_link_libraries(InteropTest PRIVATE cmesh_core)
add_test(NAME InteropTest COMMAND InteropTest ${CMAKE_CURRENT_BINARY_DIR})

add_executable(AnisotropicTest UnitTests/AnisotropicTest.cxx)
target_include_directories(AnisotropicTest PRIVATE ${CONVERTMESH_INCLUDE_DIRS})
target_link_libraries(AnisotropicTest PRIVATE cmesh_core)
add_test(NAME AnisotropicTest COMMAND AnisotropicTest)

# -----------------------------------------------------------------------
# Fixture-generator helpers
# -----------------------------------------------------------------------
add_executable(MakeSphereImage Fixtures/Tools/MakeSphereImage.cxx)
target_include_directories(MakeSphereImage PRIVATE ${CONVERTMESH_INCLUDE_DIRS})
target_link_libraries(MakeSphereImage PRIVATE cmesh_core)

add_executable(AddTagArray Fixtures/Tools/AddTagArray.cxx)
target_include_directories(AddTagArray PRIVATE ${CONVERTMESH_INCLUDE_DIRS})
target_link_libraries(AddTagArray PRIVATE cmesh_core)

# -----------------------------------------------------------------------
# Cmrep parity tests
# -----------------------------------------------------------------------
set(CMREP_TRUTH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/Fixtures/cmrep-truth)
if(EXISTS ${CMREP_TRUTH_DIR}/sphere.nii.gz)
  add_executable(CmrepParityTest UnitTests/CmrepParityTest.cxx)
  target_include_directories(CmrepParityTest PRIVATE ${CONVERTMESH_INCLUDE_DIRS})
  target_link_libraries(CmrepParityTest PRIVATE cmesh_core)
  add_test(NAME CmrepParityTest
           COMMAND CmrepParityTest ${CMREP_TRUTH_DIR})
else()
  message(STATUS "ConvertMesh: cmrep parity fixtures not found at "
                 "${CMREP_TRUTH_DIR} — run scripts/generate-cmrep-ground-truth.sh "
                 "to populate them. Skipping CmrepParityTest.")
endif()

# -----------------------------------------------------------------------
# CLI end-to-end tests
# -----------------------------------------------------------------------
if(TARGET cmesh)
  add_test(NAME CLIHelp
           COMMAND $<TARGET_FILE:cmesh> --help)

  add_test(NAME CLIVersion
           COMMAND $<TARGET_FILE:cmesh> --version)
  set_tests_properties(CLIVersion PROPERTIES
    PASS_REGULAR_EXPRESSION "cmesh.*version")

  add_test(NAME CLIRoundTrip
           COMMAND ${CMAKE_COMMAND} -E env
             $<TARGET_FILE:cmesh>
             ${CMAKE_CURRENT_BINARY_DIR}/cube.vtp
             -o ${CMAKE_CURRENT_BINARY_DIR}/cube-roundtrip.stl)
  set_tests_properties(CLIRoundTrip PROPERTIES
    DEPENDS MeshIOTest
    REQUIRED_FILES ${CMAKE_CURRENT_BINARY_DIR}/cube.vtp)

  add_test(NAME CLIIsoSurfacePipeline
           COMMAND ${CMAKE_COMMAND} -E env
             $<TARGET_FILE:cmesh>
             -verbose
             ${CMAKE_CURRENT_BINARY_DIR}/sphere.nii.gz
             -extract-isosurface 0.5 --clean
             -smooth-mesh 3 0.2
             -decimate 0.3
             -compute-normals
             -o ${CMAKE_CURRENT_BINARY_DIR}/sphere-pipeline.vtp)
  set_tests_properties(CLIIsoSurfacePipeline PROPERTIES
    DEPENDS IsoSurfaceTest
    REQUIRED_FILES ${CMAKE_CURRENT_BINARY_DIR}/sphere.nii.gz)

  add_test(NAME CLIMeshDiffZero
           COMMAND ${CMAKE_COMMAND} -E env
             $<TARGET_FILE:cmesh>
             ${CMAKE_CURRENT_BINARY_DIR}/cube.vtp
             -meshdiff ${CMAKE_CURRENT_BINARY_DIR}/cube.vtp)
  set_tests_properties(CLIMeshDiffZero PROPERTIES
    DEPENDS MeshIOTest
    PASS_REGULAR_EXPRESSION "mean=0")

  add_test(NAME CLIRasterize
           COMMAND ${CMAKE_COMMAND} -E env
             $<TARGET_FILE:cmesh>
             ${CMAKE_CURRENT_BINARY_DIR}/sphere-mesh.vtp
             -rasterize --spacing 0.5 0.5 0.5 --margin 1.0
             -o ${CMAKE_CURRENT_BINARY_DIR}/sphere-raster.nii.gz)
  set_tests_properties(CLIRasterize PROPERTIES DEPENDS IsoSurfaceTest)

  add_test(NAME CLISampleImage
           COMMAND ${CMAKE_COMMAND} -E env
             $<TARGET_FILE:cmesh>
             ${CMAKE_CURRENT_BINARY_DIR}/sphere-mesh.vtp
             -push-image ${CMAKE_CURRENT_BINARY_DIR}/sphere.nii.gz
             -sample-image Surface
             -o ${CMAKE_CURRENT_BINARY_DIR}/sphere-sampled.vtp)
  set_tests_properties(CLISampleImage PROPERTIES DEPENDS IsoSurfaceTest)

  add_test(NAME CLIWarpMesh
           COMMAND ${CMAKE_COMMAND} -E env
             $<TARGET_FILE:cmesh>
             ${CMAKE_CURRENT_BINARY_DIR}/sphere-mesh.vtp
             -warp-mesh ${CMAKE_CURRENT_BINARY_DIR}/warp-tx.nii.gz
             -o ${CMAKE_CURRENT_BINARY_DIR}/sphere-warped.vtp)
  set_tests_properties(CLIWarpMesh PROPERTIES DEPENDS InteropTest)
endif()
