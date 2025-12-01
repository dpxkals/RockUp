#version 330 core

in vec3 vertexColor;
in vec3 FragPos;
in vec3 Normal;
in vec2 TexCoord; // [추가]

out vec4 FragColor;

uniform vec3 lightPos;
uniform vec3 lightColor;
uniform vec3 viewPos;
uniform vec3 objectColor;

uniform sampler2D texture1; // [추가] 텍스처 샘플러
uniform int useTexture;     // [추가] 텍스처 사용 여부 (1: 사용, 0: 미사용)

void main() {
    // 0. 텍스처 처리
    vec3 finalObjectColor = objectColor;
    if (useTexture == 1) {
        finalObjectColor = texture(texture1, TexCoord).rgb;
    }

    // 1. 주변광 (Ambient)
    vec3 ambientLight = vec3(0.5);
    vec3 ambient = ambientLight * finalObjectColor; 

    // 2. 난반사 (Diffuse)
    vec3 normalVector = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diffuseLight = max(dot(normalVector, lightDir), 0.0);
    vec3 diffuse = diffuseLight * lightColor * finalObjectColor;

    // 3. 정반사 (Specular)
    int shininess = 32;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, normalVector);
    float specularLight = max(dot(viewDir, reflectDir), 0.0);
    specularLight = pow(specularLight, shininess);
    vec3 specular = specularLight * lightColor; 

    vec3 result = ambient + diffuse + specular;
    FragColor = vec4(result, 1.0);
}