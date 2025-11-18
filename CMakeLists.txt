# 设置CMake的最低版本要求
cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(floating-point)

# 设置C++标准为C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加 OpenMP 支持
find_package(OpenMP REQUIRED)

# 定义项目包含目录 - 统一头文件管理
set(PROJECT_INCLUDE_DIRS
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/emp-tool/emp-tool/#用于包含emp-tool.h
)


# 定义通用的源文件集合，避免重复
set(COMMON_SOURCES
)

# 为所有目标设置宏定义和包含目录
function(setup_target target_name)
    # 设置项目源码路径宏
    target_compile_definitions(${target_name} PRIVATE "PROJECT_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\"")
    
    # 统一设置包含目录
    target_include_directories(${target_name} PRIVATE ${PROJECT_INCLUDE_DIRS})
endfunction()

# Helper: add a test target with common sources and common libraries
function(add_test_target target_name)
    add_executable(${target_name}
        ${COMMON_SOURCES}
        "src/test/${target_name}.cpp"
    )
    setup_target(${target_name})
    target_link_libraries(${target_name} pthread gmp OpenMP::OpenMP_CXX)
endfunction()

# 主程序可执行文件
# add_executable(floating-point
#     ${COMMON_SOURCES}
#     src/main.cpp  # 当有主程序时取消注释
# )
# setup_target(floating-point)


# net_io_channel测试
add_test_target(net_io_channel_test)

# prg_sync测试
add_test_target(prg_test)

# MSS基础功能测试
add_test_target(MSS_basic_test)

# MSS加法功能测试
add_test_target(MSS_add_test)

# 定点数功能测试
add_test_target(fixed_point_test)

# 定点数明文乘法测试
add_test_target(fixed_point_plain_mul_test)

# detect协议测试
add_test_target(detect_test)

# prefix协议测试
add_test_target(prefix_test)

# 两方加法分享测试
add_test_target(addictive_share_test)

# 移位协议测试
add_test_target(shift_test)

# 最高位提取协议测试
add_test_target(sign_test)

# wrap协议测试
add_test_target(wrap_test)

# convert协议测试
add_test_target(convert_test)

# align协议测试
add_test_target(align_test)

# great协议测试
add_test_target(great_test)

# eq协议测试
add_test_target(eq_test)

# select协议测试
add_test_target(select_test)

# trunc协议测试
add_test_target(trunc_test)

# floating_point_mult协议测试
add_test_target(floating_point_mult_test)

# floating_point_add协议测试
add_test_target(floating_point_add_test)

# 定点乘法速度测试
add_test_target(speed_fixed_mult_test)

# 定点乘法SOTA速度测试
add_test_target(speed_SOTA_test)

# 浮点乘法速度测试
add_test_target(speed_floating_mult_test)

# 浮点加法速度测试
add_test_target(speed_floating_add_test)

# 如果有其他测试文件，可以按相同模式添加
# add_executable(otherTest
#     ${COMMON_SOURCES}
#     src/test/otherTest.cpp
# )
# setup_target(otherTest)

