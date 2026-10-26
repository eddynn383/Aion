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
    glm::dmat4 m_lightViewProj { 1.0 };

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
