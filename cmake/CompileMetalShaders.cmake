# Compiles every given .metal file into one shared default.metallib in build/. Call this once,
# project-wide, listing every shader file. Executables that need it call
# glint_depends_on_metallib(TARGET) to make sure it's built first.
function(glint_compile_metal_shaders)
  set(air_files "")
  foreach(metal_src ${ARGN})
    get_filename_component(shader_name ${metal_src} NAME_WE)
    set(air_file "${CMAKE_BINARY_DIR}/${shader_name}.air")
    # Step 1: compile each .metal file to an intermediate .air file.
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

  # Step 2: link every .air file into one default.metallib.
  set(metallib "${CMAKE_BINARY_DIR}/default.metallib")
  add_custom_command(
    OUTPUT ${metallib}
    COMMAND xcrun -sdk macosx metallib ${air_files} -o ${metallib}
    DEPENDS ${air_files}
    COMMENT "metal: linking default.metallib"
    VERBATIM
  )

  add_custom_target(glint_metallib DEPENDS ${metallib})
endfunction()

# Makes TARGET depend on the shared default.metallib having been built first.
function(glint_depends_on_metallib TARGET)
  add_dependencies(${TARGET} glint_metallib)
endfunction()
