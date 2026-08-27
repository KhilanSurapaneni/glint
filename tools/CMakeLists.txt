# glint_io already carries glint_core and stb_image transitively (both PUBLIC in its own
# CMakeLists.txt), so linking it alone is enough — including the include path for
# stb_image_write.h, which lives in the same vendored stb repo as stb_image.h.
add_executable(replica_to_glcb replica_to_glcb.cpp)
target_link_libraries(replica_to_glcb PRIVATE glint_io)
