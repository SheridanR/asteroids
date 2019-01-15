// Line3D.cpp

#include "Main.hpp"
#include "Engine.hpp"
#include "ShaderProgram.hpp"
#include "Renderer.hpp"
#include "Line3D.hpp"

const GLfloat Line3D::vertices[6] = {
	0.f, 0.f, 0.f,
	1.f, 1.f, 1.f
};

const GLuint Line3D::indices[2] {
	0, 1
};

Line3D::Line3D() {
	// initialize buffer names
	for( int i=0; i<BUFFER_TYPE_LENGTH; ++i ) {
		vbo[static_cast<buffer_t>(i)] = 0;
	}

	// create vertex array
	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);

	// upload vertex data
	glGenBuffers(1, &vbo[VERTEX_BUFFER]);
	glBindBuffer(GL_ARRAY_BUFFER, vbo[VERTEX_BUFFER]);
	glBufferData(GL_ARRAY_BUFFER, 2 * 3 * sizeof(GLfloat), vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(0);

	// upload index data
	glGenBuffers(1, &vbo[INDEX_BUFFER]);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vbo[INDEX_BUFFER]);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, 2 * sizeof(GLuint), indices, GL_STATIC_DRAW);
	glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, 0, NULL);
	glEnableVertexAttribArray(1);

	// unbind vertex array
	glBindVertexArray(0);
}

Line3D::~Line3D() {
	for( int i=0; i<BUFFER_TYPE_LENGTH; ++i ) {
		buffer_t buffer = static_cast<buffer_t>(i);
		if( vbo[buffer] ) {
			glDeleteBuffers(1,&vbo[buffer]);
		}
	}
	if( vao ) {
		glDeleteVertexArrays(1,&vao);
	}
}

void Line3D::drawLine(Camera& camera, const Vector& src, const Vector& dest, const WideVector& color) {
	draw(camera, src, dest, color, "shaders/basic/line3D.json");
}

void Line3D::draw(Camera& camera, const glm::vec3& src, const glm::vec3& dest, const glm::vec4& color, const char* material) {
	// get difference of vectors
	glm::vec3 diff(0.f);
	diff.x = dest.x-src.x;
	diff.y = dest.y-src.y;
	diff.z = dest.z-src.z;

	// setup model matrix
	const glm::mat4 modelMatrix = glm::translate(glm::mat4(1.f),glm::vec3(src.x,-src.z,src.y));

	// setup viewproj matrix
	const glm::mat4 viewProjMatrix = camera.projView;

	// setup modelview matrix
	const glm::mat4 modelViewMatrix = camera.view * modelMatrix;

	// load shader
	Material* mat = mainEngine->getMaterialResource().dataForString(material);
	if( mat ) {
		ShaderProgram& shader = mat->getShader();
		if( &shader != ShaderProgram::getCurrentShader() )
			shader.mount();

		// upload uniform variables
		glUniformMatrix4fv(shader.getUniformLocation("gModel"),1,GL_FALSE,glm::value_ptr(modelMatrix));
		glUniformMatrix4fv(shader.getUniformLocation("gViewProj"),1,GL_FALSE,glm::value_ptr(viewProjMatrix));
		glUniformMatrix4fv(shader.getUniformLocation("gModelView"),1,GL_FALSE,glm::value_ptr(modelViewMatrix));
		glUniform3fv(shader.getUniformLocation("gDiff"),1,glm::value_ptr(glm::vec3(diff.x,-diff.z,diff.y)));
		glUniform4fv(shader.getUniformLocation("gColor"),1,glm::value_ptr(color));

		// draw elements
		glBindVertexArray(vao);
		glDrawElements(GL_LINES, 2, GL_UNSIGNED_INT, NULL);
		glBindVertexArray(0);
	}
}