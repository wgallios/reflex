#version 330 core
layout (location = 0) in vec3 aPosition;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoord;
layout (location = 3) in uvec4 aJoints;
layout (location = 4) in vec4 aWeights;

uniform mat4 uModel;
uniform mat4 uView;
uniform mat4 uProjection;
uniform mat4 uJoints[128];

out vec3 vNormal;
out vec2 vTexCoord;

void main() {
    mat4 skin = aWeights.x * uJoints[aJoints.x] + aWeights.y * uJoints[aJoints.y] +
                aWeights.z * uJoints[aJoints.z] + aWeights.w * uJoints[aJoints.w];
    vec4 world = uModel * skin * vec4(aPosition, 1.0);
    vNormal = mat3(transpose(inverse(uModel * skin))) * aNormal;
    vTexCoord = aTexCoord;
    gl_Position = uProjection * uView * world;
}
