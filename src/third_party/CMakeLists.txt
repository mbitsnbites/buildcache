#---------------------------------------------------------------------------------------------------
# Copyright (c) 2018 Marcus Geelnard
#
# This software is provided 'as-is', without any express or implied warranty. In no event will the
# authors be held liable for any damages arising from the use of this software.
#
# Permission is granted to anyone to use this software for any purpose, including commercial
# applications, and to alter it and redistribute it freely, subject to the following restrictions:
#
#  1. The origin of this software must not be misrepresented; you must not claim that you wrote
#     the original software. If you use this software in a product, an acknowledgment in the
#     product documentation would be appreciated but is not required.
#
#  2. Altered source versions must be plainly marked as such, and must not be misrepresented as
#     being the original software.
#
#  3. This notice may not be removed or altered from any source distribution.
#---------------------------------------------------------------------------------------------------

# Disable all warnings for third party code.
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU" OR "${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -w")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -w")
elseif(MSVC)
  # Remove any /W flags from the compiler options.
  string(REGEX REPLACE "(^| )([/-])W[0-9]( |$)" " " CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "(^| )([/-])W[0-9]( |$)" " " CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
endif()

#---------------------------------------------------------------------------------------------------
# doctest
#---------------------------------------------------------------------------------------------------
add_library(doctest
  doctest/doctest_main.cpp
  )
target_include_directories(doctest INTERFACE .)
if(MINGW)
  target_link_libraries(doctest mingw_stdthreads)
endif()

#---------------------------------------------------------------------------------------------------
# cJSON
#---------------------------------------------------------------------------------------------------
add_library(cjson
  cjson/cJSON.c
  )
target_include_directories(cjson INTERFACE .)

#---------------------------------------------------------------------------------------------------
# xxHash
#---------------------------------------------------------------------------------------------------
add_library(xxhash INTERFACE)
target_include_directories(xxhash INTERFACE .)

#---------------------------------------------------------------------------------------------------
# lz4
#---------------------------------------------------------------------------------------------------
add_library(lz4
  lz4/lz4.c
  )
target_include_directories(lz4 INTERFACE .)

#---------------------------------------------------------------------------------------------------
# zstd
#---------------------------------------------------------------------------------------------------
add_library(zstd
  zstd/common/debug.c
  zstd/common/entropy_common.c
  zstd/common/error_private.c
  zstd/common/fse_decompress.c
  zstd/common/pool.c
  zstd/common/threading.c
  zstd/common/xxhash.c
  zstd/common/zstd_common.c
  zstd/compress/fse_compress.c
  zstd/compress/hist.c
  zstd/compress/huf_compress.c
  zstd/compress/zstd_compress.c
  zstd/compress/zstd_compress_literals.c
  zstd/compress/zstd_compress_sequences.c
  zstd/compress/zstd_compress_superblock.c
  zstd/compress/zstd_double_fast.c
  zstd/compress/zstd_fast.c
  zstd/compress/zstd_lazy.c
  zstd/compress/zstd_ldm.c
  zstd/compress/zstd_opt.c
  zstd/compress/zstdmt_compress.c
  zstd/decompress/huf_decompress.c
  zstd/decompress/zstd_ddict.c
  zstd/decompress/zstd_decompress.c
  zstd/decompress/zstd_decompress_block.c
  zstd/dictBuilder/cover.c
  zstd/dictBuilder/divsufsort.c
  zstd/dictBuilder/fastcover.c
  zstd/dictBuilder/zdict.c
  zstd/legacy/zstd_v01.c
  zstd/legacy/zstd_v02.c
  zstd/legacy/zstd_v03.c
  zstd/legacy/zstd_v04.c
  zstd/legacy/zstd_v05.c
  zstd/legacy/zstd_v06.c
  zstd/legacy/zstd_v07.c
  )
target_include_directories(zstd INTERFACE .)
target_include_directories(zstd PRIVATE zstd/common)

#---------------------------------------------------------------------------------------------------
# Lua
#---------------------------------------------------------------------------------------------------
set(LUA_DIR lua-5.3.4)
add_library(lua
  ${LUA_DIR}/src/lapi.c
  ${LUA_DIR}/src/lcode.c
  ${LUA_DIR}/src/lctype.c
  ${LUA_DIR}/src/ldebug.c
  ${LUA_DIR}/src/ldo.c
  ${LUA_DIR}/src/ldump.c
  ${LUA_DIR}/src/lfunc.c
  ${LUA_DIR}/src/lgc.c
  ${LUA_DIR}/src/llex.c
  ${LUA_DIR}/src/lmem.c
  ${LUA_DIR}/src/lobject.c
  ${LUA_DIR}/src/lopcodes.c
  ${LUA_DIR}/src/lparser.c
  ${LUA_DIR}/src/lstate.c
  ${LUA_DIR}/src/lstring.c
  ${LUA_DIR}/src/ltable.c
  ${LUA_DIR}/src/ltm.c
  ${LUA_DIR}/src/lundump.c
  ${LUA_DIR}/src/lvm.c
  ${LUA_DIR}/src/lzio.c
  ${LUA_DIR}/src/lauxlib.c
  ${LUA_DIR}/src/lbaselib.c
  ${LUA_DIR}/src/lbitlib.c
  ${LUA_DIR}/src/lcorolib.c
  ${LUA_DIR}/src/ldblib.c
  ${LUA_DIR}/src/liolib.c
  ${LUA_DIR}/src/lmathlib.c
  ${LUA_DIR}/src/loslib.c
  ${LUA_DIR}/src/lstrlib.c
  ${LUA_DIR}/src/ltablib.c
  ${LUA_DIR}/src/lutf8lib.c
  ${LUA_DIR}/src/loadlib.c
  ${LUA_DIR}/src/linit.c
  )
target_include_directories(lua PUBLIC ${LUA_DIR}/src)

if(APPLE)
  target_compile_definitions(lua PRIVATE LUA_USE_MACOSX)
  target_link_libraries(lua ${CMAKE_DL_LIBS})
elseif(UNIX AND NOT (WIN32 OR MINGW))
  target_compile_definitions(lua PRIVATE LUA_USE_LINUX)
  target_link_libraries(lua ${CMAKE_DL_LIBS})
endif()

#---------------------------------------------------------------------------------------------------
# cpp-base64
#---------------------------------------------------------------------------------------------------
add_library(cpp-base64
  cpp-base64/base64.cpp
  )
target_include_directories(cpp-base64 INTERFACE .)

#---------------------------------------------------------------------------------------------------
# Hiredis
#---------------------------------------------------------------------------------------------------
set(hiredis_sources
  hiredis/alloc.c
  hiredis/async.c
  hiredis/dict.c
  hiredis/hiredis.c
  hiredis/net.c
  hiredis/read.c
  hiredis/sds.c
  hiredis/sockcompat.c
  )
# hiredis SSL support requires OpenSSL
# for now, disable on windows
if(NOT (WIN32 OR MINGW))
  find_package(OpenSSL)
  if(OPENSSL_FOUND)
    list(APPEND hiredis_sources
      hiredis/ssl.c
      )
  endif()
endif()
add_library(hiredis ${hiredis_sources})
target_include_directories(hiredis INTERFACE .)
set_property(TARGET hiredis PROPERTY C_STANDARD 99)
if(MSVC)
  target_compile_options(hiredis PRIVATE /wd4312)
endif()
if(WIN32 OR MINGW)
  target_link_libraries(hiredis PRIVATE ws2_32)
elseif(OPENSSL_FOUND)
  target_link_libraries(hiredis PRIVATE OpenSSL::SSL)
endif()

#---------------------------------------------------------------------------------------------------
# mingw-std-threads (implements std::thread for MinGW).
#---------------------------------------------------------------------------------------------------
if(MINGW)
  option(MINGW_STDTHREADS_GENERATE_STDHEADERS "" ON)
  add_subdirectory(mingw-std-threads)
  target_include_directories(mingw_stdthreads INTERFACE .)
endif()

#---------------------------------------------------------------------------------------------------
# HTTPRequest
#---------------------------------------------------------------------------------------------------
add_library(HTTPRequest INTERFACE)
target_include_directories(HTTPRequest INTERFACE HTTPRequest/include)
if(WIN32 OR MINGW)
  target_link_libraries(HTTPRequest INTERFACE ws2_32)
endif()
