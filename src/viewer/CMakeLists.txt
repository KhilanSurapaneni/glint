add_library(glint_viewer STATIC metal_layer_bridge.mm)
target_include_directories(glint_viewer PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(glint_viewer PUBLIC metal_cpp glfw "-framework Cocoa")

# ARC (automatic reference counting) manages the Objective-C objects in this file for us —
# without this flag, CMake compiles .mm files without ARC by default.
set_source_files_properties(metal_layer_bridge.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
