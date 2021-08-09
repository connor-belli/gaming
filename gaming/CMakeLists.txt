# CMakeList.txt : CMake project for gaming, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.10)
set (CMAKE_CXX_STANDARD 20)

find_package(Vulkan REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(SDL2 CONFIG REQUIRED)
find_package(Bullet CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

set(KERNELS 
	"shaders/default.frag"
	"shaders/default.vert"
)

set(COMPILED_KERNELS
	"shaders/default.frag.spv"
	"shaders/default.vert.spv"
)



# Add source to this project's executable.
add_executable (gaming ${COMPILED_KERNELS} "gaming.cpp" "gaming.h" "imgui_custom.cpp" "imgui_custom.h" "vkctx.h" "pch.h" "vkctx.cpp" "windowswapchain.h" "windowswapchain.cpp" "defaultshader.h" "defaultshader.cpp" "defaultlayout.h" "defaultlayout.cpp" "shadermodule.h" "shadermodule.cpp" "defaultvertex.h")

foreach(KERNEL ${KERNELS})
	add_custom_command(OUTPUT ${KERNEL}.spv
	COMMAND glslc ${CMAKE_CURRENT_SOURCE_DIR}/${KERNEL} -g -o "${CMAKE_CURRENT_BINARY_DIR}/${KERNEL}.spv"
	DEPENDS ${KERNEL}
	COMMENT "Rebuilding ${KERNEL}.spv" )
	message(STATUS "Generating build commands for ${KERNEL}.spv")
endforeach()

target_link_libraries(gaming PRIVATE ${Vulkan_LIBRARIES})
target_link_libraries(gaming PRIVATE glm::glm)
target_link_libraries(gaming PRIVATE imgui::imgui)
target_link_libraries(gaming PRIVATE SDL2::SDL2 SDL2::SDL2main)
target_link_libraries(gaming PRIVATE LinearMath Bullet3Common BulletDynamics BulletSoftBody)

target_include_directories(gaming PRIVATE ${Vulkan_INCLUDE_DIRS})

# TODO: Add tests and install targets if needed.
