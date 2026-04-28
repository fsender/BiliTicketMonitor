cmake_minimum_required(VERSION 3.24)
project(BiliTicketMonitor)

# simdjson 需要 C++17，项目使用 C++20
set(CMAKE_CXX_STANDARD 20)

# 设置可执行文件输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 添加可执行文件
add_executable(BiliTicketMonitor
    main.cpp
    Config.cpp
    Monitor.cpp
    simdjson.cpp
)

# 查找libcurl包
find_package(CURL REQUIRED)

# 包含头文件目录
target_include_directories(BiliTicketMonitor PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接库
target_link_libraries(BiliTicketMonitor PRIVATE
    ${CURL_LIBRARIES}
)

# ==================== 跨平台编译配置 ====================

# 选项: 全静态构建 (musl 或 glibc)
option(STATIC_BUILD "Build fully static binary" OFF)
if(STATIC_BUILD)
    set(CMAKE_FIND_LIBRARY_SUFFIXES .a .lib)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    message(STATUS "Build mode: FULL STATIC")
else()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc -static-libstdc++")
    endif()
    message(STATUS "Build mode: SEMI-STATIC")
endif()

# musl 工具链检测
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER MATCHES "musl")
    message(STATUS "Musl toolchain detected")
    set(STATIC_BUILD ON)
endif()

# 查找libcurl包 (静态模式下会优先找 .a 文件)
find_package(CURL REQUIRED)

# 包含头文件目录
target_include_directories(BiliTicketMonitor PRIVATE
    ${CURL_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接库
target_link_libraries(BiliTicketMonitor PRIVATE
    ${CURL_LIBRARIES}
)

# 平台特定设置
if(WIN32)
    target_link_libraries(BiliTicketMonitor PRIVATE ws2_32 wininet)
    target_compile_definitions(BiliTicketMonitor PRIVATE CURL_STATICLIB=1 _HAS_STD_BYTE=0)
    if(STATIC_BUILD)
        # MSVC 全静态: /MT 替代 /MD
        set_property(TARGET BiliTicketMonitor PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
else()
    target_link_libraries(BiliTicketMonitor PRIVATE pthread)
endif()

if(STATIC_BUILD)
    target_compile_definitions(BiliTicketMonitor PRIVATE CURL_STATICLIB=1)
endif()

# 输出构建信息
message(STATUS "Build configuration complete")
message(STATUS "  Target: BiliTicketMonitor")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Libcurl: ${CURL_LIBRARIES}")
message(STATUS "  C++ Compiler: ${CMAKE_CXX_COMPILER}")
