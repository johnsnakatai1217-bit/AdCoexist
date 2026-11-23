cmake_minimum_required(VERSION 3.23)

project(AdCoexist VERSION 1.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# RED4ext.SDK (git submodule in third_party)
add_subdirectory(third_party/RED4ext.SDK EXCLUDE_FROM_ALL)

# The plugin (AdCoexist.dll)
add_library(AdCoexist SHARED
  plugin/src/Main.cpp
  plugin/src/Coexist.cpp
  plugin/src/AdCatalog.cpp
  plugin/src/Archive.cpp
  plugin/src/Cr2w.cpp
  plugin/src/Manifest.cpp
  plugin/src/Naming.cpp
  plugin/src/Oodle.cpp
  plugin/src/Report.cpp
  plugin/src/Variant.cpp
)
# version: reads the versions of RED4ext and TweakXL for the report
target_link_libraries(AdCoexist PRIVATE RED4ext::SDK version)
target_compile_definitions(AdCoexist PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX
  # The version is set only here (project VERSION). tools/package.ps1 reads it from here too
  ADCOEXIST_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
  ADCOEXIST_VERSION_MINOR=${PROJECT_VERSION_MINOR}
  ADCOEXIST_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)
# Read the sources as UTF-8
target_compile_options(AdCoexist PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# Tests that run without the game (tools/package.ps1 runs them with ctest after the build)
enable_testing()
add_executable(AdCoexistTests
  tests/UnitTests.cpp
  plugin/src/Archive.cpp
  plugin/src/Cr2w.cpp
  plugin/src/Manifest.cpp
  plugin/src/Naming.cpp
  plugin/src/Oodle.cpp
  plugin/src/Report.cpp
  plugin/src/Variant.cpp
)
# Only the RED4ext.SDK headers are used (to compare with the ResourcePath hash)
target_include_directories(AdCoexistTests PRIVATE plugin/src third_party/RED4ext.SDK/include)
target_compile_definitions(AdCoexistTests PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(AdCoexistTests PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_test(NAME UnitTests COMMAND AdCoexistTests)

# Development tool that builds the ad catalog (used by tools/catalog/build-catalog.ps1, not part of the release)
add_executable(AdCoexistCatalogTool EXCLUDE_FROM_ALL
  tools/catalog/CatalogTool.cpp
  plugin/src/Archive.cpp
  plugin/src/Cr2w.cpp
  plugin/src/Naming.cpp
  plugin/src/Oodle.cpp
)
target_include_directories(AdCoexistCatalogTool PRIVATE plugin/src)
target_compile_definitions(AdCoexistCatalogTool PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(AdCoexistCatalogTool PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)

# Development tool that builds test replacer mods (used by tools/testmods/build-testmods.ps1, not part of the release).
# Only added when tools/testmods exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools/testmods/TestModTool.cpp")
  add_executable(AdCoexistTestModTool EXCLUDE_FROM_ALL
    tools/testmods/TestModTool.cpp
    plugin/src/Archive.cpp
    plugin/src/Naming.cpp
  )
  target_include_directories(AdCoexistTestModTool PRIVATE plugin/src)
  target_compile_definitions(AdCoexistTestModTool PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
  target_compile_options(AdCoexistTestModTool PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
endif()

