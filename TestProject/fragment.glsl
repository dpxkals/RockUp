#version 330 core

in vec3 vertexColor;

in vec3 FragPos; //--- 노멀값을 계산하기 위해 객체의 위치값을 버텍스 세이더에서 받아온다.
in vec3 Normal;

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform vec3 objectColor;

void main() {
    // 1. 주변광 (Ambient)
    vec3 ambientLight = vec3(0.5); 
    vec3 ambient = ambientLight * objectColor; 

    // 2. 난반사 (Diffuse)
    vec3 normalVector = normalize (Normal); // <--- 통일된 변수명 사용: normalVector
    vec3 lightDir = normalize (lightPos - FragPos.xyz); // <--- FragPos.xyz 사용
    
    float diffuseLight = max (dot (normalVector, lightDir), 0.0); // <--- normalVector 사용
    vec3 diffuse = diffuseLight * lightColor * objectColor; // 조명색상과 객체색상을 모두 곱함

    // 3. 정반사 (Specular)
    int shininess = 64; 
    vec3 viewDir = normalize (viewPos - FragPos.xyz); // <--- FragPos.xyz 사용
    vec3 reflectDir = reflect (-lightDir, normalVector); // <--- normalVector 사용
    
    float specularLight = max (dot (viewDir, reflectDir), 0.0); 
    specularLight = pow(specularLight, shininess); 
    vec3 specular = specularLight * lightColor; // 보통 lightColor만 곱함 (하이라이트는 광원 자체의 색이므로)
    
    // 최종 색상
    vec3 result = (ambient + diffuse + specular); 
    FragColor = vec4 (result, 1.0);
}