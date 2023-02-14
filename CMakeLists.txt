cmake_minimum_required(VERSION 3.14)

set( CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_CXX_FLAGS -pg)
set(CMAKE_EXE_LINKER_FLAGS -pg)
set(CMAKE_SHARED_LINKER_FLAGS -pg)

find_package( OpenCV REQUIRED )
find_package( X11 REQUIRED )

find_package(PkgConfig REQUIRED)
pkg_check_modules( AV_C1394 REQUIRED IMPORTED_TARGET libavc1394)
pkg_check_modules( AV_CODEC REQUIRED IMPORTED_TARGET libavcodec )
pkg_check_modules( AV_DEVICE REQUIRED IMPORTED_TARGET libavdevice )
pkg_check_modules( AV_FILTER REQUIRED IMPORTED_TARGET libavfilter )
pkg_check_modules( AV_FORMAT REQUIRED IMPORTED_TARGET libavformat )
pkg_check_modules( AV_IF REQUIRED IMPORTED_TARGET libavif )
pkg_check_modules( AV_UTIL REQUIRED IMPORTED_TARGET libavutil )
pkg_check_modules( AV_SWSCALE REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules( OpenGL REQUIRED IMPORTED_TARGET opengl)

if(${AV_CODEC_FOUND})
    MESSAGE("pkg-config executable " ${PKG_CONFIG_EXECUTABLE})
    MESSAGE("AV codec libs " ${AV_CODEC_LIBRARIES})
    MESSAGE("av codec libs2 "${AV_CODEC_LINK_LIBRARIES})
    MESSAGE("av codec libs3 "${AV_CODEC_LIBS})
endif()

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

add_compile_options(-Wall -O0 -pg)
add_link_options()

project( videoCapture )

add_executable( videoCapture src/videoCaptureNvFBC.cpp src/tcpServer.cpp ../common/NvFBCUtils.c )
target_link_libraries( videoCapture 
    ${OpenCV_LIBS} 
    ${OpenGL_LIBRARIES} 
    ${X11_LIBRARIES} 
    ${AV_C1394_LIBRARIES} 
    ${AV_CODEC_LIBRARIES} 
    ${AV_DEVICE_LIBRARIES} 
    ${AV_FILTER_LIBRARIES} 
    ${AV_FORMAT_LIBRARIES} 
    ${AV_IF_LIBRARIES} 
    ${AV_UTIL_LIBRARIES} 
    ${AV_SWSCALE_LIBRARIES} 
    )

project( videoStream )

add_executable( videoStream src/tcpClient.cpp )
target_link_libraries( videoStream ${OpenCV_LIBS} ${OpenGL_LIBRARIES} ${X11_LIBRARIES} )

project( chatServer )

add_executable( chatServer src/chat_server.cpp )

project( chatClient)

add_executable( chatClient src/chat_client.cpp )
