#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
uniform sampler2D texture_diffuse1;

uniform vec3 objectColor;
uniform vec3 lightColor;
uniform vec3 lightPos;
uniform vec3 viewPos;

in vec3 Normal;
in vec3 FragPos;

void main()
{     
    // 检查是否是坐标轴（通过纹理坐标的特殊值来判断）
    bool isAxis = (TexCoords.x == 1.0 && TexCoords.y == 0.0) || // 红色轴
                  (TexCoords.x == 0.0 && TexCoords.y == 1.0) || // 绿色轴
                  (TexCoords.x == 0.0 && TexCoords.y == 0.0);   // 蓝色轴
    
    if (isAxis) {
        // 坐标轴使用固定颜色，不参与光照计算
        vec3 axisColor;
        if (TexCoords.x == 1.0 && TexCoords.y == 0.0) {
            axisColor = vec3(1.0, 0.0, 0.0); // 红色
        } else if (TexCoords.x == 0.0 && TexCoords.y == 1.0) {
            axisColor = vec3(0.0, 1.0, 0.0); // 绿色
        } else if (TexCoords.x == 0.0 && TexCoords.y == 0.0) {
            axisColor = vec3(0.0, 0.0, 1.0); // 蓝色
        }
        FragColor = vec4(axisColor, 1.0);
        return;
    }
    
    // 普通模型进行光照计算
    vec3 baseColor = objectColor;
    
    // 如果有纹理，使用纹理颜色
    vec3 textureColor = texture(texture_diffuse1, TexCoords).rgb;
    if (length(textureColor) > 0.1) {
        baseColor = textureColor;
    }

    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * lightColor;

    // 漫反射光
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // 镜面反射光
    float specularStrength = 0.5;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 256);
    vec3 specular = specularStrength * spec * lightColor;  

    vec3 result = (ambient + diffuse + specular) * baseColor;
    FragColor = vec4(result, 1.0);
}