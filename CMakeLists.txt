cmake_minimum_required(VERSION 3.20)

project(Prism LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

include(GNUInstallDirs)

find_package(Qt6 REQUIRED COMPONENTS Core DBus Widgets)

add_executable(prism
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/glsleffectinstaller.cpp
    src/glsleffectinstaller.h
)

target_link_libraries(prism PRIVATE
    Qt6::Core
    Qt6::DBus
    Qt6::Widgets
)

install(TARGETS prism
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES org.kde.prism.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
