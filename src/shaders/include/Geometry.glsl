#include "Utils.glsl"

struct Vertex {
	vec2 texCoord;
	vec3 position;
};

struct DrawCommand {
	uint count;
	uint instanceCount;
	uint firstIndex;
	uint baseVertex;
	uint bvhID;
};

struct AABB{
    vec4 start;
    vec4 end;
};

struct Chunk {
	vec4 start;
	vec4 end;
};

layout (std430, binding = 2) readonly buffer VertexData { Vertex vertices[]; };
layout (std430, binding = 3) readonly buffer IndexData { uint indices[]; };
layout (std430, binding = 4) readonly buffer DrawCommandData { DrawCommand drawCommands[]; };
layout (std430, binding = 5) readonly buffer BVH { AABB boundingBoxes[]; };

vec3 Intersection(const in vec3 rayOrigin, const in vec3 rayDirection, const in uint i, const in DrawCommand cmd) {
	uint indexOffset = i + cmd.firstIndex;
	vec3 a = vertices[indices[indexOffset + 0] + cmd.baseVertex].position;
	vec3 b = vertices[indices[indexOffset + 1] + cmd.baseVertex].position;
	vec3 c = vertices[indices[indexOffset + 2] + cmd.baseVertex].position;
	vec3 e1 = b - a;
	vec3 e2 = c - a;
    vec3 crossRDE2 = cross(rayDirection, e2);
    float dotE1CrossRDE2 = 1.f / dot(e1, crossRDE2);
	vec3 rOa = rayOrigin - a;
	vec3 crossROAE1 = cross(rOa, e1);
	vec2 uv;
	uv.x = dot(rOa, crossRDE2) * dotE1CrossRDE2;
	uv.y = dot(rayDirection, crossROAE1 * dotE1CrossRDE2);

	float t = dot(e2, crossROAE1) * dotE1CrossRDE2;
		if (!(t <= 0.f || uv.x < 0.f || uv.x > 1.f || uv.y < 0.f || uv.x + uv.y > 1.f))
			return vec3(t, uv);
		else 
			return vec3(0.f, uv);	
}

bool BoxIntersect(vec3 rayOrigin, vec3 invRayDir, vec3 boxMin, vec3 boxMax) {
    vec3 tMin = (boxMin - rayOrigin) * invRayDir;
    vec3 tMax = (boxMax - rayOrigin) * invRayDir;
	for (int a = 0; a < 3; a++){
		if (invRayDir[a] < 0.f){
			float temp = tMin[a];
			tMin[a] = tMax[a];
			tMax[a] = temp;
		}
	}
    vec3 t1 = min(tMin, tMax);
    vec3 t2 = max(tMin, tMax);
    float tNear = max(max(t1.x, t1.y), t1.z);
    float tFar = min(min(t2.x, t2.y), t2.z);
    return tNear <= tFar && tFar > 0.f;
}