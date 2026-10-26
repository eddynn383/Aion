#pragma once

#include "ViewportFrame.h"

#include <QQuickRhiItem>
#include <rhi/qrhi.h>

#include <memory>

namespace forma::render {

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

    enum class Depth { None, Test, TestAndWrite };
    std::unique_ptr<QRhiGraphicsPipeline> makePipeline(const QString& vertexShader,
        const QString& fragmentShader, const QRhiVertexInputLayout& layout, Depth depth, bool blend);

    QRhi* m_rhi = nullptr;
    int m_sampleCount = 0;
    QVector<quint32> m_passFormat;

    std::unique_ptr<QRhiBuffer> m_uniforms;
    std::unique_ptr<QRhiShaderResourceBindings> m_bindings;
    std::unique_ptr<QRhiBuffer> m_quadCorners; // 6 vârfuri pentru linii
    std::unique_ptr<QRhiBuffer> m_pointCorners; // 6 vârfuri pentru puncte
    bool m_cornersUploaded = false;
    std::unique_ptr<QRhiBuffer> m_lineInstances;
    std::unique_ptr<QRhiBuffer> m_pointInstances;
    std::unique_ptr<QRhiBuffer> m_mesh;
    unsigned m_uploadedMeshRevision = ~0u;
    quint32 m_meshVertexCount = 0;

    std::unique_ptr<QRhiGraphicsPipeline> m_backgroundPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_meshPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_gridPipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_linePipeline;
    std::unique_ptr<QRhiGraphicsPipeline> m_pointPipeline;

    ViewportFrame m_frame;
};

} // namespace forma::render
