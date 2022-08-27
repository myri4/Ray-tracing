#pragma once

#include <glad/glad.h>
#include <stb_image/stb_image.h>
#include <glm/glm.hpp>

namespace gl {
	class Texture3D {
		uint32_t width = 16, height = 16, depth = 16;

		uint32_t m_RendererID = 0;
	public:
		Texture3D() {}

		void Create(const glm::ivec3& size) {
			width = size.x;
			height = size.y;
			depth = size.z;
			glCreateTextures(GL_TEXTURE_3D, 1, &m_RendererID);
			glTextureStorage3D(m_RendererID, 1, GL_R8UI, width, height, depth);
		}

		~Texture3D() { glDeleteTextures(1, &m_RendererID); }

		void setData(const void* data, const glm::ivec3& size, const glm::ivec3& offset = glm::ivec3(0)) {
			glTextureSubImage3D(m_RendererID, 0, offset.x, offset.y, offset.z, size.x, size.y, size.z, GL_RED_INTEGER, GL_UNSIGNED_BYTE, data);
		}

		void Bind(const uint32_t& unit = 0) { glBindTextureUnit(unit, m_RendererID); }

		void BindTextureImage(const uint8_t& textureUnit = 0, const GLenum& access = GL_READ_ONLY, const uint32_t& level = 0, const bool& layered = false, const int32_t& layer = 0) 
		{ glBindImageTexture(textureUnit, m_RendererID, level, layered, layer, access, GL_R8UI); }

		inline operator uint32_t& () { return m_RendererID; }
		inline operator const uint32_t& () const { return m_RendererID; }
	};
}