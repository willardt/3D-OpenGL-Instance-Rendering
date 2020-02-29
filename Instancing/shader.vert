#version 450 core

layout (location = 0) in vec2 position;
layout (location = 1) in vec2 offset;
layout (location = 2) in vec4 color;

out vec4 p_color;

uniform mat4 projection;
uniform mat4 view;

void main() {
	gl_Position = projection * view * vec4(position.x + offset.x, position.y + offset.y, 0.0, 1.0);
	p_color = color;
}