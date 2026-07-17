#version 330 core

in vec3 vWorldNormal;
in vec2 vTexCoord;

uniform sampler2D uBaseColorTexture;
uniform vec3 uBaseColorFactor;
uniform int uHasBaseColorTexture;
uniform vec3 uLightDirection;
uniform float uAmbientStrength;

out vec4 fragColor;

void main() {
    vec4 sampledColor = uHasBaseColorTexture != 0
        ? texture(uBaseColorTexture, vTexCoord)
        : vec4(1.0);
    vec3 baseColor = sampledColor.rgb * uBaseColorFactor;
    float diffuse = max(dot(normalize(vWorldNormal), -normalize(uLightDirection)), 0.0);
    vec3 lighting = baseColor * (uAmbientStrength + (1.0 - uAmbientStrength) * diffuse);
    fragColor = vec4(lighting, sampledColor.a);
}
