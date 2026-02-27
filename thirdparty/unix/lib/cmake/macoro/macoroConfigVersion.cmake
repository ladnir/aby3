
include("${CMAKE_CURRENT_LIST_DIR}/macoroBuildOptions.cmake")

set(PACKAGE_VERSION "${macoro_VERSION_MAJOR}.${macoro_VERSION_MINOR}.${macoro_VERSION_PATCH}")

if (PACKAGE_FIND_VERSION_RANGE)
  # Package version must be in the requested version range
  if ((PACKAGE_FIND_VERSION_RANGE_MIN STREQUAL "INCLUDE" AND PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION_MIN)
      OR ((PACKAGE_FIND_VERSION_RANGE_MAX STREQUAL "INCLUDE" AND PACKAGE_VERSION VERSION_GREATER PACKAGE_FIND_VERSION_MAX)
        OR (PACKAGE_FIND_VERSION_RANGE_MAX STREQUAL "EXCLUDE" AND PACKAGE_VERSION VERSION_GREATER_EQUAL PACKAGE_FIND_VERSION_MAX)))
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
  else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
  endif()
else()
  if(PACKAGE_VERSION VERSION_LESS PACKAGE_FIND_VERSION)
    set(PACKAGE_VERSION_COMPATIBLE FALSE)
  else()
    set(PACKAGE_VERSION_COMPATIBLE TRUE)
    if(PACKAGE_FIND_VERSION STREQUAL PACKAGE_VERSION)
      set(PACKAGE_VERSION_EXACT TRUE)
    endif()
  endif()
endif()



set(missing_components)
foreach(comp ${macoro_FIND_COMPONENTS})
    if(NOT macoro_${comp}_FOUND)
        if(macoro_FIND_REQUIRED_${comp})
            set(PACKAGE_VERSION_UNSUITABLE TRUE)
            set(missing_components ${missing_components} ${comp})
        endif()
    endif()
endforeach()


if(PACKAGE_VERSION_UNSUITABLE AND NOT macoro_FIND_QUIETLY) 
	message("Found incompatible macoro at ${CMAKE_CURRENT_LIST_DIR}. Missing components: ${missing_components}")
endif()


# if the installed project requested no architecture check, don't perform the check
if("FALSE")
  return()
endif()

# if the installed or the using project don't have CMAKE_SIZEOF_VOID_P set, ignore it:
if("${CMAKE_SIZEOF_VOID_P}" STREQUAL "" OR "8" STREQUAL "")
  return()
endif()

# check that the installed version has the same 32/64bit-ness as the one which is currently searching:
if(NOT CMAKE_SIZEOF_VOID_P STREQUAL "8")
  math(EXPR installedBits "8 * 8")
  set(PACKAGE_VERSION "${PACKAGE_VERSION} (${installedBits}bit)")
  set(PACKAGE_VERSION_UNSUITABLE TRUE)
endif()
