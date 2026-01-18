file(GLOB_RECURSE FALTER_SOURCES CONFIGURE_DEPENDS src/*.cpp)

add_library(falter SHARED ${FALTER_SOURCES})

target_include_directories(falter PUBLIC include)
target_include_directories(falter PRIVATE src)
