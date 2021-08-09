#pragma once

#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

struct DefaultVertex {
	glm::vec3 pos;
	glm::vec3 normal;
	glm::vec3 color;
};

struct UniformBufferObject {
	glm::mat4 model;
	glm::mat4 view;
	glm::mat4 projection;
};