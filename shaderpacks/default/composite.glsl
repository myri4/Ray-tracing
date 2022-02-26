#type vertex
#version 430 core
layout (location = 0) in vec2 a_Pos;
layout (location = 1) in vec2 a_TexCoords;

out vec2 v_TexCoords;

void main()
{
    gl_Position = vec4(a_Pos, 0.0, 1.0); 
    v_TexCoords = a_TexCoords;
}  

#type fragment
#version 430 core

in vec2 v_TexCoords;

layout(binding = 0) uniform sampler2D screenTexture;
layout(binding = 1) uniform sampler2D bloomTexture;

layout(location = 0) out vec4 FragColor;

void main()
{    
    vec3 result = texture(screenTexture, v_TexCoords).rgb + texture(bloomTexture, v_TexCoords).rgb;
    
	// HDR tonemapping
	result = result / (result + 1.f);
	// gamma correct
	result = pow(result, vec3(1.f / 2.2f));  

    FragColor = vec4(result, 1.f);
}