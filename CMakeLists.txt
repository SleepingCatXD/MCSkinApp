cmake_minimum_required(VERSION 3.16)

# 定义项目名称和使用的语言
project(MCSkinManager VERSION 1.0 LANGUAGES CXX)

# 强制使用 C++17 标准 (现代 Qt 开发的标配)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 寻找 Qt 库 (兼容 Qt6 或 Qt5)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets)

# 包含子目录
add_subdirectory(DatasetCore)
add_subdirectory(MCSkinApp)
