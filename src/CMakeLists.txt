cmake_minimum_required(VERSION 3.2)
project(VeriFS2)
# set GDB debug flag (-g) with cmake
set(CMAKE_BUILD_TYPE Debug)
# preprocessor for verifying Checkpoint/Restore APIs
#add_definitions(-DDUMP_TESTING)
add_executable(fuse-cpp-ramfs main.cpp directory.cpp inode.cpp symlink.cpp file.cpp util.cpp fuse_cpp_ramfs.cpp special_inode.cpp cr_util.cpp pickle.cpp)
add_executable(ckpt ckpt.cpp testops.cpp)
add_executable(restore restore.cpp testops.cpp)
add_executable(pkl pkl.cpp)
add_executable(load load.cpp)
set_property(TARGET fuse-cpp-ramfs PROPERTY CXX_STANDARD 17)
set_property(TARGET ckpt PROPERTY CXX_STANDARD 17)
set_property(TARGET restore PROPERTY CXX_STANDARD 17)
set_property(TARGET pkl PROPERTY CXX_STANDARD 17)
set_property(TARGET load PROPERTY CXX_STANDARD 17)
target_compile_definitions(fuse-cpp-ramfs PRIVATE FUSE_USE_VERSION=30 _FILE_OFFSET_BITS=64)
target_compile_definitions(ckpt PRIVATE FUSE_USE_VERSION=30 _FILE_OFFSET_BITS=64)
target_compile_definitions(restore PRIVATE FUSE_USE_VERSION=30 _FILE_OFFSET_BITS=64)
target_compile_definitions(pkl PRIVATE FUSE_USE_VERSION=30 _FILE_OFFSET_BITS=64)
target_compile_definitions(load PRIVATE FUSE_USE_VERSION=30 _FILE_OFFSET_BITS=64)
if(APPLE)
  target_link_libraries(fuse-cpp-ramfs osxfuse)
  target_link_libraries(ckpt osxfuse)
  target_link_libraries(restore osxfuse)
elseif(UNIX) # Linux, BSD etc
  target_link_libraries(fuse-cpp-ramfs fuse)
  target_link_libraries(ckpt fuse)
  target_link_libraries(restore fuse)
endif()
target_link_libraries(fuse-cpp-ramfs pthread ssl crypto mcfs)
target_link_libraries(ckpt pthread)
target_link_libraries(restore pthread)
target_link_libraries(pkl mcfs)
target_link_libraries(load mcfs)
add_custom_command(
    OUTPUT ${CMAKE_BINARY_DIR}/mount.fuse.fuse-cpp-ramfs
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/create-mount-helper.sh
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
add_custom_target(
    mount-helper ALL
    DEPENDS fuse-cpp-ramfs ${CMAKE_BINARY_DIR}/mount.fuse.fuse-cpp-ramfs)
install(TARGETS fuse-cpp-ramfs DESTINATION bin)
install(PROGRAMS ${CMAKE_BINARY_DIR}/mount.fuse.fuse-cpp-ramfs DESTINATION /sbin)

