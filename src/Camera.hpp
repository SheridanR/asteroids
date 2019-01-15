// Camera.hpp

#pragma once

#include "Main.hpp"
#include "Line3D.hpp"

class Camera {
public:
	Camera();
	~Camera();

	void init();
	void term();

	glm::mat4 proj;
	glm::mat4 view;
	glm::mat4 projView;
	Line3D* line = nullptr;
};