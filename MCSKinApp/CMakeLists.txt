# 开启 Qt 的三大自动化魔法，告别手动编译 UI 和 MOC 报错
set(CMAKE_AUTOMOC ON)  # 自动处理 Q_OBJECT 宏
set(CMAKE_AUTOUIC ON)  # 自动将 .ui 转换为 ui_*.h
set(CMAKE_AUTORCC ON)  # 自动编译 .qrc 资源文件

# 收集所有的源码文件
set(APP_SOURCES
    main.cpp
    MCSkinApp.cpp
    MCSkinApp.h
    MCSkinApp.ui
    MCSkinApp.qrc
    GalleryWindow.cpp
    GalleryWindow.h
    GalleryWindow.ui
    ImageCropperLabel.cpp
    ImageCropperLabel.h
    AnnotationWindow.cpp
    AnnotationWindow.h
    AnnotationWindow.ui
)

# 生成可执行文件。WIN32 参数可以隐藏 Windows 下背后那个黑色的控制台窗口
add_executable(MCSKinApp WIN32 ${APP_SOURCES})

# 将主程序与 Qt 模块，以及我们自己写的 DatasetCore 静态库链接起来
target_link_libraries(MCSKinApp PRIVATE 
    Qt${QT_VERSION_MAJOR}::Core 
    Qt${QT_VERSION_MAJOR}::Gui 
    Qt${QT_VERSION_MAJOR}::Widgets 
    DatasetCore
)