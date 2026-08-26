add_library(glint_viewer STATIC
  metal_layer_bridge.mm
  imgui_metal_bridge.mm
  orbit_camera.cpp
)
target_include_directories(glint_viewer PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(glint_viewer PUBLIC metal_cpp glfw imgui "-framework Cocoa")

# ARC (automatic reference counting) manages these files' Objective-C objects for us.
set_source_files_properties(metal_layer_bridge.mm imgui_metal_bridge.mm
  PROPERTIES COMPILE_FLAGS "-fobjc-arc")
