
out vec3 texCoord;

void main() {
        texCoord = mat3(0,0,1,1,0,0,0,1,0) * in_position;
        vec4 pos = modelViewProjection * vec4(in_position, 1);
        gl_Position = pos.xyww;
}
