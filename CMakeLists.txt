cmake_minimum_required(VERSION 2.8)
project( raspiproject )
SET(COMPILE_DEFINITIONS -Werror)

include_directories(/usr/local/include)
include_directories(/opt/vc/include)
include_directories(/opt/vc/userland-master/interface/vcos)
include_directories(/opt/vc/userland-master)
include_directories(/opt/vc/userland-master/interface/vcos/pthreads)
include_directories(/opt/vc/userland-master/interface/vmcs_host/linux)
include_directories(/opt/vc/userland-master/host_applications/linux/libs/bcm_host/include)

link_directories(/usr/local/lib)
link_directories(/opt/vc/lib)

file(MAKE_DIRECTORY "${PROJECT_BINARY_DIR}/output")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/script.txt ${CMAKE_CURRENT_BINARY_DIR}/script.txt COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/examplescript.txt ${CMAKE_CURRENT_BINARY_DIR}/examplescript.txt COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/simplefragshader.glsl ${CMAKE_CURRENT_BINARY_DIR}/simplefragshader.glsl COPYONLY)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/simplevertshader.glsl ${CMAKE_CURRENT_BINARY_DIR}/simplevertshader.glsl COPYONLY)

add_executable(raspiproject raspiproject.cpp camera.cpp cameracontrol.cpp graphics.cpp)

target_link_libraries(raspiproject wiringPi sfml-system sfml-audio -lpng -ljpeg -lz libmmal_core.so libmmal_util.so libmmal_vc_client.so libvcos.so libbcm_host.so GLESv2 EGL)

#g++ raspiproject.cpp -o raspiproject -lwiringPi -lsfml-system -lsfml-audio -Wall