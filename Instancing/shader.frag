#version 450 core

in vec4 p_color;

layout (location = 0) out vec4 f_color;

void main() {
	f_color = p_color;
}