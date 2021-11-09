#version 330 core

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_texCoord;

uniform mat4 modelOrtho;
uniform vec2 uvMul;
uniform vec2 uvAdd;

out vec2 texCoord;

void main() {
        gl_Position = modelOrtho * vec4(in_position, 1);
        texCoord = in_texCoord * uvMul + uvAdd;
}
