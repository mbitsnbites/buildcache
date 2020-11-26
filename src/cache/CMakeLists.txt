#---------------------------------------------------------------------------------------------------
# Copyright (c) 2019 Marcus Geelnard
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

set(CACHE_SRCS
  cache.cpp
  cache.hpp
  expected_file.hpp
  cache_entry.cpp
  cache_entry.hpp
  cache_stats.cpp
  cache_stats.hpp
  data_store.cpp
  data_store.hpp
  local_cache.cpp
  local_cache.hpp
  http_cache_provider.cpp
  http_cache_provider.hpp
  redis_cache_provider.cpp
  redis_cache_provider.hpp
  remote_cache.cpp
  remote_cache.hpp
  remote_cache_provider.cpp
  remote_cache_provider.hpp
  )

# In order to enable S3, we need HMAC support.
if(HAS_HMAC)
  set(HAS_S3 true)
endif()

if(HAS_S3)
  list(APPEND CACHE_SRCS
    s3_cache_provider.cpp
    s3_cache_provider.hpp
    )
endif()

add_library(cache ${CACHE_SRCS})
target_link_libraries(cache base config hiredis HTTPRequest)
if(HAS_S3)
  target_link_libraries(cache cpp-base64)
  target_compile_definitions(cache PUBLIC ENABLE_S3)
endif()

buildcache_add_test(NAME remote_cache_provider_test
                    SOURCES remote_cache_provider_test.cpp
                    LIBRARIES cache)

