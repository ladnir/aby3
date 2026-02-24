#----------------------------------------------------------------
# Generated CMake target import file for configuration "Release".
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "oc::libOTe" for configuration "Release"
set_property(TARGET oc::libOTe APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(oc::libOTe PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/liblibOTe.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS oc::libOTe )
list(APPEND _IMPORT_CHECK_FILES_FOR_oc::libOTe "${_IMPORT_PREFIX}/lib/liblibOTe.a" )

# Import target "oc::libOTe_Tests" for configuration "Release"
set_property(TARGET oc::libOTe_Tests APPEND PROPERTY IMPORTED_CONFIGURATIONS RELEASE)
set_target_properties(oc::libOTe_Tests PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_RELEASE "CXX"
  IMPORTED_LOCATION_RELEASE "${_IMPORT_PREFIX}/lib/liblibOTe_Tests.a"
  )

list(APPEND _IMPORT_CHECK_TARGETS oc::libOTe_Tests )
list(APPEND _IMPORT_CHECK_FILES_FOR_oc::libOTe_Tests "${_IMPORT_PREFIX}/lib/liblibOTe_Tests.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
