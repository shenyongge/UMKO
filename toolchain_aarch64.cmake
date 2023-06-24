
# cmake .. -DCMAKE_TOOLCHAIN_FILE=../toolchain_aarch64.cmake

# Set Sysem Name
set(CMAKE_SYSTEM_NAME Linux)
# 指定目标平台
set(CMAKE_SYSTEM_PROCESSOR arm)

# 指定C编译器
set(CMAKE_C_COMPILER "aarch64-linux-gnu-gcc")
# 指定C++编译器
set(CMAKE_CXX_COMPILER "aarch64-linux-gnu-g++")

set(CMAKE_C_FLAGS "-std=gnu17 -g ")
set(CMAKE_CXX_FLAGS "-std=c++17 -g -O2 ")