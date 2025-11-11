#version 330 core

layout(location = 0) in vec3 vPos;
layout(location = 1) in vec3 vColor;

uniform mat4 model; //--- 모델링 변환값: 응용 프로그램에서 전달 ? uniform 변수로 선언: 변수 이름“model”로 받아옴
uniform mat4 view; //--- 뷰잉 변환값: 응용 프로그램에서 전달 ? uniform 변수로 선언: 변수 이름“view”로 받아옴
uniform mat4 projection; //--- 투영 변환값: 응용 프로그램에서 전달 ? uniform 변수로 선언: 변수 이름“projection”로 받아옴

out vec3 vertexColor;

void main() {
    gl_Position = projection * view * model * vec4(vPos, 1.0);
    vertexColor = vColor;
}