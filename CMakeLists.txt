#指定CMake要求的最低版本号
cmake_minimum_required(VERSION 3.10)
#生成的项目名(即PROJECT_NAME)
project(ConfigTest)

#用于定义某个宏
#add_definitions(-THREADED)

#设置C标准和C++标准
#set (CMAKE_C_FLAGS "-std=c11")
#set (CMAKE_CXX_FLAGS "-std=c++23")
#【优化】使用现代 CMake 推荐的方式设置 C++23 标准，而不是硬编码 CMAKE_CXX_FLAGS
set(CMAKE_CXX_STANDARD 23)
# 强制要求必须支持 C++23，如果编译器不支持则直接报错
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# 禁用特定编译器的扩展（如 gnu++23），保证代码的跨平台纯洁性
set(CMAKE_CXX_EXTENSIONS OFF)

#导出编译命令(compile_commands.json)，用于给 VS Code 的 clangd 插件提供精确的语法提示
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#打开详细信息，如果编译时嫌控制台输出太乱，可以将其设为 OFF
set(CMAKE_VERBOSE_MAKEFILE OFF)

#寻找依赖包(必须分开寻找)
find_package(spdlog CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(unofficial-zookeeper CONFIG REQUIRED)
find_package(Threads REQUIRED)    #系统级多线程库
find_package(zstd CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)


# 随着模块增加，建议按目录分类管理源文件，保持清晰
#自定义变量，解析变量使用美元符$
set(CONFIG_SRC
    src/log/log_manager.cpp
    src/load_config/thread_pool_config.cpp
    src/load_config/registry_config.cpp
    src/load_config/rpc_server_config.cpp
    src/load_config/rpc_client_config.cpp
)

set(TEST_SRC
    test/config_test.cpp
)

#合并源文件列表
set(PROJECT_SOURCES
    ${CONFIG_SRC}
    ${TEST_SRC}
)

#定义构建可执行程序，指定源文件列表，参数1表示可执行程序名，后面表示全部的源文件
add_executable(${PROJECT_NAME} ${PROJECT_SOURCES})

# 【优化】使用 target_include_directories 代替全局的 include_directories
# PRIVATE 关键字表示：只有 ConfigTest 这个程序能看到 src 下的头文件，不会污染其他目标
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

#链接静态库/动态库到指定的可执行程序(-l)
target_link_libraries(${PROJECT_NAME} PRIVATE 
    spdlog::spdlog
    nlohmann_json::nlohmann_json
    unofficial::zookeeper::zookeeper
    Threads::Threads
    # 优先链接共享库(动态库)，如果没有再退化链接静态库，这是 vcpkg 官方推荐的写法
    $<IF:$<TARGET_EXISTS:zstd::libzstd_shared>,zstd::libzstd_shared,zstd::libzstd_static>
    protobuf::libprotobuf
)