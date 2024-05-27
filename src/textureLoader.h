#pragma once
#include <GL/glew.h>
#include <IL/ilut.h>
#include <IL/il.h>
#include <vector>
#include <stdio.h>
#include <string>

class textureLoader {
public:
	static GLuint loadImage(const char* theFileName);
	static GLuint loadCubemap(std::vector<std::string> faces);
	static void Init();
};