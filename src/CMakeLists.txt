cmake_minimum_required(VERSION 3.6)
project(mcbopomofo VERSION 2.4)

find_package(fmt REQUIRED)
find_package(Gettext REQUIRED)
# for McBopomofoApi
find_package(GLIB2 REQUIRED)

add_subdirectory(Engine/gramambular2)
add_subdirectory(Engine/Mandarin)

set(MCBOPOMOFO_LIB_SOURCES
 api/McBopomofoApi.cpp
 KeyHandler.cpp
 UTF8Helper.cpp
 Engine/AssociatedPhrases.h
 Engine/AssociatedPhrases.cpp
 Engine/KeyValueBlobReader.cpp 
 Engine/McBopomofoLM.cpp
 Engine/ParselessLM.cpp
 Engine/ParselessPhraseDB.cpp
 Engine/PhraseReplacementMap.cpp
 Engine/UserOverrideModel.cpp
 Engine/UserPhrasesLM.cpp)

# https://stackoverflow.com/questions/26549137/shared-library-on-linux-and-fpic-error
set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

# Setup some compiler option that is generally useful and compatible with Fcitx 5 (C++17)
# include("${FCITX_INSTALL_CMAKECONFIG_DIR}/Fcitx5Utils/Fcitx5CompilerSettings.cmake")
include("${CMAKE_SOURCE_DIR}/CompilerSettings.cmake")

add_library(McBopomofoLib STATIC ${MCBOPOMOFO_LIB_SOURCES})
target_compile_options(McBopomofoLib PRIVATE -Wno-unknown-pragmas)
if (TARGET fmt::fmt-header-only)
  target_link_libraries(McBopomofoLib PRIVATE fmt::fmt-header-only)
else()
  # old fmt libraries does not have cmake scripts for header-only usage
  target_link_libraries(McBopomofoLib PRIVATE fmt::fmt)
endif()
target_link_libraries(McBopomofoLib PRIVATE gramambular2_lib)
target_link_libraries(McBopomofoLib PRIVATE MandarinLib)
# target_compile_definitions(McBopomofoLib PRIVATE FCITX_GETTEXT_DOMAIN=\"fcitx5-mcbopomofo\")
# unused; may change in the future
target_compile_definitions(McBopomofoLib PRIVATE GETTEXT_DOMAIN=\"fcitx5-mcbopomofo\")

include_directories(${GLIB2_INCLUDE_DIRS})
include_directories(Engine)
include_directories(Engine/Gramambular)
include_directories(Engine/Mandarin)

set (MCBOPOMOFO_CORE_DIR ${CMAKE_CURRENT_BINARY_DIR}/tmp)
file(MAKE_DIRECTORY ${MCBOPOMOFO_CORE_DIR})

# https://stackoverflow.com/a/37941544
add_custom_target(combine_static_libs ALL
  COMMAND ar -x $<TARGET_FILE:McBopomofoLib>
  COMMAND ar -x $<TARGET_FILE:gramambular2_lib>
  COMMAND ar -x $<TARGET_FILE:MandarinLib>
  COMMAND ar -qcs ../libMcBopomofoCore.a *.o
  WORKING_DIRECTORY ${MCBOPOMOFO_CORE_DIR}
  DEPENDS McBopomofoLib gramambular2_lib MandarinLib)
set_target_properties(combine_static_libs
  PROPERTIES IMPORTED_LOCATION ${CMAKE_CURRENT_BINARY_DIR}/libMcBopomofoCore.a)


if (ENABLE_TEST)
    # Let CMake fetch Google Test for us.
    # https://github.com/google/googletest/tree/main/googletest#incorporating-into-an-existing-cmake-project
    include(FetchContent)

    FetchContent_Declare(
            googletest
            # Specify the commit you depend on and update it regularly.
            URL https://github.com/google/googletest/archive/609281088cfefc76f9d0ce82e1ff6c30cc3591e5.zip
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)

    include(GoogleTest)

    # Test target declarations.
    add_executable(McBopomofoTest
            KeyHandlerTest.cpp)
    target_compile_options(McBopomofoTest PRIVATE -Wno-unknown-pragmas)
    target_link_libraries(McBopomofoTest PRIVATE gtest_main gmock_main McBopomofoLib)

    configure_file(../data/data.txt mcbopomofo-test-data.txt)

    gtest_discover_tests(McBopomofoTest)

    add_custom_target(
            runTest
            COMMAND ${CMAKE_CURRENT_BINARY_DIR}/McBopomofoTest
    )
    add_dependencies(runTest McBopomofoTest)
endif ()
