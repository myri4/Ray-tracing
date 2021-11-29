#version 450 core

layout(local_size_x = 1, local_size_y = 1) in;
layout(rgba32f, binding = 5) readonly uniform image2D screenImage;
layout(rgba32f, binding = 6) writeonly uniform image2D Bright;

vec4 DownsampleBox13Tap(ivec2 uv)
{
    ivec2 texelSize = imageSize(screenImage);
    vec4 A = imageLoad(screenImage, uv + ivec2(texelSize * vec2(-1.0, -1.0)));
    vec4 B = imageLoad(screenImage, uv + ivec2(texelSize * vec2( 0.0, -1.0)));
    vec4 C = imageLoad(screenImage, uv + ivec2(texelSize * vec2( 1.0, -1.0)));
    vec4 D = imageLoad(screenImage, uv + ivec2(texelSize * vec2(-0.5, -0.5)));
    vec4 E = imageLoad(screenImage, uv + ivec2(texelSize * vec2( 0.5, -0.5)));
    vec4 F = imageLoad(screenImage, uv + ivec2(texelSize * vec2(-1.0,  0.0)));
    vec4 G = imageLoad(screenImage, uv                                      );
    vec4 H = imageLoad(screenImage, uv + ivec2(texelSize * vec2( 1.0,  0.0)));
    vec4 I = imageLoad(screenImage, uv + ivec2(texelSize * vec2(-0.5,  0.5)));
    vec4 J = imageLoad(screenImage, uv + ivec2(texelSize * vec2( 0.5,  0.5)));
    vec4 K = imageLoad(screenImage, uv + ivec2(texelSize * vec2(-1.0,  1.0)));
    vec4 L = imageLoad(screenImage, uv + ivec2(texelSize * vec2( 0.0,  1.0)));
    vec4 M = imageLoad(screenImage, uv + ivec2(texelSize * vec2( 1.0,  1.0)));

    vec2 div = (1.0 / 4.0) * vec2(0.5, 0.125);

    vec4 o = (D + E + I + J) * div.x;
    o += (A + B + G + F) * div.y;
    o += (B + C + H + G) * div.y;
    o += (F + G + L + K) * div.y;
    o += (G + H + M + L) * div.y;

    return o;
}

vec4 getBrightnessColor(const in ivec2 pixel_coords) {
  vec4 FragColor = imageLoad(screenImage, pixel_coords);

  return vec4(FragColor.rgb, 1.0) * dot(FragColor.rgb, vec3(0.2126, 0.7152, 0.0722));
}

const float weight[5] = float[] (0.06136, 0.24477, 0.38774, 0.24477, 0.06136);

void main() {
  // base pixel colour for image
  ivec2 pixel_coords = ivec2(gl_GlobalInvocationID.xy);
  vec3 result = vec3(0.f);
  
 // // Horizontal
 //     for(int i = 1; i < 5; ++i)
 //     {
 //         result += getBrightnessColor(pixel_coords + ivec2(i, 0)).rgb * weight[i];
 //         result += getBrightnessColor(pixel_coords - ivec2(i, 0)).rgb * weight[i];
 //     }
 // // Vertical
 // for(int i = 1; i < 5; ++i)
 // {
 //     result += getBrightnessColor(pixel_coords + ivec2(0, i)).rgb * weight[i];
 //     result += getBrightnessColor(pixel_coords - ivec2(0, i)).rgb * weight[i];
 // }  

  vec3 finalColor = imageLoad(screenImage, pixel_coords).rgb;// + result;
  
  // HDR tonemapping
  finalColor = finalColor / (finalColor + 1.f);
  // gamma correct
  finalColor = pow(finalColor, vec3(1.f / 2.2f));  

  imageStore(Bright, pixel_coords, vec4(finalColor, 1.f));
}