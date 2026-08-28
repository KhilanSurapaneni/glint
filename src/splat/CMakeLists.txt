# STATIC now that model.cpp (initialization, Phase 1.3) exists — was INTERFACE (header-only)
# through Phase 1.1/1.1b.
add_library(glint_splat STATIC model.cpp sort.cpp rasterizer.cpp loss.cpp)
target_include_directories(glint_splat PUBLIC ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(glint_splat PUBLIC glint_core glint_gpu)
