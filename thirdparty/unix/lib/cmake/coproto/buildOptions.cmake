
set(coproto_VERSION_MAJOR     0)
set(coproto_VERSION_MINOR     3)
set(coproto_VERSION_PATCH     2)

# compile the library with c++20 support
set(COPROTO_CPP20 ON)

set(COPROTO_ENABLE_SPAN )
set(COPROTO_ENABLE_BOOST ON)
set(COPROTO_ENABLE_OPENSSL OFF)

# compile the library logging support
set(COPROTO_LOGGING ) 

set(COPROTO_CPP_VER 20)
set(COPROTO_ENABLE_ASSERTS ON)

set(COPROTO_ASAN OFF)
set(COPROTO_PIC OFF)


unset(coproto_debug_FOUND CACHE)
unset(coproto_Debug_FOUND CACHE)
unset(coproto_DEBUG_FOUND CACHE)
unset(coproto_release_FOUND CACHE)
unset(coproto_Release_FOUND CACHE)
unset(coproto_RELEASE_FOUND CACHE)
unset(coproto_relwithdebinfo_FOUND CACHE)
unset(coproto_RelWithDebInfo_FOUND CACHE)
unset(coproto_RELWITHDEBINFO_FOUND CACHE)

set(COPROTO_BUILD_TYPE "Release")
string( TOLOWER "Release" CMAKE_BUILD_TYPE_lower )
if(CMAKE_BUILD_TYPE_lower STREQUAL "debug")
	set(coproto_debug_FOUND true)
	set(coproto_Debug_FOUND true)
	set(coproto_DEBUG_FOUND true)
endif()
if(CMAKE_BUILD_TYPE_lower STREQUAL "release")
	set(coproto_release_FOUND true)
	set(coproto_Release_FOUND true)
	set(coproto_RELEASE_FOUND true)
endif()
if(CMAKE_BUILD_TYPE_lower STREQUAL "relwithdebinfo")
	set(coproto_relwithdebinfo_FOUND true)
	set(coproto_RelWithDebInfo_FOUND true)
	set(coproto_RELWITHDEBINFO_FOUND true)
endif()



if(20 EQUAL 14)
	set(coproto_cpp14_FOUND true)
	set(coproto_cpp17_FOUND false)
elseif(20 EQUAL 17)
	set(coproto_cpp17_FOUND true)
	set(coproto_cpp14_FOUND false)
else()
	set(coproto_cpp14_FOUND false)
	set(coproto_cpp17_FOUND false)
endif()
set(coproto_cpp20_FOUND ON)

set(coproto_boost_FOUND ON)
set(coproto_openssl_FOUND OFF)
set(coproto_asan_FOUND OFF)
set(coproto_pic_FOUND OFF)


if(NOT COPROTO_ENABLE_BOOST)
	set(coproto_no_boost_FOUND true)
else()
	set(coproto_no_boost_FOUND false)
endif()

if(NOT COPROTO_ENABLE_OPENSSL)
	set(coproto_no_openssl_FOUND true)
else()
	set(coproto_no_openssl_FOUND false)
endif()

if(NOT COPROTO_PIC)
	set(coproto_no_pic_FOUND true)
else()
	set(coproto_no_pic_FOUND false)
endif()

if(NOT COPROTO_ASAN)
	set(coproto_no_asan_FOUND true)
else()
	set(coproto_no_asan_FOUND false)
endif()

