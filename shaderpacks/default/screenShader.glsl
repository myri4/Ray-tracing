#type vertex
#version 450 core
layout (location = 0) in vec2 a_Pos;

void main()
{
    gl_Position = vec4(a_Pos, 0.0, 1.0); 
}  
#type fragment
#version 450 core
layout(location = 0) out vec4 FragColor;

struct Triangle{
	vec3 a;
	vec3 b;
	vec3 c;    

	vec2 texCoord1;
	vec2 texCoord2;
	vec2 texCoord3;

	vec3 normal;
};

struct Vertex {
	vec2 texCoord;
	vec3 position;
	vec3 normal;
};

struct Light {
	uint color;
	vec3 vector;
};

float rtLerp(const in vec3 uvw, const in float a, const in float b, const in float c) { return uvw.x * a + uvw.y * b + uvw.z * c; }
vec2 rtLerp(const in vec3 uvw, const in vec2 a, const in vec2 b, const in vec2 c) { return uvw.x * a + uvw.y * b + uvw.z * c; }
vec3 rtLerp(const in vec3 uvw, const in vec3 a, const in vec3 b, const in vec3 c) { return uvw.x * a + uvw.y * b + uvw.z * c; }
vec4 rtLerp(const in vec3 uvw, const in vec4 a, const in vec4 b, const in vec4 c) { return uvw.x * a + uvw.y * b + uvw.z * c; }

layout (std140, binding = 0) uniform SceneData
{
    vec2 windowSize;
	vec3 lower_left_corner;
	vec3 horizontal;
	vec3 vertical;
	vec3 cameraPos;
    uint numIndices;
    uint numLights;
	uint maxBounces;
};

layout (std140, binding = 1) uniform Lighting
{
    Light lights[1];
};

layout (std140, binding = 2) uniform VertexData
{
    Vertex vertices[1];
};

layout (std140, binding = 3) uniform IndexData
{
    uint indices[1];
};

layout(binding = 0) uniform sampler2D u_Albedo;
layout(binding = 1) uniform sampler2D u_MaterialInfo;
layout(binding = 2) uniform sampler2D u_Normal;

bool Intersection(const in vec3 rayOrigin, const in vec3 rayDirection, const in uint i, inout vec2 uv, inout float t) {
	vec3 a = vertices[indices[i]].position;
	vec3 b = vertices[indices[i + 1]].position;
	vec3 c = vertices[indices[i + 2]].position;
	vec3 e1 = b - a;
	vec3 e2 = c - a;
    vec3 crossRDE2 = cross(rayDirection, e2);
    float dotE1CrossRDE2 = 1.f / dot(e1, crossRDE2);
	vec3 rOa = rayOrigin - a;
	vec3 crossROAE1 = cross(rOa, e1);
	uv.x = dot(rOa, crossRDE2) * dotE1CrossRDE2;
	uv.y = dot(rayDirection, crossROAE1 * dotE1CrossRDE2);

	t = dot(e2, crossROAE1) * dotE1CrossRDE2;
		
	return !(t <= 0.f || uv.x < 0.f || uv.x > 1.f || uv.y < 0.f || uv.x + uv.y > 1.f);	
}

const float Epsilon = 1.192092896e-07F;
const float bias = 1e-4;

const float PI = 3.14159265358979323846264338327950288f;

vec3 getNormalFromMap(const in vec3 N, const in vec3 p0, const in vec2 TexCoords)
{
    vec3 tangentNormal = texture(u_Normal, TexCoords).rgb * 2.f - 1.f;

    vec3 Q1  = dFdx(p0);
    vec3 Q2  = dFdy(p0);
    vec2 st1 = dFdx(TexCoords);
    vec2 st2 = dFdy(TexCoords);

    vec3 T   = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B   = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return -normalize(TBN * tangentNormal);
}

float DistributionGGX(const in vec3 N, const in vec3 H, const in float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.f);
	float NdotH2 = NdotH * NdotH;

	float nom = a2;
	float denom = NdotH2 * (a2 - 1.f) + 1.f;
	denom = PI * denom * denom;

	return nom / denom;
}
// ----------------------------------------------------------------------------
float GeometrySchlickGGX(const in float NdotV, const in float roughness)
{
	float r = roughness + 1.f;
	float k = r * r * 0.125f;

	//float nom = NdotV;
	//float denom = NdotV * (1.f - k) + k;
	//	   nom   / denom
	return NdotV / (NdotV * (1.f - k) + k);
}

vec3 fresnelSchlick(const in float cosTheta, const in vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
// ----------------------------------------------------------------------------
vec3 fresnelSchlickRoughness(const in float cosTheta, const in vec3 F0, const in float roughness)
{
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

/*sky start*/
float hash( const in float n ) {
	return fract(sin(n)*4378.5453);
}

float pnoise(in vec3 o) 
{
	vec3 p = floor(o);
	vec3 fr = fract(o);
		
	float n = p.x + p.y*57.0 + p.z * 1009.0;

	float a = hash(n+  0.0);
	float b = hash(n+  1.0);
	float c = hash(n+ 57.0);
	float d = hash(n+ 58.0);
	
	float e = hash(n+  0.0 + 1009.0);
	float f = hash(n+  1.0 + 1009.0);
	float g = hash(n+ 57.0 + 1009.0);
	float h = hash(n+ 58.0 + 1009.0);
	
	
	vec3 fr2 = fr * fr;
	vec3 fr3 = fr2 * fr;
	
	vec3 t = 3.0 * fr2 - 2.0 * fr3;
	
	float u = t.x;
	float v = t.y;
	float w = t.z;

	// this last bit should be refactored to the same form as the rest :)
	float res1 = a + (b-a)*u +(c-a)*v + (a-b+d-c)*u*v;
	float res2 = e + (f-e)*u +(g-e)*v + (e-f+h-g)*u*v;
	
	float res = res1 * (1.0- w) + res2 * (w);
	
	return res;
}

const mat3 m = mat3( 0.00,  0.80,  0.60,
                    -0.80,  0.36, -0.48,
                    -0.60, -0.48,  0.64 );

float SmoothNoise( vec3 p )
{
    float f;
    f  = 0.5000*pnoise( p ); p = m*p*2.02;
    f += 0.2500*pnoise( p ); 
	
    return f * (1.0 / (0.5000 + 0.2500));
}

vec3 getStars(in vec3 from, in vec3 dir, int levels, float power) 
{
	vec3 color=vec3(0.0);
	vec3 st = (dir * 2.+ vec3(0.3,2.5,1.25)) * .3;
	for (int i = 0; i < levels; i++) st = abs(st) / dot(st,st) - .9;
    float star = min( 1., pow( min( 5., length(st) ), 3. ) * .0025 )*1.5;

   	vec3 randc = vec3(SmoothNoise( dir.xyz*10.0*float(levels) ), SmoothNoise( dir.xzy*10.0*float(levels) ), SmoothNoise( dir.yzx*10.0*float(levels) ));
	color += star * randc;

	return pow(color*2.25, vec3(power));
}

float earthRadius = 6360e3;      // In the paper this is usually Rg or Re (radius ground, eart) 
float atmosphereRadius = 6420e3; // In the paper this is usually R or Ra (radius atmosphere) 
float Hr = 7994.f;               // Thickness of the atmosphere if density was uniform (Hr) 
float Hm = 1200.f;               // Same as above but for Mie scattering (Hm)
vec3 betaR = vec3(3.8e-6f, 13.5e-6f, 33.1e-6f); 
vec3 betaM = vec3(21e-6f); 
const float kInfinity = 3.402823466e+38F;

vec3 position = vec3(0.f);
bool raySphereIntersect(const in vec3 rayOrigin, const in vec3 rayDirection, const in float radius, inout float t0, inout float t1) {
	vec3 L = position - rayOrigin;
	float tca = dot(L, rayDirection);

	//if (tca < 0) return false;

	float s2 = (dot(L, L)) - (tca * tca);

	if (s2 > radius * radius) return false;	

	float thc = sqrt((radius * radius) - s2);
	t0 = tca - thc; 
    t1 = tca + thc;

	if (t0 > t1) {
		float t = t0;
		t0 = t1;
		t1 = t;
	}

	return true;
}

vec3 computeIncidentLight(const in vec3 orig, const in vec3 dir, out float mixer) 
{ 
    uint numSamples = 16; 
    uint numSamplesLight = 8; 
    float t0 = 0.f, t1 = 0.f; 
    float g = 0.78f; 
	vec3 sunDirection = lights[0].vector;
	float tmin = 0.f, tmax = kInfinity;
    if (!raySphereIntersect(orig, dir, atmosphereRadius, t0, t1) || t1 < 0) return vec3(0.f); 
    if (t0 > tmin && t0 > 0) tmin = t0; 
    if (t1 < tmax) tmax = t1; 
    float segmentLength = (tmax - tmin) / numSamples; 
    float tCurrent = tmin; 
    vec3 sumR = vec3(0.f), sumM = vec3(0.f); // mie and rayleigh contribution 
    float opticalDepthR = 0.f, opticalDepthM = 0.f; 
    float mu = dot(dir, sunDirection); // mu in the paper which is the cosine of the angle between the sun direction and the ray direction 
    float phaseR = 3.f / (16.f * PI) * (1 + mu * mu); 
    float phaseM = 3.f / (8.f * PI) * ((1.f - g * g) * (1.f + mu * mu)) / ((2.f + g * g) * pow(1.f + g * g - 2.f * g * mu, 1.5f)); 
    for (uint i = 0; i < numSamples; ++i) { 
        vec3 samplePosition = orig + (tCurrent + segmentLength * 0.5f) * dir; 
        float height = length(samplePosition) - earthRadius; 
        // compute optical depth for light
        float hr = exp(-height / Hr) * segmentLength; 
        float hm = exp(-height / Hm) * segmentLength; 
        opticalDepthR += hr; 
        opticalDepthM += hm; 
        // light optical depth
        float t0Light = 0.f, t1Light = 0.f; 
        raySphereIntersect(samplePosition, sunDirection, atmosphereRadius, t0Light, t1Light); 
        float segmentLengthLight = t1Light / numSamplesLight, tCurrentLight = 0; 
        float opticalDepthLightR = 0.f, opticalDepthLightM = 0.f; 
        uint j = 0; 
        for (; j < numSamplesLight; ++j) { 
            vec3 samplePositionLight = samplePosition + (tCurrentLight + segmentLengthLight * 0.5f) * sunDirection; 
            float heightLight = length(samplePositionLight) - earthRadius; 
            if (heightLight < 0) break; 
            opticalDepthLightR += exp(-heightLight / Hr) * segmentLengthLight; 
            opticalDepthLightM += exp(-heightLight / Hm) * segmentLengthLight; 
            tCurrentLight += segmentLengthLight; 
        } 
        if (j == numSamplesLight) { 
            vec3 tau = betaR * (opticalDepthR + opticalDepthLightR) + betaM * 1.1f * (opticalDepthM + opticalDepthLightM); 
            vec3 attenuation = vec3(exp(-tau)); 
            sumR += attenuation * hr; 
            sumM += attenuation * hm; 
        } 
        tCurrent += segmentLength; 
    } 
	mixer = phaseR;
    return (sumR * betaR * phaseR + sumM * betaM * phaseM) * 20.f;
}
/*sky end*/

vec3 ambientColor = vec3(0.03f);

vec3 rayTrace(inout vec3 refRayOrigin, inout vec3 refRayDirection, inout vec3 ref) {
	vec3 color = vec3(0.f);
	float minT = kInfinity;
	uint shapeHit = 0;
	vec2 uv;
	bool triangleFound = false;
	float t = 0.f;
	Triangle triangle;
	Triangle testTriangle;

	for (uint i = 0; i < numIndices; i += 3) {
		vec2 uv2;
		
		testTriangle.texCoord3 = vertices[indices[i]].texCoord;
		testTriangle.texCoord1 = vertices[indices[i + 1]].texCoord;
		testTriangle.texCoord2 = vertices[indices[i + 2]].texCoord;

		testTriangle.normal = vertices[indices[i]].normal;

		if (Intersection(refRayOrigin, refRayDirection, i, uv2, t) && t < minT) {			
			minT = t;
			uv = uv2;
			shapeHit = i;
			triangleFound = true;
			triangle = testTriangle;
		}
	}

	if (triangleFound) {
		vec3 p0 = refRayOrigin + minT * refRayDirection;

		vec3 uvw = vec3(uv, 1.f - uv.x - uv.y);
		vec2 texCoords = rtLerp(uvw, triangle.texCoord1, triangle.texCoord2, triangle.texCoord3);

		vec4 materialInfo = texture(u_MaterialInfo, texCoords);
		vec3 albedo = texture(u_Albedo, texCoords).rgb;
		float ao = materialInfo.r;
		float roughness = materialInfo.g;
		float metallic = materialInfo.b;
		
		vec3 N = getNormalFromMap(triangle.normal, p0, texCoords);

		if (dot(refRayDirection, N) > 0.f) N = -N;

		float NdotV = max(dot(N, -refRayDirection), 0.f);
		float GSNVRoughness = GeometrySchlickGGX(NdotV, roughness);

		// calculate reflectance at normal incidence; if dia-electric (like plastic) use F0
		// of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
		vec3 F0 = mix(vec3(0.04f), albedo, metallic);

		const float c = 1.f / 255.f;
		// reflectance equation
		vec3 Lo = vec3(0.f);
		for (uint i = 0; i < numLights; i++)
		{
			// calculate per-light radiance
			vec3 radiance;
			uint color = lights[i].color;
			radiance.r = float((color & uint(0x000000ff))) * c;
			radiance.g = float((color & uint(0x0000ff00)) >> 8) * c;
			radiance.b = float((color & uint(0x00ff0000)) >> 16) * c;
			float radius = float((color & uint(0xff000000)) >> 24) * c;

			vec3 L = lights[i].vector;
			if (radius > 0.f){
				L -= p0;
				radiance /= dot(L, L) * radius; // radiance / attenuation
			}
			L = normalize(L);

			vec3 H = normalize(L - refRayDirection);
			
			bool LightHit = false;
			float t0 = 0.f;
			for (uint i = 0; i < numIndices; i += 3) {
				vec2 uv2;
			
				if (Intersection(p0 + 0.000001 * N, L, i, uv2, t0)){
					LightHit = true;
					minT = t0;
				} 
			}

			if (!LightHit){
				float NdotL = max(dot(N, L), 0.f);
				// Cook-Torrance BRDF
				//float NDF = DistributionGGX(N, H, roughness);
				float G = GSNVRoughness * GeometrySchlickGGX(NdotL, roughness) * DistributionGGX(N, H, roughness);
				// fresnelSchlick
				vec3 F = fresnelSchlick(max(dot(H, -refRayDirection), 0.f), F0);

				vec3 numerator = G * F;
				float denominator = 4.f * NdotV * NdotL + 0.001f; // 0.001 to prevent divide by zero.
				vec3 specular = numerator / denominator;

				vec3 kD = (1.f - F) * (1.f - metallic); // diffuse
				Lo += ((kD * albedo / PI + specular) * radiance * NdotL);  // note that we already multiplied the BRDF by the Fresnel (kS) so we won't multiply by kS again
			}
		}

		// ambient lighting (note that the next IBL tutorial will replace
		// this ambient lighting with environment lighting).

		vec3 ambient = ambientColor * ao * albedo;

		color = ambient + Lo;
		ref = albedo * fresnelSchlickRoughness(NdotV, F0, roughness);
		refRayOrigin = p0 + 0.000001 * N;
		refRayDirection = reflect(refRayDirection, N);
		//color = vec3(5.f, 0.f, 0.f);
	}
	else{
		vec3 color1 = clamp(getStars(refRayOrigin, refRayDirection, 1, 0.5) * 1.5, 0.0, 1.0) * vec3(0.0, 0.0, 1.0);
		vec3 color2 = clamp(getStars(refRayOrigin, -refRayDirection, 2, 0.5) * 0.9, 0.0, 1.0) * vec3(1.0, 0.0, 0.0);
		vec3 color3 = clamp(getStars(refRayOrigin, -refRayDirection, 3, 0.5) * 0.7, 0.0, 1.0) * vec3(1.0, 1.0, 0.0);
		
		vec3 colorStars = getStars(refRayOrigin, refRayDirection, 17, 0.9);
		float mixer = 0.f;
		vec3 daySky = computeIncidentLight(vec3(refRayOrigin.x, earthRadius + 1.f, refRayOrigin.z), refRayDirection, mixer);
		vec3 nightSky = color1 + color2 + color3 + colorStars;
		color = mix(daySky, nightSky, mixer);
	}

    return color;
}

vec3 Render(const in vec3 rayDirection){
	vec3 refRayOrigin = cameraPos;
	vec3 refRayDirection = rayDirection;
	float blendFactor = 0.f;
	vec3 color, ref, fil = vec3(1.f);//

	for (uint i = 0; i < maxBounces; i++){
		vec3 pass = rayTrace(refRayOrigin, refRayDirection, ref); 
		color += pass * fil;

		fil *= ref;
	}

	return color;
}

void main()
{
    vec2 uv = gl_FragCoord.xy / windowSize;
    vec3 rayDir = normalize(lower_left_corner + uv.x * horizontal - uv.y * vertical - cameraPos);
	vec3 result = Render(rayDir);
	
	// HDR tonemapping
	result = result / (result + 1.f);
	// gamma correct
	result = pow(result, vec3(1.f / 2.2f));  

    FragColor = vec4(result, 1.f);
}