
INCLUDE(${CMAKE_CURRENT_SOURCE_DIR}/../test_macros.cmake)

INCLUDE_DIRECTORIES(${CMAKE_BINARY_DIR}/fem/src)

CONFIGURE_FILE(ELMERSOLVER_STARTINFO ELMERSOLVER_STARTINFO COPYONLY)
CONFIGURE_FILE(Ldomain.sif Ldomain.sif COPYONLY)

ADD_ELMER_TEST(Adaptivity)

