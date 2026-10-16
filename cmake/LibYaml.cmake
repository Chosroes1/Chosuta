# SPDX-License-Identifier: GPL-3.0-or-later
option(CHOSUTA_DOWNLOAD_DEPENDENCIES "Fetch pinned parser/compression dependency sources (explicit network opt-in)" OFF)
set(CHOSUTA_LIBYAML_SOURCE_DIR "" CACHE PATH "Existing LibYAML 0.2.5 source tree; build statically")
set(CHOSUTA_LIBYAML_PROVIDER "system")
if(CHOSUTA_LIBYAML_SOURCE_DIR OR CHOSUTA_DOWNLOAD_DEPENDENCIES)
 if(CHOSUTA_LIBYAML_SOURCE_DIR)
  get_filename_component(libyaml_source "${CHOSUTA_LIBYAML_SOURCE_DIR}" ABSOLUTE)
 else()
  include(FetchContent)
  FetchContent_Declare(chosuta_libyaml
   URL https://github.com/yaml/libyaml/releases/download/0.2.5/yaml-0.2.5.tar.gz
   URL_HASH SHA256=c642ae9b75fee120b2d96c712538bd2cf283228d2337df2cf2988e3c02678ef4
   DOWNLOAD_EXTRACT_TIMESTAMP TRUE
   SOURCE_SUBDIR chosuta-unused-upstream-build)
  FetchContent_MakeAvailable(chosuta_libyaml)
  set(libyaml_source "${chosuta_libyaml_SOURCE_DIR}")
 endif()
 if(NOT EXISTS "${libyaml_source}/include/yaml.h" OR NOT EXISTS "${libyaml_source}/License")
  message(FATAL_ERROR "Expected the complete LibYAML 0.2.5 source tree: ${libyaml_source}")
 endif()
 file(READ "${libyaml_source}/CMakeLists.txt" libyaml_cmake)
 foreach(component MAJOR MINOR PATCH)
  if(component STREQUAL "MAJOR")
   set(expected 0)
  elseif(component STREQUAL "MINOR")
   set(expected 2)
  else()
   set(expected 5)
  endif()
  if(NOT libyaml_cmake MATCHES "set[ \t]*\\([ \t]*YAML_VERSION_${component}[ \t]+${expected}[ \t]*\\)")
   message(FATAL_ERROR "CHOSUTA_LIBYAML_SOURCE_DIR must contain LibYAML 0.2.5")
  endif()
 endforeach()
 enable_language(C)
 # Build only the upstream library; avoid its global options, tests and install rules.
 add_library(chosuta_yaml STATIC
  "${libyaml_source}/src/api.c" "${libyaml_source}/src/dumper.c"
  "${libyaml_source}/src/emitter.c" "${libyaml_source}/src/loader.c"
  "${libyaml_source}/src/parser.c" "${libyaml_source}/src/reader.c"
  "${libyaml_source}/src/scanner.c" "${libyaml_source}/src/writer.c")
 set_target_properties(chosuta_yaml PROPERTIES AUTOMOC OFF AUTORCC OFF POSITION_INDEPENDENT_CODE ON)
 target_include_directories(chosuta_yaml PUBLIC "${libyaml_source}/include")
 target_compile_definitions(chosuta_yaml PUBLIC YAML_DECLARE_STATIC PRIVATE
  YAML_VERSION_MAJOR=0 YAML_VERSION_MINOR=2 YAML_VERSION_PATCH=5 YAML_VERSION_STRING="0.2.5")
 if(MSVC)
  target_compile_definitions(chosuta_yaml PRIVATE _CRT_SECURE_NO_WARNINGS)
 endif()
 set(CHOSUTA_LIBYAML_PROVIDER "source-static-0.2.5")
 # Ship the exact source used with static binaries, including upstream license/build files.
 if(WIN32)
  set(libyaml_source_destination "dependency-sources/libyaml-0.2.5")
 else()
  set(libyaml_source_destination "share/chosuta/dependency-sources/libyaml-0.2.5")
 endif()
 install(DIRECTORY "${libyaml_source}/" DESTINATION "${libyaml_source_destination}"
  PATTERN ".git" EXCLUDE PATTERN ".github" EXCLUDE)
else()
 if(WIN32)
  message(FATAL_ERROR "Windows requires static LibYAML source: set CHOSUTA_LIBYAML_SOURCE_DIR or explicitly enable CHOSUTA_DOWNLOAD_DEPENDENCIES.")
 endif()
 find_path(CHOSUTA_YAML_INCLUDE_DIR yaml.h REQUIRED)
 find_library(CHOSUTA_YAML_LIBRARY NAMES yaml REQUIRED)
 add_library(chosuta_yaml UNKNOWN IMPORTED)
 set_target_properties(chosuta_yaml PROPERTIES IMPORTED_LOCATION "${CHOSUTA_YAML_LIBRARY}"
  INTERFACE_INCLUDE_DIRECTORIES "${CHOSUTA_YAML_INCLUDE_DIR}")
endif()
