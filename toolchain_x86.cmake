
# cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain_x86.cmake

# Set Sysem Name
set(CMAKE_SYSTEM_NAME Linux)
# 指定目标平台
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# 指定C编译器
set(CMAKE_C_COMPILER "gcc")
# 指定C++编译器
set(CMAKE_CXX_COMPILER "g++")

set(CMAKE_C_FLAGS "-std=c17 -g -O2 ")
set(CMAKE_CXX_FLAGS "-std=c++17 -g -O2 ")

