#version 330 core
out vec4 FragColor;

in vec2 texPos;
in vec4 sphereCenter;
in vec4 fragPos;

uniform vec3 lightColor;
uniform vec3 lightDirection;
uniform vec3 cameraPos;
uniform float ignoreTextures;
uniform sampler2D ourTexture;

void main() {
    float materialShininess = 5;

    vec3 lightDirectionNorm = normalize(lightDirection);

    vec3 normal = normalize(-sphereCenter.xyz + fragPos.xyz);
    vec3 viewDirection = normalize(-cameraPos + fragPos.xyz);
    vec3 reflectedLightDir = reflect(lightDirectionNorm, normal); 

    float ambientCoef = 0.05;
    vec3 ambientLight = ambientCoef * lightColor;

    float diffuseCoef = 1;
    float diffuseStrength = max(0.0, dot(normal, lightDirectionNorm));
    vec3 diffuseLight = diffuseCoef * diffuseStrength * lightColor;
    
    float specularCoef = 0.17;
    float specularStrength = pow(max(0.0, dot(viewDirection, reflectedLightDir)), materialShininess);
    vec3 specularLight = specularCoef * specularStrength * lightColor;

    vec3 totalLight = ambientLight + diffuseLight + specularLight;
    
    vec4 textureColor = texture(ourTexture, texPos);
    if (ignoreTextures == 1) textureColor = vec4(0.5, 0.5, 0.5, 1.0);

    FragColor = textureColor * vec4(totalLight, 1.0);
}
