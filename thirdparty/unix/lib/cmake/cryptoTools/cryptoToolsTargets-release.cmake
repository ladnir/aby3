#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "oc::cryptoTools" for configuration "Release"
set_property(TARGET oc::cryptoTools APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(oc::cryptoTools PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "C;CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libcryptoTools.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS oc::cryptoTools )
list(APPEND _IMPORT_CHECK_FILES_FOR_oc::cryptoTools "${_IMPORT_PREFIX}/lib/libcryptoTools.a" )

# Import target "oc::tests_cryptoTools" for configuration "Release"
set_property(TARGET oc::tests_cryptoTools APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(oc::tests_cryptoTools PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/libtests_cryptoTools.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS oc::tests_cryptoTools )
list(APPEND _IMPORT_CHECK_FILES_FOR_oc::tests_cryptoTools "${_IMPORT_PREFIX}/lib/libtests_cryptoTools.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
