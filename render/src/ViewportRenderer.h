#pragma once

#include "ViewportFrame.h"

#include <QQuickRhiItem>
#include <rhi/qrhi.h>

#include <glm/mat4x4.hpp>

#include <memory>

namespace aion::render {

// Randează un cadru al viewport-ului pe firul de randare: fundal, fețe, grid, linii, puncte.
class ViewportRenderer final : public QQuickRhiItemRenderer {
public:
    ViewportRenderer() = default;

protected:
    void initialize(QRhiCommandBuffer* cb) override;
    void synchronize(QQuickRhiItem* item) override;
    void render(QRhiCommandBuffer* cb) override;

private:
    void createStaticResources();
    void createPipelines();
    void ensureInstanceBuffer(std::unique_ptr<QRhiBuffer>& buffer, quint32 bytes);
    void updateUniforms(QRhiResourceUpdateBatch* batch);
    // Ținta în care se desenează masca fețelor selectate (refăcută când se schimbă mărimea).
    void ensureMaskTarget(QSize size);
    void releaseMaskTarget();

    enum class Depth { None, Test, TestAndWrite };
    std::unique_ptr<QRhiGraphicsPipeline> makePipeline(const QString& vertexShader,
        const QString& fragmentShader, const QRhiVertexInputLayout& layout, Depth depth, bool blend);

    QRhi* m_rhi = nullptr;
    int m_sampleCount = 0;
    int m_shadowMapSize = 2048;
    QVector<quint32> m_passFormat;

    std::unique_ptr<QRhiBuffer> m_uniforms;
    std::unique_ptr<QRhiShaderResourceBindings> m_bindings;
    std::unique_ptr<QRhiBuffer> m_quadCorners; // 6 vârfuri pentru linii
    std::unique_ptr<QRhiBuffer> m_pointCorners; // 6 vârfuri pentru puncte
    bool m_cornersUploaded = false;
    bool m_shadowsSupported = true;
    bool shadowsActive() const { return m_frame.shadows && m_shadowsSupported; }
    std::unique_ptr<QRhiBuffer> m_lineInstances;
    std::unique_ptr<QRhiBuffer> m_pointInstances;
    std::unique_ptr<QRhiBuffer> m_mesh;
    std::unique_ptr<QRhiBuffer> m_transparentMesh;
    quint32 m_transparentVertexCount = 0;
    unsigned m_uploadedMeshRevision = ~0u;
    quint32 m_meshVertexCount = 0;

    std::unique_ptr<QRhiGraphicsPipeline> m_backgroundPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_meshPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_gridPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_linePipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_pointPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_topLinePipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_topPointPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_transparentPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_groundPipeline;

    // Umbrele: harta de adâncime văzută din direcția soarelui (R32F, cu adâncime separată).
    std::unique_ptr<QRhiTexture> m_shadowTexture;
    std::unique_ptr<QRhiRenderBuffer> m_shadowDepth;
    std::unique_ptr<QRhiTextureRenderTarget> m_shadowTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_shadowPass;
    std::unique_ptr<QRhiGraphicsPipeline> m_shadowPipeline;
    std::unique_ptr<QRhiSampler> m_shadowSampler;

    // Texturile materialelor: array RGBA8 cu mipmap-uri, câte un strat pe textură (redimensionate la kTextureSize).
    static constexpr int kTextureSize = 1024;
    std::unique_ptr<QRhiTexture> m_textureArray;
    std::unique_ptr<QRhiSampler> m_textureSampler;
    int m_textureLayers = 0;
    std::size_t m_uploadedTextureSignature = ~std::size_t { 0 };
    bool m_texturesInitialized = false;
    void createTextureArray(int layers);

    // SSAO: normala + adâncimea (RGBA16F), apoi umbrirea (RGBA8), la mărimea viewport-ului.
    std::unique_ptr<QRhiTexture> m_gbufferTexture;
    std::unique_ptr<QRhiRenderBuffer> m_gbufferDepth;
    std::unique_ptr<QRhiTextureRenderTarget> m_gbufferTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_gbufferPass;
    std::unique_ptr<QRhiGraphicsPipeline> m_gbufferPipeline;
    std::unique_ptr<QRhiTexture> m_ssaoTexture;
    std::unique_ptr<QRhiTextureRenderTarget> m_ssaoTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_ssaoPass;
    std::unique_ptr<QRhiGraphicsPipeline> m_ssaoPipeline;
    std::unique_ptr<QRhiShaderResourceBindings> m_ssaoBindings;
    std::unique_ptr<QRhiSampler> m_linearClampSampler;
    QSize m_ssaoSize;
    bool m_ssaoSupported = false;
    [[nodiscard]] bool ssaoActive() const { return m_frame.ambientOcclusion && m_ssaoSupported && m_ssaoSize.isValid(); }
    void ensureSsaoTargets(QSize size);
    void updateBindings();
    void uploadTextures(QRhiResourceUpdateBatch* batch);
    glm::dmat4 m_lightViewProj { 1.0 };

    // Umbrele unui spot: a doua hartă (perspectivă din spot), desenată cu o copie a uniformelor (matricea spotului).
    std::unique_ptr<QRhiTexture> m_spotShadowTexture;
    std::unique_ptr<QRhiRenderBuffer> m_spotShadowDepth;
    std::unique_ptr<QRhiTextureRenderTarget> m_spotShadowTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_spotShadowPass;
    std::unique_ptr<QRhiGraphicsPipeline> m_spotShadowPipeline;
    std::unique_ptr<QRhiBuffer> m_spotUniforms;
    std::unique_ptr<QRhiShaderResourceBindings> m_spotBindings;
    glm::dmat4 m_spotViewProj { 1.0 };
    bool m_spotShadowActive = false;

    // Conturul selecției: masca fețelor selectate vizibile, apoi un contur în screen-space.
    QSize m_maskSize;
    std::unique_ptr<QRhiTexture> m_maskTexture;
    std::unique_ptr<QRhiRenderBuffer> m_maskDepth;
    std::unique_ptr<QRhiTextureRenderTarget> m_maskTarget;
    std::unique_ptr<QRhiRenderPassDescriptor> m_maskPass;
    std::unique_ptr<QRhiGraphicsPipeline> m_maskPipeline;
    std::unique_ptr<QRhiSampler> m_maskSampler;
    std::unique_ptr<QRhiShaderResourceBindings> m_outlineBindings;
    std::unique_ptr<QRhiGraphicsPipeline> m_outlinePipeline;

    ViewportFrame m_frame;
};

} // namespace aion::render
