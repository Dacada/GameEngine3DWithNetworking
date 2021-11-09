#version 330 core

#define NUM_BONES 256

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec2 in_texCoord;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec3 in_tangent;
layout (location = 4) in vec3 in_binormal;
layout (location = 5) in vec3 in_boneidx;
layout (location = 6) in vec3 in_bonewght;

uniform mat4 modelViewProjection;
uniform mat4 modelView;
uniform mat4 invView;
uniform mat4 bones[NUM_BONES];

vec4 weighted_sum(vec3 v3, float l) {
        vec4 v = vec4(v3, l);
        
        int i = int(in_boneidx.x);
        int j = int(in_boneidx.y);
        int k = int(in_boneidx.z);
        
        float weight1 = in_bonewght.x;
        float weight2 = in_bonewght.y;
        float weight3 = in_bonewght.z;
        
        float sum = weight1 + weight2 + weight3;
        if (sum < 0.001) {
                return v;
        }
        
        weight1 /= sum;
        weight2 /= sum;
        weight3 /= sum;

        vec4 r =(bones[i]*v)*weight1 +
                (bones[j]*v)*weight2 +
                (bones[k]*v)*weight3;
        
        return vec4(r.xyz, l);
}
