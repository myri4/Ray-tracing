#version 450 core

layout(local_size_x = 1, local_size_y = 1) in;
layout(binding = 5) uniform sampler2D screenImage;
layout(rgba32f, binding = 6) uniform image2D Bright;

const float weight[5] = float[] (0.2270270270, 0.1945945946, 0.1216216216, 0.0540540541, 0.0162162162);

vec4 getBrightnessColor(const in vec2 TexCoords, const in int mip) {
  vec4 FragColor = textureLod(screenImage, TexCoords, mip);
  float Brightness = dot(FragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
  if (Brightness > 0.2f) 
    return vec4(FragColor.rgb, 1.0);
  else 
    return vec4(0.f, 0.f, 0.f, 1.f);
}

vec3 GetBloom(const in ivec2 pixel_coords, const in int mip){
    vec2 TexCoords = vec2(pixel_coords) / vec2(textureSize(screenImage, mip));
    vec2 tex_offset = 1.0 / textureSize(screenImage, mip); // gets size of single texel
  
    vec3 result = getBrightnessColor(TexCoords, mip).rgb * weight[0]; // current fragment's contribution
    //Horizontal
    for(int i = 1; i < 5; ++i)
    {
        result += getBrightnessColor(TexCoords + vec2(tex_offset.x * i, 0.0), mip).rgb * weight[i];
        result += getBrightnessColor(TexCoords - vec2(tex_offset.x * i, 0.0), mip).rgb * weight[i];
    }

    //Vertical    
    for(int i = 1; i < 5; ++i)
    {
        result += getBrightnessColor(TexCoords + vec2(0.0, tex_offset.y * i), mip).rgb * weight[i];
        result += getBrightnessColor(TexCoords - vec2(0.0, tex_offset.y * i), mip).rgb * weight[i];
    }

    return result;
}

void main() {
  // base pixel colour for image
  ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);
  vec2 TexCoords = vec2(pixel_coords) / vec2(textureSize(screenImage, 0));

  //vec3 result = GetBloom(pixel_coords, 0);    

  vec3 finalColor = textureLod(screenImage, TexCoords, 0).rgb;// + result;
  
  // HDR tonemapping
  finalColor = finalColor / (finalColor + 1.f);
  // gamma correct
  finalColor = pow(finalColor, vec3(1.f / 2.2f));  

  imageStore(Bright, pixel_coords, vec4(finalColor, 1.f));
}