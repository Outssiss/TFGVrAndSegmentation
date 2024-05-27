#version 330 core

out vec4 frag_color;
in vec3 Color;
in vec2 Texcoord;

uniform sampler2D tex;

void main()
{
    frag_color = texture(tex, Texcoord) * vec4(Color, 1.0);
} 