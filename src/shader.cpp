#include "shader.h"

Shader::Shader(const GLchar* vertexPath, const GLchar* fragmentPath)
{
	std::string vertexCode;
	std::string fragmentCode;
	std::ifstream vShaderFile;
	std::ifstream fShaderFile;
	// ensure ifstream objects can throw exceptions:
	vShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
	try { // open files 
		vShaderFile.open(vertexPath);
		fShaderFile.open(fragmentPath);
		std::stringstream vShaderStream, fShaderStream;
		// read file�s buffer contents into streams 
		vShaderStream << vShaderFile.rdbuf();
		fShaderStream << fShaderFile.rdbuf();
		// close file handlers 
		vShaderFile.close();
		fShaderFile.close();
		// convert stream into string
		vertexCode = vShaderStream.str();
		fragmentCode = fShaderStream.str();
	}
	catch (std::ifstream::failure e) {
		std::cout << "ERROR::SHADER::FILE_NOT_SUCCESFULLY_READ" << std::endl;
	}
	const char* vShaderCode = vertexCode.c_str();
	const char* fShaderCode = fragmentCode.c_str();

	unsigned int vertex, fragment;
	int success;
	char infoLog[512];
	// vertex Shader 
	vertex = glCreateShader(GL_VERTEX_SHADER); 
	glShaderSource(vertex, 1, &vShaderCode, NULL);
	glCompileShader(vertex); 
	// print compile errors if any 
	glGetShaderiv(vertex, GL_COMPILE_STATUS, &success); 
	if(!success) { glGetShaderInfoLog(vertex, 512, NULL, infoLog);
	std::cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std:: endl;
	};
	// similiar for Fragment Shader [...]
	fragment = glCreateShader(GL_FRAGMENT_SHADER);
	glShaderSource(fragment, 1, &fShaderCode, NULL);
	glCompileShader(fragment);
	glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(fragment, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
	};
	// shader Program 
	this->ID = glCreateProgram();
	glAttachShader(this->ID, vertex); 
	glAttachShader(this->ID, fragment); 
	glLinkProgram(this->ID);

	glGetProgramiv(this->ID, GL_LINK_STATUS, &success); 
	if (!success) { 
		glGetProgramInfoLog(this->ID, 512, NULL, infoLog);
		std::cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
	}
	// delete the shaders as they�re linked into our program now and no longer necessery 
	glDeleteShader(vertex);
	glDeleteShader(fragment);
}


void Shader::use() {
	glUseProgram(ID);
}