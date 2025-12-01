#version 330 core

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vColor;
layout(location = 3) in vec2 vTexCoord; // [추가] 텍스처 좌표

out vec3 FragPos;
out vec3 Normal;
out vec3 vertexColor;
out vec2 TexCoord; // [추가] 프래그먼트 셰이더로 전달

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    gl_Position = projection * view * model * vec4(vPos, 1.0);
    FragPos = vec3(model * vec4(vPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * vNormal; // [수정] 노말 보정 (스케일링 시 안전)
    vertexColor = vColor;
    TexCoord = vTexCoord; // [추가]
}