#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "bitpolymul" for configuration "Release"
set_property(TARGET bitpolymul APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(bitpolymul PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libbitpolymul.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS bitpolymul )
list(APPEND _IMPORT_CHECK_FILES_FOR_bitpolymul "${_IMPORT_PREFIX}/lib/libbitpolymul.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
