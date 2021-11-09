#define LIGHTTYPE_SPOT uint(0)
#define LIGHTTYPE_DIRECTION uint(1)
#define LIGHTTYPE_POINT uint(2)
#define NUM_LIGHTS 20

struct Material {
        vec4 globalAmbient;
        
        vec4 ambientColor;
        vec4 emissiveColor;
        vec4 diffuseColor;
        vec4 specularColor;
        
        float opacity;
        float reflectance;
        float refraction;
        float indexOfRefraction;
        float specularPower;
        
        bool hasAmbientTexture;
        bool hasEmissiveTexture;
        bool hasDiffuseTexture;
        bool hasSpecularTexture;
        bool hasSpecularPowerTexture;
        bool hasNormalTexture;
        bool hasBumpTexture;
        bool hasOpacityTexture;
        
        float bumpIntensity;
        float specularScale;
        
        float alphaThreshold;
        bool alphaBlendingMode;
};

struct Light {
        bool enabled;

        float attenuation_constant;
        float attenuation_linear;
        float attenuation_quadratic;
        
        vec4 color;
        float intensity;

        uint type;
        
        vec4 position_ws;
        vec4 position_vs;
        vec4 direction_ws;
        vec4 direction_vs;
        float angle;
};

struct LightingResult {
        vec4 diffuse;
        vec4 specular;
};

in vec2 texCoord;
in vec3 position_vs;
in vec3 tangent_vs;
in vec3 binormal_vs;
in vec3 normal_vs;

in mat4 invView_out;

uniform sampler2D ambientTexture;
uniform sampler2D emissiveTexture;
uniform sampler2D diffuseTexture;
uniform sampler2D specularTexture;
uniform sampler2D specularPowerTexture;
uniform sampler2D normalTexture;
uniform sampler2D bumpTexture;
uniform sampler2D opacityTexture;

uniform samplerCube environment;

uniform Material material;
uniform Light lights[NUM_LIGHTS];

vec3 expandNormal(vec3 normal) {
        return normal * 2.0f - 1.0f;
}

vec4 doNormalMapping(mat3 TBN, sampler2D tex, vec2 uv) {
        vec3 normal = texture(tex, uv).xyz;
        normal = expandNormal(normal);
        normal *= TBN;
        return normalize(vec4(normal, 0));
}

// Might be incorrect way to do bump mapping
vec4 doBumpMapping(mat3 TBN, sampler2D tex, vec2 uv, float bumpScale) {
        float height = texture(tex, uv).x * bumpScale;
        float heightU = textureOffset(tex, uv, ivec2(1, 0)).x * bumpScale;
        float heightV = textureOffset(tex, uv, ivec2(0, 1)).x * bumpScale;

        vec3 p = vec3(0, 0, height);
        vec3 pU = vec3(1, 0, heightU);
        vec3 pV = vec3(0, 1, heightV);

        vec3 normal = cross(normalize(pU-p), normalize(pV-p));
        normal *= TBN;
        return vec4(normal, 0);
}

vec4 doDiffuse(Light light, vec4 L, vec4 N) {
        return light.color * max(dot(N, L), 0);
}

vec4 doSpecular(Light light, Material mat, vec4 V, vec4 L, vec4 N) {
        vec4 R = normalize(reflect(-L, N));
        return light.color * pow(max(dot(R, V), 0), material.specularPower);
}

float doAttenuation(Light light, float d) {
        return 1.0f / (light.attenuation_constant +
                       light.attenuation_linear*d +
                       light.attenuation_quadratic*d*d);
}

LightingResult doPointLight(Light light, Material mat,
                            vec4 V, vec4 P, vec4 N) {
        vec4 L = light.position_vs - P;
        float dist = length(L);
        L /= dist;
        
        float attenuation = doAttenuation(light, dist);

        LightingResult result;
        result.diffuse = doDiffuse(light, L, N)
                * attenuation * light.intensity;
        result.specular = doSpecular(light, mat, V, L, N)
                * attenuation * light.intensity;
        return result;
}

float doSpotCone(Light light, vec4 L) {
        float minCos = cos(light.angle);
        float maxCos = mix(minCos, 1, 0.5f);
        float cosAngle = dot(light.direction_vs, -L);
        return smoothstep(minCos, maxCos, cosAngle);
}

LightingResult doSpotLight(Light light, Material mat,
                           vec4 V, vec4 P, vec4 N) {
        vec4 L = light.position_vs - P;
        float dist = length(L);
        L /= dist;

        float attenuation = doAttenuation(light, dist);
        float spotIntensity = doSpotCone(light, L);

        LightingResult result;
        result.diffuse = doDiffuse(light, L, N) *
                attenuation * spotIntensity * light.intensity;
        result.specular = doSpecular(light, mat, V, L, N) *
                attenuation * spotIntensity * light.intensity;
        return result;
}

LightingResult doDirectionalLight(Light light, Material mat,
                                  vec4 V, vec4 P, vec4 N) {
        vec4 L = normalize(-light.direction_vs);

        LightingResult result;
        result.diffuse = doDiffuse(light, L, N) * light.intensity;
        result.specular = doSpecular(light, mat, V, L, N) * light.intensity;
        return result;
}

LightingResult doLighting(Material mat, vec4 eyePos, vec4 P, vec4 N) {
        LightingResult totalResult;
        totalResult.diffuse = vec4(0, 0, 0, 0);
        totalResult.specular = vec4(0, 0, 0, 0);
        
        vec4 V = normalize(eyePos - P);

        for (int i=0; i<NUM_LIGHTS; i++) {
                if (!lights[i].enabled) {
                        continue;
                }

                LightingResult result;
                
                switch (lights[i].type) {
                case LIGHTTYPE_SPOT:
                        result = doSpotLight(lights[i], mat, V, P, N);
                        break;
                case LIGHTTYPE_DIRECTION:
                        result = doDirectionalLight(lights[i], mat, V, P, N);
                        break;
                case LIGHTTYPE_POINT:
                        result = doPointLight(lights[i], mat, V, P, N);
                        break;
                default:
                        result.diffuse = vec4(0, 0, 0, 0);
                        result.specular = vec4(0, 0, 0, 0);
                        break;
                }

                totalResult.diffuse += result.diffuse;
                totalResult.specular += result.specular;
        }

        totalResult.diffuse = totalResult.diffuse;
        totalResult.specular = totalResult.specular;

        return totalResult;
}

void main() {
        vec4 eyePos = vec4(0, 0, 0, 1);
        Material mat = material;

        vec4 diffuse = mat.diffuseColor;
        if (mat.hasDiffuseTexture) {
                diffuse *= texture(diffuseTexture, texCoord);
        }

        float alpha = diffuse.w;
        if (mat.hasOpacityTexture) {
                alpha = texture(opacityTexture, texCoord).x;
        }
        alpha *= mat.opacity;
        if (alpha < mat.alphaThreshold) {
                discard;
        }
        if (!mat.alphaBlendingMode) {
                alpha = 1.0f;
        }

        vec4 ambient = mat.ambientColor;
        if (mat.hasAmbientTexture) {
                ambient *= texture(ambientTexture, texCoord);
        }
        ambient *= mat.globalAmbient;

        vec4 emissive = mat.emissiveColor;
        if (mat.hasEmissiveTexture) {
                emissive *= texture(emissiveTexture, texCoord);
        }

        if (mat.hasSpecularPowerTexture) {
                mat.specularPower =
                        texture(specularPowerTexture, texCoord).x
                        * mat.specularScale;
        }

        vec4 N;
        if (mat.hasNormalTexture) {
                mat3 TBN = mat3(normalize(tangent_vs),
                                normalize(binormal_vs),
                                normalize(normal_vs));
                N = doNormalMapping(TBN, normalTexture, texCoord);
        } else if (mat.hasBumpTexture) {
                mat3 TBN = mat3(normalize(tangent_vs),
                                normalize(-binormal_vs),
                                normalize(normal_vs));
                N = doBumpMapping(TBN, bumpTexture, texCoord,
                                  mat.bumpIntensity);
        } else {
                N = normalize(vec4(normal_vs, 0));
        }

        vec4 P = vec4(position_vs, 1);
        LightingResult lit = doLighting(mat, eyePos, P, N);

        lit.diffuse += ambient;
        diffuse *= vec4(lit.diffuse.xyz, 1.0f);

        vec4 specular = vec4(0, 0, 0, 0);
        if (mat.specularPower > 1.0f) {
                specular = mat.specularColor;
                if (mat.hasSpecularTexture) {
                        specular *=
                                texture(specularTexture, texCoord);
                }
                specular *= lit.specular;
        }
        
        FragColor = vec4((emissive + diffuse + specular).xyz, alpha);
        if (mat.reflectance > 0) {
                vec4 I = normalize(P - eyePos);
                vec3 R = normalize(reflect(I, N).xyz);
                R = mat3(0,0,1,1,0,0,0,1,0)*(invView_out*vec4(R,0)).xyz;
                
                FragColor *= 1 - mat.reflectance;
                FragColor += texture(environment, R);
        }
        if (mat.refraction > 0) {
                vec4 I = normalize(P - eyePos);
                vec3 R = normalize(refract(I, N, mat.indexOfRefraction).xyz);
                R = mat3(0,0,1,1,0,0,0,1,0)*(invView_out*vec4(R,0)).xyz;
                
                FragColor *= 1 - mat.refraction;
                FragColor += texture(environment, R);
        }
}
