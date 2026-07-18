#include "rendering/Renderer.hpp"

#include "rendering/Material.hpp"
#include "scene/Camera.hpp"
#include "scene/Scene.hpp"

#include <glad/gl.h>
#include <glm/geometric.hpp>

bool Renderer::initialize(const std::filesystem::path& shaderDirectory) {
    if (!staticMeshShader_.load(shaderDirectory / "static_mesh.vert",
                                shaderDirectory / "static_mesh.frag")) {
        return false;
    }
    if (!skinnedMeshShader_.load(shaderDirectory / "skinned_mesh.vert",
                                 shaderDirectory / "static_mesh.frag")) return false;

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
    return true;
}

void Renderer::render(const Scene& scene, const Camera& camera) {
    staticMeshShader_.use();
    staticMeshShader_.setMat4("uView", camera.viewMatrix());
    staticMeshShader_.setMat4("uProjection", camera.projectionMatrix());
    staticMeshShader_.setInt("uBaseColorTexture", 0);
    staticMeshShader_.setVec3("uLightDirection", glm::normalize(glm::vec3{-0.4F, -1.0F, -0.25F}));
    staticMeshShader_.setFloat("uAmbientStrength", 0.28F);

    const Material fallbackMaterial{};
    for (const ScenePrimitive& primitive : scene.primitives) {
        if (!primitive.visible || primitive.mesh >= scene.meshes.size()) {
            continue;
        }

        const Material* material = &fallbackMaterial;
        if (primitive.material >= 0 &&
            static_cast<std::size_t>(primitive.material) < scene.materials.size()) {
            material = &scene.materials[static_cast<std::size_t>(primitive.material)];
        }

        if (material->doubleSided) {
            glDisable(GL_CULL_FACE);
        } else {
            glEnable(GL_CULL_FACE);
        }

        const bool hasTexture = material->hasBaseColorTexture &&
            material->baseColorTexture >= 0 &&
            static_cast<std::size_t>(material->baseColorTexture) < scene.textures.size();
        if (hasTexture) {
            scene.textures[static_cast<std::size_t>(material->baseColorTexture)].bind();
        }

        staticMeshShader_.setMat4("uModel", primitive.worldTransform);
        staticMeshShader_.setVec3("uBaseColorFactor", material->baseColorFactor);
        staticMeshShader_.setInt("uHasBaseColorTexture", hasTexture ? 1 : 0);
        scene.meshes[primitive.mesh].draw();
    }

    glEnable(GL_CULL_FACE);
    glBindVertexArray(0);

    skinnedMeshShader_.use();
    skinnedMeshShader_.setMat4("uView", camera.viewMatrix());
    skinnedMeshShader_.setMat4("uProjection", camera.projectionMatrix());
    skinnedMeshShader_.setInt("uBaseColorTexture", 0);
    skinnedMeshShader_.setVec3("uLightDirection", glm::normalize(glm::vec3{-0.4F, -1.0F, -0.25F}));
    skinnedMeshShader_.setFloat("uAmbientStrength", 0.28F);
    for (const SkinnedScenePrimitive& primitive : scene.skinnedPrimitives) {
        if (!primitive.visible || primitive.mesh >= scene.skinnedMeshes.size() ||
            primitive.skinMatrices.empty() || primitive.skinMatrices.size() > 128) continue;
        const Material* material = &fallbackMaterial;
        if (primitive.material >= 0 && static_cast<std::size_t>(primitive.material) < scene.materials.size())
            material = &scene.materials[static_cast<std::size_t>(primitive.material)];
        const bool hasTexture = material->hasBaseColorTexture && material->baseColorTexture >= 0 &&
            static_cast<std::size_t>(material->baseColorTexture) < scene.textures.size();
        if (hasTexture) scene.textures[static_cast<std::size_t>(material->baseColorTexture)].bind();
        if (material->doubleSided) glDisable(GL_CULL_FACE); else glEnable(GL_CULL_FACE);
        skinnedMeshShader_.setMat4("uModel", primitive.worldTransform);
        skinnedMeshShader_.setMat4Array("uJoints[0]", primitive.skinMatrices);
        skinnedMeshShader_.setVec3("uBaseColorFactor", material->baseColorFactor);
        skinnedMeshShader_.setInt("uHasBaseColorTexture", hasTexture ? 1 : 0);
        scene.skinnedMeshes[primitive.mesh].draw();
    }
    glEnable(GL_CULL_FACE); glBindVertexArray(0);
}
