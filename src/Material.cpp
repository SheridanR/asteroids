// Material.cpp

#include "Main.hpp"
#include "Engine.hpp"
#include "Renderer.hpp"
#include "Material.hpp"
#include "Image.hpp"

Material::Material(const char* _name) : Asset(_name) {
	path = mainEngine->buildPath(_name).get();
	loaded = FileHelper::readObject(path.get(), *this);
}

Material::~Material() {
}

void Material::serialize(FileInterface* file) {
	int version = 0;
	file->property("Material::version", version);
	file->property("program", shader);
	file->property("textures", stdTextureStrs);
	if (file->isReading()) {
		for (auto& path : stdTextureStrs) {
			Image* image = mainEngine->getImageResource().dataForString(path.get());
			if (image) {
				stdTextures.push(image);
			}
		}
	}
}

unsigned int Material::bindTextures() {
	ArrayList<Image*>& images = stdTextures;
	unsigned int textureNum = 0;

	// bind normal textures
	if( images.getSize()==0 ) {
		Renderer* renderer = mainEngine->getRenderer();
		if( !renderer ) {
			return 0;
		} else if( !renderer->isInitialized() ) {
			return 0;
		}

		glUniform1i(shader.getUniformLocation("gTexture"), 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,renderer->getNullImage()->getTexID());
		++textureNum;
	} else if( images.getSize()==1 ) {
		glUniform1i(shader.getUniformLocation("gTexture"), 0);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D,images[0]->getTexID());
		++textureNum;
	} else if( images.getSize()>1 ) {
		for( size_t index = 0; index < images.getSize() && textureNum < GL_MAX_TEXTURE_IMAGE_UNITS; ++index, ++textureNum ) {
			Image* image = images[index];

			char buf[32] = { 0 };
			snprintf(buf,32,"gTexture[%d]",(int)index);

			glUniform1i(shader.getUniformLocation(buf), textureNum);
			glActiveTexture(GL_TEXTURE0+textureNum);
			glBindTexture(GL_TEXTURE_2D, image->getTexID());
		}
	}

	return textureNum;
}