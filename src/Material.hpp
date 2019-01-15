// Material.hpp
// Materials combine images, shaders, and other meta-data into a single class

#pragma once

#include "ArrayList.hpp"
#include "Asset.hpp"
#include "Image.hpp"
#include "ShaderProgram.hpp"

class Material : public Asset {
public:
	Material() {}
	Material(const char* _name);
	virtual ~Material();

	// binds all the material textures (should be called after the shader is mounted)
	// @return the next unused texture unit
	unsigned int bindTextures();

	// save/load this object to a file
	// @param file interface to serialize with
	virtual void serialize(FileInterface * file) override;

	// getters & setters
	virtual const type_t		getType() const				{ return ASSET_MATERIAL; }
	const ShaderProgram&		getShader() const			{ return shader; }
	ShaderProgram&				getShader()					{ return shader; }

private:
	ShaderProgram shader;
	ArrayList<Image*> stdTextures;
	ArrayList<String> stdTextureStrs;
};