# 定义一个静态库，包含它的源码和头文件
add_library(DatasetCore STATIC 
    DatasetManager.cpp 
    DatasetManager.h
)

# 链接 Qt 核心模块
target_link_libraries(DatasetCore PRIVATE Qt${QT_VERSION_MAJOR}::Core)

# 【核心黑科技】：PUBLIC 包含目录
# 这句话的意思是：任何链接了 DatasetCore 的程序（比如我们的主程序），
# 都能直接 include 这个文件夹里的头文件，而不需要写长长的相对路径。
target_include_directories(DatasetCore PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})