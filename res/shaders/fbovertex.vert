#version 330 core

in vec2 position;
in vec2 texcoord;
in vec3 color;

out vec3 Color;
out vec2 Texcoord;
void main(){

Color = color;
Texcoord = texcoord; 
gl_Position = vec4(position, 1.0, 1.0);

}