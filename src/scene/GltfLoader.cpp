#include "scene/GltfLoader.hpp"

#include "rendering/Mesh.hpp"
#include "scene/Scene.hpp"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
#define TINYGLTF_NO_INCLUDE_STB_IMAGE
#define TINYGLTF_NO_INCLUDE_STB_IMAGE_WRITE
#include <tiny_gltf.h>

#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/mat4x4.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

namespace {
struct LoadStatistics {
    std::size_t primitives{0};
    std::size_t vertices{0};
    std::size_t indices{0};
};

struct CpuPrimitive {
    std::vector<glm::vec3> positions;
    std::vector<std::uint32_t> indices;
};

template <typename T>
T readUnaligned(const unsigned char* data) {
    static_assert(std::is_trivially_copyable_v<T>);
    T value{};
    std::memcpy(&value, data, sizeof(T));
    return value;
}

class AccessorView {
public:
    [[nodiscard]] bool initialize(const tinygltf::Model& model, const int accessorIndex,
                                  const char* usage) {
        if (accessorIndex < 0 ||
            static_cast<std::size_t>(accessorIndex) >= model.accessors.size()) {
            std::cerr << "Invalid " << usage << " accessor index.\n";
            return false;
        }

        accessor_ = &model.accessors[static_cast<std::size_t>(accessorIndex)];
        if (accessor_->sparse.isSparse) {
            std::cerr << "Sparse accessors are not supported for " << usage << ".\n";
            return false;
        }
        if (accessor_->bufferView < 0 ||
            static_cast<std::size_t>(accessor_->bufferView) >= model.bufferViews.size()) {
            std::cerr << "The " << usage << " accessor has no valid buffer view.\n";
            return false;
        }

        const auto& view = model.bufferViews[static_cast<std::size_t>(accessor_->bufferView)];
        if (view.buffer < 0 || static_cast<std::size_t>(view.buffer) >= model.buffers.size()) {
            std::cerr << "The " << usage << " accessor references an invalid buffer.\n";
            return false;
        }

        componentSize_ = tinygltf::GetComponentSizeInBytes(accessor_->componentType);
        componentCount_ = tinygltf::GetNumComponentsInType(accessor_->type);
        if (componentSize_ <= 0 || componentCount_ <= 0) {
            std::cerr << "Unsupported component or element type in " << usage << " accessor.\n";
            return false;
        }

        const int byteStride = accessor_->ByteStride(view);
        if (byteStride < 0) {
            std::cerr << "Invalid byte stride in " << usage << " accessor.\n";
            return false;
        }
        stride_ = static_cast<std::size_t>(byteStride);

        const auto& buffer = model.buffers[static_cast<std::size_t>(view.buffer)].data;
        const std::size_t start = view.byteOffset + accessor_->byteOffset;
        const std::size_t elementSize = static_cast<std::size_t>(componentSize_) *
                                        static_cast<std::size_t>(componentCount_);
        const std::size_t end = accessor_->count == 0
            ? start
            : start + (accessor_->count - 1) * stride_ + elementSize;
        if (start > buffer.size() || end > buffer.size() || end < start) {
            std::cerr << "Out-of-bounds data in " << usage << " accessor.\n";
            return false;
        }

        data_ = buffer.data() + start;
        return true;
    }

    [[nodiscard]] std::size_t count() const noexcept { return accessor_->count; }
    [[nodiscard]] int componentCount() const noexcept { return componentCount_; }
    [[nodiscard]] int componentType() const noexcept { return accessor_->componentType; }
    [[nodiscard]] bool normalized() const noexcept { return accessor_->normalized; }
    [[nodiscard]] const unsigned char* element(const std::size_t index) const noexcept {
        return data_ + index * stride_;
    }
    [[nodiscard]] int componentSize() const noexcept { return componentSize_; }

private:
    const tinygltf::Accessor* accessor_{nullptr};
    const unsigned char* data_{nullptr};
    std::size_t stride_{0};
    int componentSize_{0};
    int componentCount_{0};
};

float readFloatComponent(const unsigned char* data, const int componentType,
                         const bool normalized) {
    switch (componentType) {
    case TINYGLTF_COMPONENT_TYPE_FLOAT:
        return readUnaligned<float>(data);
    case TINYGLTF_COMPONENT_TYPE_BYTE: {
        const auto value = readUnaligned<std::int8_t>(data);
        return normalized ? std::max(static_cast<float>(value) / 127.0F, -1.0F)
                          : static_cast<float>(value);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
        const auto value = readUnaligned<std::uint8_t>(data);
        return normalized ? static_cast<float>(value) / 255.0F : static_cast<float>(value);
    }
    case TINYGLTF_COMPONENT_TYPE_SHORT: {
        const auto value = readUnaligned<std::int16_t>(data);
        return normalized ? std::max(static_cast<float>(value) / 32767.0F, -1.0F)
                          : static_cast<float>(value);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        const auto value = readUnaligned<std::uint16_t>(data);
        return normalized ? static_cast<float>(value) / 65535.0F : static_cast<float>(value);
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        const auto value = readUnaligned<std::uint32_t>(data);
        return normalized ? static_cast<float>(static_cast<double>(value) / 4294967295.0)
                          : static_cast<float>(value);
    }
    default:
        return 0.0F;
    }
}

bool isSupportedAttributeComponent(const int componentType) {
    return componentType == TINYGLTF_COMPONENT_TYPE_FLOAT ||
           componentType == TINYGLTF_COMPONENT_TYPE_BYTE ||
           componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE ||
           componentType == TINYGLTF_COMPONENT_TYPE_SHORT ||
           componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT ||
           componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT;
}

bool readVec3Attribute(const tinygltf::Model& model, const int accessorIndex,
                       const char* usage, std::vector<glm::vec3>& output) {
    AccessorView view;
    if (!view.initialize(model, accessorIndex, usage) || view.componentCount() != 3 ||
        !isSupportedAttributeComponent(view.componentType())) {
        std::cerr << "Expected a three-component accessor for " << usage << ".\n";
        return false;
    }

    output.resize(view.count());
    for (std::size_t i = 0; i < view.count(); ++i) {
        const unsigned char* element = view.element(i);
        for (int component = 0; component < 3; ++component) {
            output[i][component] = readFloatComponent(
                element + static_cast<std::size_t>(component * view.componentSize()),
                view.componentType(), view.normalized());
        }
    }
    return true;
}

bool readVec2Attribute(const tinygltf::Model& model, const int accessorIndex,
                       const char* usage, std::vector<glm::vec2>& output) {
    AccessorView view;
    if (!view.initialize(model, accessorIndex, usage) || view.componentCount() != 2 ||
        !isSupportedAttributeComponent(view.componentType())) {
        std::cerr << "Expected a two-component accessor for " << usage << ".\n";
        return false;
    }

    output.resize(view.count());
    for (std::size_t i = 0; i < view.count(); ++i) {
        const unsigned char* element = view.element(i);
        for (int component = 0; component < 2; ++component) {
            output[i][component] = readFloatComponent(
                element + static_cast<std::size_t>(component * view.componentSize()),
                view.componentType(), view.normalized());
        }
    }
    return true;
}

bool readIndices(const tinygltf::Model& model, const int accessorIndex,
                 std::vector<std::uint32_t>& output) {
    AccessorView view;
    if (!view.initialize(model, accessorIndex, "indices") || view.componentCount() != 1) {
        return false;
    }

    output.resize(view.count());
    for (std::size_t i = 0; i < view.count(); ++i) {
        switch (view.componentType()) {
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
            output[i] = readUnaligned<std::uint8_t>(view.element(i));
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
            output[i] = readUnaligned<std::uint16_t>(view.element(i));
            break;
        case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
            output[i] = readUnaligned<std::uint32_t>(view.element(i));
            break;
        default:
            std::cerr << "Unsupported index component type " << view.componentType()
                      << "; expected unsigned byte, unsigned short, or unsigned int.\n";
            return false;
        }
    }
    return true;
}

void generateNormals(std::vector<Vertex>& vertices, const std::vector<std::uint32_t>& indices) {
    for (Vertex& vertex : vertices) {
        vertex.normal = glm::vec3{0.0F};
    }

    for (std::size_t i = 0; i + 2 < indices.size(); i += 3) {
        const std::uint32_t a = indices[i];
        const std::uint32_t b = indices[i + 1];
        const std::uint32_t c = indices[i + 2];
        if (a >= vertices.size() || b >= vertices.size() || c >= vertices.size()) {
            continue;
        }
        const glm::vec3 faceNormal = glm::cross(vertices[b].position - vertices[a].position,
                                                vertices[c].position - vertices[a].position);
        vertices[a].normal += faceNormal;
        vertices[b].normal += faceNormal;
        vertices[c].normal += faceNormal;
    }

    for (Vertex& vertex : vertices) {
        const float length = glm::length(vertex.normal);
        vertex.normal = length > 0.00001F ? vertex.normal / length
                                         : glm::vec3{0.0F, 1.0F, 0.0F};
    }
}

TextureSampler samplerFor(const tinygltf::Model& model, const tinygltf::Texture& texture) {
    TextureSampler result;
    if (texture.sampler < 0 ||
        static_cast<std::size_t>(texture.sampler) >= model.samplers.size()) {
        return result;
    }

    const tinygltf::Sampler& sampler = model.samplers[static_cast<std::size_t>(texture.sampler)];
    if (sampler.wrapS == TINYGLTF_TEXTURE_WRAP_REPEAT ||
        sampler.wrapS == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE ||
        sampler.wrapS == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT) {
        result.wrapS = sampler.wrapS;
    }
    if (sampler.wrapT == TINYGLTF_TEXTURE_WRAP_REPEAT ||
        sampler.wrapT == TINYGLTF_TEXTURE_WRAP_CLAMP_TO_EDGE ||
        sampler.wrapT == TINYGLTF_TEXTURE_WRAP_MIRRORED_REPEAT) {
        result.wrapT = sampler.wrapT;
    }
    if (sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST ||
        sampler.minFilter == TINYGLTF_TEXTURE_FILTER_LINEAR ||
        sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_NEAREST ||
        sampler.minFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST ||
        sampler.minFilter == TINYGLTF_TEXTURE_FILTER_NEAREST_MIPMAP_LINEAR ||
        sampler.minFilter == TINYGLTF_TEXTURE_FILTER_LINEAR_MIPMAP_LINEAR) {
        result.minFilter = sampler.minFilter;
    }
    if (sampler.magFilter == TINYGLTF_TEXTURE_FILTER_NEAREST ||
        sampler.magFilter == TINYGLTF_TEXTURE_FILTER_LINEAR) {
        result.magFilter = sampler.magFilter;
    }
    return result;
}

bool uploadFallbackTexture(Texture& texture) {
    constexpr std::uint8_t checkerboard[] = {
        255, 0, 255, 255, 30, 30, 30, 255,
        30, 30, 30, 255, 255, 0, 255, 255,
    };
    return texture.upload(2, 2, 4, checkerboard);
}

bool loadImageWithFallback(tinygltf::Image* image, const int imageIndex,
                           std::string*, std::string* warning,
                           const int requestedWidth, const int requestedHeight,
                           const unsigned char* bytes, const int size, void*) {
    std::string decodeError;
    if (tinygltf::LoadImageData(image, imageIndex, &decodeError, warning,
                                requestedWidth, requestedHeight, bytes, size, nullptr)) {
        return true;
    }

    if (warning != nullptr) {
        *warning += "Image " + std::to_string(imageIndex) +
                    " could not be decoded; using a checkerboard fallback. " + decodeError;
    }
    image->width = 2;
    image->height = 2;
    image->component = 4;
    image->bits = 8;
    image->pixel_type = TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE;
    image->image = {
        255, 0, 255, 255, 30, 30, 30, 255,
        30, 30, 30, 255, 255, 0, 255, 255,
    };
    return true;
}

bool uploadTexture(const tinygltf::Model& model, const tinygltf::Texture& source,
                   Texture& destination) {
    if (source.source < 0 ||
        static_cast<std::size_t>(source.source) >= model.images.size()) {
        std::cerr << "Warning: glTF texture has no valid image; using checkerboard fallback.\n";
        return uploadFallbackTexture(destination);
    }

    const tinygltf::Image& image = model.images[static_cast<std::size_t>(source.source)];
    if (image.width <= 0 || image.height <= 0 || image.image.empty()) {
        std::cerr << "Warning: glTF image could not be decoded; using checkerboard fallback.\n";
        return uploadFallbackTexture(destination);
    }
    if (image.bits != 8 ||
        image.pixel_type != TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE) {
        std::cerr << "Warning: only 8-bit texture channels are supported; using "
                     "checkerboard fallback.\n";
        return uploadFallbackTexture(destination);
    }

    if (image.component == 3 || image.component == 4) {
        return destination.upload(image.width, image.height, image.component, image.image,
                                  samplerFor(model, source));
    }

    if (image.component == 1 || image.component == 2) {
        const std::size_t pixelCount = static_cast<std::size_t>(image.width) *
                                       static_cast<std::size_t>(image.height);
        std::vector<std::uint8_t> rgba(pixelCount * 4);
        for (std::size_t i = 0; i < pixelCount; ++i) {
            const std::uint8_t value = image.image[i * static_cast<std::size_t>(image.component)];
            rgba[i * 4] = value;
            rgba[i * 4 + 1] = value;
            rgba[i * 4 + 2] = value;
            rgba[i * 4 + 3] = image.component == 2 ? image.image[i * 2 + 1] : 255;
        }
        return destination.upload(image.width, image.height, 4, rgba,
                                  samplerFor(model, source));
    }

    std::cerr << "Warning: unsupported image channel count; using checkerboard fallback.\n";
    return uploadFallbackTexture(destination);
}

glm::mat4 nodeLocalTransform(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        glm::mat4 matrix{1.0F};
        for (int column = 0; column < 4; ++column) {
            for (int row = 0; row < 4; ++row) {
                matrix[column][row] = static_cast<float>(node.matrix[
                    static_cast<std::size_t>(column * 4 + row)]);
            }
        }
        return matrix;
    }

    glm::vec3 translation{0.0F};
    if (node.translation.size() == 3) {
        translation = {static_cast<float>(node.translation[0]),
                       static_cast<float>(node.translation[1]),
                       static_cast<float>(node.translation[2])};
    }
    glm::vec3 scale{1.0F};
    if (node.scale.size() == 3) {
        scale = {static_cast<float>(node.scale[0]),
                 static_cast<float>(node.scale[1]),
                 static_cast<float>(node.scale[2])};
    }
    glm::quat rotation{1.0F, 0.0F, 0.0F, 0.0F};
    if (node.rotation.size() == 4) {
        // glTF stores x, y, z, w; GLM's constructor accepts w, x, y, z.
        rotation = glm::quat{static_cast<float>(node.rotation[3]),
                             static_cast<float>(node.rotation[0]),
                             static_cast<float>(node.rotation[1]),
                             static_cast<float>(node.rotation[2])};
    }

    return glm::translate(glm::mat4{1.0F}, translation) * glm::mat4_cast(rotation) *
           glm::scale(glm::mat4{1.0F}, scale);
}

bool nodeCollisionEnabled(const tinygltf::Node& node) {
    if (node.name.rfind("nocollide_", 0) == 0) {
        return false;
    }
    if (node.extras.IsObject() && node.extras.Has("collision")) {
        const tinygltf::Value& value = node.extras.Get("collision");
        if (value.IsBool()) {
            return value.Get<bool>();
        }
    }
    return true;
}

bool isPlayerSpawn(const tinygltf::Node& node) {
    if (node.name == "player_spawn") {
        return true;
    }
    if (node.extras.IsObject() && node.extras.Has("type")) {
        const tinygltf::Value& value = node.extras.Get("type");
        return value.IsString() && value.Get<std::string>() == "player_spawn";
    }
    return false;
}
} // namespace

bool GltfLoader::loadGlb(const std::filesystem::path& path, Scene& scene) const {
    if (!std::filesystem::exists(path)) {
        std::cerr << "Scene file not found: " << path << '\n';
        return false;
    }
    if (path.extension() != ".glb") {
        std::cerr << "Reflex Engine supports binary glTF .glb scenes only: " << path << '\n';
        return false;
    }

    tinygltf::TinyGLTF loader;
    loader.SetImageLoader(loadImageWithFallback, nullptr);
    tinygltf::Model model;
    std::string warning;
    std::string error;
    const bool loaded = loader.LoadBinaryFromFile(&model, &error, &warning, path.string());
    if (!warning.empty()) {
        std::cerr << "glTF warning: " << warning << '\n';
    }
    if (!loaded) {
        std::cerr << "Failed to load glTF scene " << path << ": " << error << '\n';
        return false;
    }

    Scene newScene;
    newScene.textures.reserve(model.textures.size());
    for (const tinygltf::Texture& sourceTexture : model.textures) {
        Texture texture;
        if (!uploadTexture(model, sourceTexture, texture)) {
            std::cerr << "Failed to upload both a glTF texture and its fallback.\n";
            return false;
        }
        newScene.textures.push_back(std::move(texture));
    }

    newScene.materials.reserve(model.materials.size());
    for (const tinygltf::Material& sourceMaterial : model.materials) {
        Material material;
        const auto& factor = sourceMaterial.pbrMetallicRoughness.baseColorFactor;
        if (factor.size() >= 3) {
            material.baseColorFactor = {static_cast<float>(factor[0]),
                                        static_cast<float>(factor[1]),
                                        static_cast<float>(factor[2])};
        }
        const int textureIndex = sourceMaterial.pbrMetallicRoughness.baseColorTexture.index;
        if (textureIndex >= 0 &&
            static_cast<std::size_t>(textureIndex) < newScene.textures.size()) {
            material.baseColorTexture = textureIndex;
            material.hasBaseColorTexture = true;
        } else if (textureIndex >= 0) {
            std::cerr << "Warning: material references an invalid base-color texture.\n";
        }
        material.doubleSided = sourceMaterial.doubleSided;
        newScene.materials.push_back(material);
    }

    LoadStatistics statistics;
    std::vector<std::vector<std::optional<std::size_t>>> meshPrimitiveMap(model.meshes.size());
    std::vector<std::vector<std::optional<CpuPrimitive>>> cpuPrimitiveMap(model.meshes.size());
    for (std::size_t meshIndex = 0; meshIndex < model.meshes.size(); ++meshIndex) {
        const tinygltf::Mesh& sourceMesh = model.meshes[meshIndex];
        auto& primitiveMap = meshPrimitiveMap[meshIndex];
        primitiveMap.resize(sourceMesh.primitives.size());
        auto& cpuPrimitives = cpuPrimitiveMap[meshIndex];
        cpuPrimitives.resize(sourceMesh.primitives.size());

        for (std::size_t primitiveIndex = 0;
             primitiveIndex < sourceMesh.primitives.size(); ++primitiveIndex) {
            const tinygltf::Primitive& primitive = sourceMesh.primitives[primitiveIndex];
            if (primitive.mode != -1 && primitive.mode != TINYGLTF_MODE_TRIANGLES) {
                std::cerr << "Warning: skipping unsupported glTF primitive mode "
                          << primitive.mode << " (only triangles are supported).\n";
                continue;
            }

            const auto positionAttribute = primitive.attributes.find("POSITION");
            if (positionAttribute == primitive.attributes.end()) {
                std::cerr << "Warning: skipping a primitive without POSITION data.\n";
                continue;
            }

            std::vector<glm::vec3> positions;
            if (!readVec3Attribute(model, positionAttribute->second, "positions", positions) ||
                positions.empty()) {
                std::cerr << "Warning: skipping a primitive with invalid positions.\n";
                continue;
            }

            std::vector<std::uint32_t> indices;
            if (primitive.indices >= 0) {
                if (!readIndices(model, primitive.indices, indices)) {
                    std::cerr << "Warning: skipping a primitive with invalid indices.\n";
                    continue;
                }
            } else {
                indices.resize(positions.size());
                std::iota(indices.begin(), indices.end(), 0U);
            }
            if (indices.empty() || indices.size() % 3 != 0 ||
                std::any_of(indices.begin(), indices.end(),
                            [&positions](const std::uint32_t index) {
                                return index >= positions.size();
                            })) {
                std::cerr << "Warning: skipping a primitive with invalid triangle indices.\n";
                continue;
            }

            std::vector<Vertex> vertices(positions.size());
            for (std::size_t i = 0; i < positions.size(); ++i) {
                vertices[i].position = positions[i];
            }

            bool hasNormals = false;
            const auto normalAttribute = primitive.attributes.find("NORMAL");
            if (normalAttribute != primitive.attributes.end()) {
                std::vector<glm::vec3> normals;
                if (readVec3Attribute(model, normalAttribute->second, "normals", normals) &&
                    normals.size() == vertices.size()) {
                    for (std::size_t i = 0; i < vertices.size(); ++i) {
                        vertices[i].normal = normals[i];
                    }
                    hasNormals = true;
                } else {
                    std::cerr << "Warning: invalid normals; generated normals will be used.\n";
                }
            }
            if (!hasNormals) {
                generateNormals(vertices, indices);
            }

            const auto uvAttribute = primitive.attributes.find("TEXCOORD_0");
            if (uvAttribute != primitive.attributes.end()) {
                std::vector<glm::vec2> texCoords;
                if (readVec2Attribute(model, uvAttribute->second, "TEXCOORD_0", texCoords) &&
                    texCoords.size() == vertices.size()) {
                    for (std::size_t i = 0; i < vertices.size(); ++i) {
                        vertices[i].texCoord = texCoords[i];
                    }
                } else {
                    std::cerr << "Warning: invalid TEXCOORD_0; using (0, 0).\n";
                }
            }

            Mesh mesh;
            if (!mesh.upload(vertices, indices)) {
                return false;
            }
            primitiveMap[primitiveIndex] = newScene.meshes.size();
            cpuPrimitives[primitiveIndex] = CpuPrimitive{positions, indices};
            newScene.meshes.push_back(std::move(mesh));
            ++statistics.primitives;
            statistics.vertices += vertices.size();
            statistics.indices += indices.size();
        }
    }

    if (model.scenes.empty()) {
        std::cerr << "The glTF file contains no scenes.\n";
        return false;
    }
    const int sceneIndex = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (static_cast<std::size_t>(sceneIndex) >= model.scenes.size()) {
        std::cerr << "The glTF default scene index is invalid.\n";
        return false;
    }

    std::vector<Triangle> collisionTriangles;
    collisionTriangles.reserve(statistics.indices / 3);
    std::size_t degenerateTriangles = 0;
    std::vector<bool> recursionStack(model.nodes.size(), false);
    std::function<bool(int, const glm::mat4&)> visitNode;
    visitNode = [&](const int nodeIndex, const glm::mat4& parentTransform) {
        if (nodeIndex < 0 || static_cast<std::size_t>(nodeIndex) >= model.nodes.size()) {
            std::cerr << "A scene references an invalid node.\n";
            return false;
        }
        if (recursionStack[static_cast<std::size_t>(nodeIndex)]) {
            std::cerr << "A cycle was found in the glTF node hierarchy.\n";
            return false;
        }

        recursionStack[static_cast<std::size_t>(nodeIndex)] = true;
        const tinygltf::Node& node = model.nodes[static_cast<std::size_t>(nodeIndex)];
        const glm::mat4 worldTransform = parentTransform * nodeLocalTransform(node);

        if (isPlayerSpawn(node) && !newScene.hasPlayerSpawn) {
            newScene.playerSpawnPosition = glm::vec3{worldTransform[3]};
            glm::vec3 forward = glm::vec3{worldTransform *
                glm::vec4{0.0F, 0.0F, -1.0F, 0.0F}};
            if (glm::length(forward) > 0.000001F) {
                forward = glm::normalize(forward);
                newScene.playerSpawnYawDegrees =
                    glm::degrees(std::atan2(forward.z, forward.x));
            }
            newScene.hasPlayerSpawn = true;
        }

        if (node.mesh >= 0 && static_cast<std::size_t>(node.mesh) < meshPrimitiveMap.size()) {
            const auto& primitiveMap = meshPrimitiveMap[static_cast<std::size_t>(node.mesh)];
            const auto& sourcePrimitives = model.meshes[static_cast<std::size_t>(node.mesh)].primitives;
            for (std::size_t i = 0; i < primitiveMap.size(); ++i) {
                if (primitiveMap[i].has_value()) {
                    newScene.primitives.push_back(ScenePrimitive{
                        *primitiveMap[i], sourcePrimitives[i].material, worldTransform});
                }
                if (nodeCollisionEnabled(node) && cpuPrimitiveMap[static_cast<std::size_t>(node.mesh)][i]) {
                    const CpuPrimitive& cpu =
                        *cpuPrimitiveMap[static_cast<std::size_t>(node.mesh)][i];
                    for (std::size_t index = 0; index + 2 < cpu.indices.size(); index += 3) {
                        Triangle triangle;
                        triangle.a = glm::vec3{worldTransform * glm::vec4{cpu.positions[cpu.indices[index]], 1.0F}};
                        triangle.b = glm::vec3{worldTransform * glm::vec4{cpu.positions[cpu.indices[index + 1]], 1.0F}};
                        triangle.c = glm::vec3{worldTransform * glm::vec4{cpu.positions[cpu.indices[index + 2]], 1.0F}};
                        const glm::vec3 cross = glm::cross(triangle.b - triangle.a,
                                                         triangle.c - triangle.a);
                        const float areaTwice = glm::length(cross);
                        if (areaTwice <= 0.000001F) {
                            ++degenerateTriangles;
                            continue;
                        }
                        triangle.normal = cross / areaTwice;
                        triangle.bounds.expand(triangle.a);
                        triangle.bounds.expand(triangle.b);
                        triangle.bounds.expand(triangle.c);
                        collisionTriangles.push_back(triangle);
                    }
                }
            }
        } else if (node.mesh >= 0) {
            std::cerr << "Warning: a node references an invalid mesh.\n";
        }

        for (const int child : node.children) {
            if (!visitNode(child, worldTransform)) {
                return false;
            }
        }
        recursionStack[static_cast<std::size_t>(nodeIndex)] = false;
        return true;
    };

    for (const int rootNode : model.scenes[static_cast<std::size_t>(sceneIndex)].nodes) {
        if (!visitNode(rootNode, glm::mat4{1.0F})) {
            return false;
        }
    }

    if (degenerateTriangles > 0) {
        std::cerr << "Warning: skipped " << degenerateTriangles
                  << " degenerate collision triangles.\n";
    }
    if (!newScene.collisionWorld.build(std::move(collisionTriangles))) {
        std::cerr << "Failed to build the collision spatial index.\n";
        return false;
    }
    if (newScene.collisionWorld.empty()) {
        std::cerr << "Warning: scene contains no collision triangles; starting in noclip mode.\n";
    }
    if (!newScene.hasPlayerSpawn) {
        std::cerr << "Warning: no player_spawn node; using fallback (0, 1, 5).\n";
    }

    scene = std::move(newScene);
    std::cout << "Loaded scene:       " << path << '\n'
              << "  nodes:            " << model.nodes.size() << '\n'
              << "  meshes:           " << model.meshes.size() << '\n'
              << "  primitives:       " << statistics.primitives << '\n'
              << "  materials:        " << model.materials.size() << '\n'
              << "  textures:         " << model.textures.size() << '\n'
              << "  uploaded vertices: " << statistics.vertices << '\n'
              << "  uploaded indices: " << statistics.indices << '\n';
    std::cout << "  player spawn:     (" << scene.playerSpawnPosition.x << ", "
              << scene.playerSpawnPosition.y << ", " << scene.playerSpawnPosition.z
              << ")\n";
    return true;
}
