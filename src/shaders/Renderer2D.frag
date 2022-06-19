#version 450

layout(location = 0) in vec3 v_TexCoords;
layout(location = 1) in vec4 v_Color;
layout(location = 2) in float v_Type;

layout(binding = 0) uniform sampler2D u_Texture[32];

layout(location = 0) out vec4 FragColor;
void main()
{
    int index = int(v_TexCoords.z);
    vec4 finalColor = vec4(1.f);
    if(v_Type == 0.f) finalColor = texture(u_Texture[index], v_TexCoords.xy);
    else if(v_Type == 1.f) finalColor = vec4(1.f, 1.f, 1.f, texture(u_Texture[index], v_TexCoords.xy).r);

    FragColor = finalColor * v_Color;
}