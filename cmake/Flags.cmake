if("${CMAKE_BUILD_TYPE}" STREQUAL "Asan")
    add_compile_options(-fsanitize=address,undefined -fno-sanitize-recover=all)
    add_link_options(-fsanitize=address,undefined)
elseif("${CMAKE_BUILD_TYPE}" STREQUAL "Tsan")
    add_compile_options(-fsanitize=thread -fno-sanitize-recover=all)
    add_link_options(-fsanitize=thread)
endif()

