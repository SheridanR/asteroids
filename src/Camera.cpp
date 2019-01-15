// Camera.cpp

#include "Main.hpp"
#include "Engine.hpp"
#include "Camera.hpp"
#include "Renderer.hpp"

Camera::Camera() {}

Camera::~Camera() {
	term();
}

void Camera::init() {
	if (line == nullptr) {
		line = new Line3D();
	}

	// setup camera
	float xres = mainEngine->getRenderer()->getXres();
	float yres = mainEngine->getRenderer()->getYres();
	float depth = 1024.f;

	// get camera transformation
	view = glm::lookAt(
		glm::vec3( 0.f, 0.f, 0.f ), // origin
		glm::vec3( 0.f, 1.f, 0.f ), // target vector
		glm::vec3( 0.f, 0.f, 1.f )  // up vector
	); 

	// get projection transformation
	proj = glm::ortho( -xres / 2.f, xres / 2.f, yres / 2.f, -yres / 2.f, depth, -depth );
	projView = proj * view;
}

void Camera::term() {
	if (line) {
		delete line;
		line = nullptr;
	}
}