#pragma once
#include <glm/glm.hpp>

constexpr uint32_t chunkSize = 16;
constexpr uint32_t chunkVolume = chunkSize * chunkSize * chunkSize;
constexpr uint32_t numChunks = 1;

namespace wc {

	int to1D(const int& x, const int& y, const int& z, const uint32_t& size = chunkSize) { return (z * size * size) + (y * size) + x; }
	glm::ivec3 to3D(int i, const glm::ivec3& size) {
		int z = i / (size.x * size.y);
		i -= (z * size.x * size.y);
		int y = i / size.x;
		int x = i % size.x;
		return glm::ivec3(x, y, z);
	}

	struct AABB {
		glm::vec4 start;
		glm::vec4 end;
	};

	struct Chunk {
		glm::vec4 start;
		glm::vec4 end;
		glm::ivec3 position = glm::ivec3(0); // offset
		uint32_t pointerStart; // actual position
	};

	struct ChunkData {
		uint8_t data[chunkSize][chunkSize][chunkSize] = { 0 };
	};

	glm::ivec3 getBlockPos(const int& x, const int& y, const int& z)
	{
		return { (chunkSize + (x % chunkSize)) % chunkSize,
				(chunkSize + (y % chunkSize)) % chunkSize,
				(chunkSize + (z % chunkSize)) % chunkSize };
	}

	glm::ivec3 getBlockPos(const glm::ivec3& pos)
	{
		return getBlockPos(pos.x, pos.y, pos.z);
	}

	glm::ivec3 getChunkPos(const int& x, const int& y, const int& z)
	{
		glm::ivec3 res = {
			x < 0 ? ((x - chunkSize) / chunkSize) : (x / chunkSize),
			y < 0 ? ((y - chunkSize) / chunkSize) : (y / chunkSize),
			z < 0 ? ((z - chunkSize) / chunkSize) : (z / chunkSize),
		};

		glm::ivec3 localPos = getBlockPos(x, y, z);
		if (localPos.x == 0 && x < 0) res.x++;
		if (localPos.y == 0 && y < 0) res.y++;
		if (localPos.z == 0 && z < 0) res.z++;
		return res;
	}

	glm::ivec3 getChunkPos(const glm::ivec3& pos)
	{
		return getChunkPos(pos.x, pos.y, pos.z);
	}
}