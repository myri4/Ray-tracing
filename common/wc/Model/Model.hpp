#pragma once

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "Mesh.hpp"

#include <unordered_map>
#include <Maths/AssimpGLMHelpers.hpp>
#undef max
namespace wc {

	struct Model {
		Model() = default;
		std::vector<Vertex> vertices;
		std::vector<uint32_t> indices;

		void Load(const std::string& path, glm::vec4& start, glm::vec4& end) {
			Assimp::Importer importer;
			const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_OptimizeMeshes | aiProcess_JoinIdenticalVertices | aiProcess_GenNormals | aiProcess_FlipUVs);

			if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
			{
				WC_ERROR(importer.GetErrorString());
				return;
			}
			uint32_t offset = 0;
			processNode(scene->mRootNode, *scene, offset);
			start = glm::vec4(vertices[0].position, 1.f); 
			end = glm::vec4(vertices[0].position, 1.f);

			for (uint32_t i = 1; i < vertices.size(); i++) {
				start = glm::max(start, glm::vec4(vertices[i].position, 1.f));
				end = glm::min(end, glm::vec4(vertices[i].position, 1.f));
			}
		}

		void processNode(const aiNode* node, const aiScene& scene, uint32_t& offset) {
			// process each mesh located at the current node		
			// the node object only contains indices to index the actual objects in the scene. 
			// the scene contains all the data, node is just to keep stuff organized (like relations between nodes).
			for (uint32_t m = 0; m < node->mNumMeshes; m++) {
				auto& mesh = *scene.mMeshes[node->mMeshes[m]];
				for (uint32_t i = 0; i < mesh.mNumVertices; i++)
				{
					Vertex vertex;
					vertex.position = AssimpGLMHelpers::GetGLMVec(mesh.mVertices[i]);

					if (mesh.mTextureCoords[0])
						vertex.texCoord = glm::vec3(mesh.mTextureCoords[0][i].x, mesh.mTextureCoords[0][i].y, 0.f);

					vertices.push_back(vertex);
				}

				for (uint32_t i = 0; i < mesh.mNumFaces; i++)
				{
					aiFace& face = mesh.mFaces[i];
					for (uint32_t j = 0; j < face.mNumIndices; j++)
						indices.push_back(face.mIndices[j] + offset);
				}

				offset += mesh.mNumVertices;
			}

			// after we've processed all of the meshes (if any) we then recursively process each of the children nodes
			for (uint32_t i = 0; i < node->mNumChildren; i++)
				processNode(node->mChildren[i], scene, offset);
		}
	};
}