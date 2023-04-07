cmake_minimum_required(VERSION 3.14)

enable_language(CXX)

set( CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_CXX_FLAGS -pg)
set(CMAKE_EXE_LINKER_FLAGS -pg)
set(CMAKE_SHARED_LINKER_FLAGS -pg)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package( OpenCV REQUIRED )

set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
set(Boost_NO_BOOST_CMAKE ON)
find_package(Boost COMPONENTS thread REQUIRED)
#find_package( X11 REQUIRED )

find_package(PkgConfig REQUIRED)
pkg_check_modules( AV_C1394 REQUIRED IMPORTED_TARGET libavc1394)
pkg_check_modules( AV_CODEC REQUIRED IMPORTED_TARGET libavcodec )
pkg_check_modules( AV_DEVICE REQUIRED IMPORTED_TARGET libavdevice )
pkg_check_modules( AV_FILTER REQUIRED IMPORTED_TARGET libavfilter )
pkg_check_modules( AV_FORMAT REQUIRED IMPORTED_TARGET libavformat )
pkg_check_modules( AV_IF REQUIRED IMPORTED_TARGET libavif )
pkg_check_modules( AV_UTIL REQUIRED IMPORTED_TARGET libavutil )
pkg_check_modules( AV_SWSCALE REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules( XORG_DO REQUIRED IMPORTED_TARGET libxdo )
pkg_check_modules( OpenGL REQUIRED IMPORTED_TARGET opengl)

add_subdirectory(submodule/SDL)

include_directories(
    ${OpenCV_INCLUDE_DIRS} 
    ${OpenGL_INCLUDE_DIRS} 
    ${X11_INCLUDE_DIRS} 
    ${AV_C1394_INCLUDE_DIRS} 
    ${AV_CODEC_INCLUDE_DIRS} 
    ${AV_DEVICE_INCLUDE_DIRS} 
    ${AV_FILTER_INCLUDE_DIRS} 
    ${AV_FORMAT_INCLUDE_DIRS} 
    ${AV_IF_INCLUDE_DIRS} 
    ${AV_UTIL_INCLUDE_DIRS} 
    ${AV_SWSCALE_INCLUDE_DIRS} 
    ../../inc 
    ../inc
    ./include
)

add_compile_options(-Wall -O0 -g -pg)
add_link_options()

project( videoCapture )

message("Test di boost\n${Boost_LIBS}\n\n")

add_executable( videoCapture src/videoCaptureNvFBC.cpp src/tcpServer.cpp src/NvFBCUtils.c src/protocol.cpp )
target_link_libraries( videoCapture 
    PRIVATE SDL3::SDL3-static 
    PRIVATE ${OpenCV_LIBS} 
    PRIVATE ${OpenGL_LIBRARIES} 
    PRIVATE ${X11_LIBRARIES} 
    PRIVATE ${AV_C1394_LIBRARIES} 
    PRIVATE ${AV_CODEC_LIBRARIES} 
    PRIVATE ${AV_DEVICE_LIBRARIES} 
    PRIVATE ${AV_FILTER_LIBRARIES} 
    PRIVATE ${AV_FORMAT_LIBRARIES} 
    PRIVATE ${AV_IF_LIBRARIES} 
    PRIVATE ${AV_UTIL_LIBRARIES} 
    PRIVATE ${AV_SWSCALE_LIBRARIES}
    PRIVATE ${XORG_DO}
    Boost::thread
    )

project( videoStream )
add_executable( videoStream src/tcpClient.cpp src/protocol.cpp )
target_link_libraries( videoStream 
    PRIVATE SDL3::SDL3-static
    PRIVATE ${OpenCV_LIBS} 
    PRIVATE ${OpenGL_LIBRARIES} 
    PRIVATE ${X11_LIBRARIES}
    PRIVATE ${XORG_DO_LIBRARIES}
    Boost::thread
    )
