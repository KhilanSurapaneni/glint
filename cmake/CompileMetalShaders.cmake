# Compiles the given .metal source files into a single default.metallib next to TARGET's
# executable, and makes TARGET depend on that build step.
function(glint_add_metal_shaders TARGET)
  set(air_files "")
  foreach(metal_src ${ARGN})
    get_filename_component(shader_name ${metal_src} NAME_WE)
    set(air_file "${CMAKE_BINARY_DIR}/${shader_name}.air")
    add_custom_command(
      OUTPUT ${air_file}
      COMMAND xcrun -sdk macosx metal -c ${metal_src} -o ${air_file}
              -I ${CMAKE_SOURCE_DIR}/src/shaders
      DEPENDS ${metal_src}
      COMMENT "metal: compiling ${metal_src}"
      VERBATIM
    )
    list(APPEND air_files ${air_file})
  endforeach()

  set(metallib "${CMAKE_BINARY_DIR}/default.metallib")
  add_custom_command(
    OUTPUT ${metallib}
    COMMAND xcrun -sdk macosx metallib ${air_files} -o ${metallib}
    DEPENDS ${air_files}
    COMMENT "metal: linking default.metallib"
    VERBATIM
  )

  add_custom_target(${TARGET}_metallib DEPENDS ${metallib})
  add_dependencies(${TARGET} ${TARGET}_metallib)
endfunction()
