cmake_minimum_required(VERSION 3.10)

# set the project name
project(Gfx2Next)

# add the executable
add_executable(gfx2next src/lodepng.c src/zx0.c src/gfx2next.c)

target_link_libraries(gfx2next m)

install(TARGETS gfx2next DESTINATION bin)
