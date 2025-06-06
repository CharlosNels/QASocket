cmake_minimum_required(VERSION 3.16)

project(QASocket LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Network)

set(ASIO_PATH F:/Libs/asio)

if(WIN32)
    include_directories(${ASIO_PATH}/include)
    set(Target_Name QASocketWin)
else()
    set(Target_Name QASocketLinux)
endif()

add_library(${Target_Name} STATIC
  mytcpsocket.cpp mytcpsocket.h myudpsocket.cpp myudpsocket.h
  mytcpserver.h mytcpserver.cpp
)

target_link_libraries(${Target_Name} PRIVATE Qt${QT_VERSION_MAJOR}::Core Qt${QT_VERSION_MAJOR}::Network)

target_compile_definitions(${Target_Name} PRIVATE QASOCKET_LIBRARY)
