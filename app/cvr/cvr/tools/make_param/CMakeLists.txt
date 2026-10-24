cmake_minimum_required(VERSION 3.8)
project(make_param)

# 编译主机版本
add_executable(make_param_host make_param.c)
target_compile_options(make_param_host PRIVATE -O2 -Wall)
target_include_directories(make_param_host PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 编译目标板版本
add_executable(make_param make_param.c)
target_compile_options(make_param PRIVATE -O2 -Wall -DRUN_ON_BOARD)
target_include_directories(make_param PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# 添加自定义清理目标
add_custom_target(clean_make_param_local ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Cleaning make_param directory..."
    COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_CURRENT_SOURCE_DIR} clean
    COMMENT "Cleaning make_param local directory"
)

# 确保在构建之前执行清理
add_dependencies(make_param clean_make_param_local)
add_dependencies(make_param_host clean_make_param_local)

# 添加清理目标
set_directory_properties(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/out;${CMAKE_CURRENT_SOURCE_DIR}/make_param;${CMAKE_CURRENT_SOURCE_DIR}/make_param_host;${CMAKE_CURRENT_SOURCE_DIR}/param.img")

# 安装
install(TARGETS make_param_host make_param DESTINATION bin)
