#version 330 core

in vec3 position;
in vec2 texcoord;
in vec3 color;

out vec3 Color;
out vec2 Texcoord;

uniform mat4 matrix;

void main()
{
    Color = color;
    Texcoord = texcoord;
    gl_Position = matrix * vec4(position, 1.0);
}
