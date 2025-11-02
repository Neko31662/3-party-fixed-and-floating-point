# 设置CMake的最低版本要求
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(floating-point)

# 设置C++标准为C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 定义项目包含目录 - 统一头文件管理
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/emp-tool/emp-tool/#用于包含emp-tool.h
)


# 定义通用的源文件集合，避免重复
set(COMMON_SOURCES
    # src/protocol/masked_RSS.cpp
)

# 为所有目标设置宏定义和包含目录
function(setup_target target_name)
    # 设置项目源码路径宏
    target_compile_definitions(${target_name} PRIVATE "PROJECT_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")
    
    # 统一设置包含目录
    target_include_directories(${target_name} PRIVATE ${PROJECT_INCLUDE_DIRS})
endfunction()

# 主程序可执行文件
# add_executable(floating-point
#     ${COMMON_SOURCES}
#     src/main.cpp  # 当有主程序时取消注释
# )
# setup_target(floating-point)

# 测试程序可执行文件
# 数据加载器测试

# net_io_channel测试
add_executable(net_io_channel_test
    ${COMMON_SOURCES}
    src/test/net_io_channel_test.cpp
)
setup_target(net_io_channel_test)
target_link_libraries(net_io_channel_test pthread gmp)

# prg_sync测试
add_executable(prg_test
    ${COMMON_SOURCES}
    src/test/prg_test.cpp
)
setup_target(prg_test)
target_link_libraries(prg_test pthread gmp)

# MSS基础功能测试
add_executable(MSS_basic_test
    ${COMMON_SOURCES}
    src/test/MSS_basic_test.cpp
)
setup_target(MSS_basic_test)
target_link_libraries(MSS_basic_test pthread gmp)

# 定点数功能测试
add_executable(fixed_point_test
    ${COMMON_SOURCES}
    src/test/fixed_point_test.cpp
)
setup_target(fixed_point_test)
target_link_libraries(fixed_point_test pthread gmp)


# 如果有其他测试文件，可以按相同模式添加
# add_executable(otherTest
#     ${COMMON_SOURCES}
#     src/test/otherTest.cpp
# )
# setup_target(otherTest)

