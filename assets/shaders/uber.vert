out vec2 texCoord;
out vec3 position_vs;
out vec3 normal_vs;
out vec3 tangent_vs;
out vec3 binormal_vs;
out mat4 invView_out;

void main() {
        vec4 boned_position = weighted_sum(in_position, 1);
        vec4 boned_normal = weighted_sum(in_normal, 0);
        vec4 boned_tangent = weighted_sum(in_tangent, 0);
        vec4 boned_binormal = weighted_sum(in_binormal, 0);
        
        gl_Position = modelViewProjection * boned_position;
        position_vs = (modelView * boned_position).xyz;
        normal_vs = mat3(modelView) * boned_normal.xyz;
        tangent_vs = mat3(modelView) * boned_tangent.xyz;
        binormal_vs = mat3(modelView) * boned_binormal.xyz;
        
        texCoord = in_texCoord;
        invView_out = invView;
}
