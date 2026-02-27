
set(macoro_VERSION_MAJOR     0)
set(macoro_VERSION_MINOR     1)
set(macoro_VERSION_PATCH     2)

# compile the library with c++20 support
set(MACORO_CPP_20 ON)


set(MACORO_CPP_VER 20)
set(MACORO_ENABLE_ASSERTS )

set(MACORO_ASAN OFF)
set(MACORO_PIC OFF)

set(MACORO_VARIANT_LITE_V OFF)
set(MACORO_OPTIONAL_LITE_V OFF)



unset(macoro_debug_FOUND CACHE)
unset(macoro_Debug_FOUND CACHE)
unset(macoro_DEBUG_FOUND CACHE)
unset(macoro_release_FOUND CACHE)
unset(macoro_Release_FOUND CACHE)
unset(macoro_RELEASE_FOUND CACHE)
unset(macoro_relwithdebinfo_FOUND CACHE)
unset(macoro_RelWithDebInfo_FOUND CACHE)
unset(macoro_RELWITHDEBINFO_FOUND CACHE)
string( TOLOWER "Release" CMAKE_BUILD_TYPE_lower )
if(CMAKE_BUILD_TYPE_lower STREQUAL "debug")
	set(macoro_debug_FOUND true)
	set(macoro_Debug_FOUND true)
	set(macoro_DEBUG_FOUND true)
endif()
if(CMAKE_BUILD_TYPE_lower STREQUAL "release")
	set(macoro_release_FOUND true)
	set(macoro_Release_FOUND true)
	set(macoro_RELEASE_FOUND true)
endif()
if(CMAKE_BUILD_TYPE_lower STREQUAL "relwithdebinfo")
	set(macoro_relwithdebinfo_FOUND true)
	set(macoro_RelWithDebInfo_FOUND true)
	set(macoro_RELWITHDEBINFO_FOUND true)
endif()



if(20 EQUAL 14)
	set(macoro_cpp_14_FOUND true)
	set(macoro_cpp_17_FOUND false)
elseif(20 EQUAL 17)
	set(macoro_cpp_14_FOUND false)
	set(macoro_cpp_17_FOUND true)
else()
	set(macoro_cpp_14_FOUND false)
	set(macoro_cpp_17_FOUND false)
endif()
set(macoro_cpp_20_FOUND ON)


set(macoro_asan_FOUND OFF)
set(macoro_pic_FOUND OFF)
set(macoro_optional_lite_FOUND OFF)
set(macoro_variant_lite_FOUND OFF)


if(NOT MACORO_PIC)
	set(macoro_no_pic_FOUND true)
else()
	set(macoro_no_pic_FOUND false)
endif()

if(NOT MACORO_ASAN)
	set(macoro_no_asan_FOUND true)
else()
	set(macoro_no_asan_FOUND false)
endif()



