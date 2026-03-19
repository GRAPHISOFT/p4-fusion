file(GLOB_RECURSE Headers **.h)
file(GLOB_RECURSE Sources **.cc)

if (NOT OPENSSL_ROOT_DIR)
    set(OPENSSL_ROOT_DIR /usr/local/ssl)
endif()

set(OPENSSL_USE_STATIC_LIBS true)
find_package(OpenSSL)
find_package(CURL REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(p4-fusion ${Headers} ${Sources})

target_include_directories(p4-fusion PUBLIC
    ../${HELIX_API}/include/
    ../vendor/libgit2/include/
    ../vendor/minitrace/
    ../vendor/rapidjson/include/
    ${OPENSSL_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${ZLIB_INCLUDE_DIRS}
    ${CMAKE_CURRENT_LIST_DIR}
)

target_link_directories(p4-fusion PUBLIC
    ../${HELIX_API}/lib/
)

set(Frameworks "")

if (APPLE)
    find_library(COREFOUNDATION_LIB CoreFoundation REQUIRED)
    find_library(CFNETWORK_LIB CFNetwork REQUIRED)
    find_library(COCOA_LIB Cocoa REQUIRED)
    find_library(SECURITY_LIB Security REQUIRED)
    find_library(NETWORK_LIB Network REQUIRED)
    set(Frameworks
        ${Frameworks}
        ${CFNETWORK_LIB}
        ${COREFOUNDATION_LIB}
        ${COCOA_LIB}
        ${SECURITY_LIB}
        ${NETWORK_LIB}
    )
endif(APPLE)

message(${OPENSSL_SSL_LIBRARIES})
message(${OPENSSL_CRYPTO_LIBRARIES})

target_link_libraries(p4-fusion PUBLIC
    aws-cpp-sdk-core
    aws-cpp-sdk-s3
    ${Frameworks}
    client
    rpc
    supp
    p4script_cstub
    ${OPENSSL_SSL_LIBRARIES}
    ${OPENSSL_CRYPTO_LIBRARIES}
    ${CURL_LIBRARIES}
    ${ZLIB_LIBRARIES}
    libgit2package
    minitrace
)
