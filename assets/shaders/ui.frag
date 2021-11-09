#version 330 core

out vec4 FragColor;

in vec2 texCoord;

uniform sampler2D quadTexture;
uniform sampler2D maskTexture;
uniform vec4 color;
uniform bool hasColor;
uniform bool hasMask;

void main() {
        if (hasColor) {
                FragColor = color;
        } else {
                FragColor = texture(quadTexture, texCoord);
        }
        if (hasMask) {
                FragColor.w *= texture(maskTexture, texCoord).x;
        }
        if (FragColor.w < 0.05) {
                discard;
        }
}
