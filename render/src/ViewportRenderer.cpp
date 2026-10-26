#include "ViewportRenderer.h"

#include "aion/render/Viewport.h"

#include <QFile>
#include <QMatrix4x4>
#include <rhi/qshader.h>

#include <glm/geometric.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/matrix.hpp>

#include <algorithm>
#include <array>
#include <cstring>

namespace aion::render {

namespace {

// Trebuie să corespundă cu blocul `Frame` din shaders/*.vert|frag (layout std140).
struct FrameUniforms {
    float viewProj[16];
    float invViewProj[16];
    float eye[4];
    float viewport[4];
    float bgTop[4];
    float bgBottom[4];
    float gridMinor[4];
    float gridMajor[4];
    float lightDir[4];
    float faceFront[4];
    float faceBack[4];
    float forward[4];
    float accent[4];
    float params[4];
    float lightViewProj[16];
    float shadow[4];
    float style[4];
    float section[4];
};

constexpr int kShadowMapSize = 2048;

QShader loadShader(const QString& name)
{
    QFile file(QStringLiteral(":/aion/shaders/%1.qsb").arg(name));
    if (!file.open(QIODevice::ReadOnly)) {
        qFatal("Shaderul %s lipsește din resurse", qPrintable(name));
    }
    return QShader::fromSerialized(file.readAll());
}

void copy4(float (&dst)[4], const glm::vec4& v)
{
    std::memcpy(dst, glm::value_ptr(v), sizeof(dst));
}

void copy4(float (&dst)[4], const glm::dvec3& v, double w)
{
    dst[0] = static_cast<float>(v.x);
    dst[1] = static_cast<float>(v.y);
    dst[2] = static_cast<float>(v.z);
    dst[3] = static_cast<float>(w);
}

void copyMatrix(float (&dst)[16], const glm::dmat4& m)
{
    const glm::mat4 f(m);
    std::memcpy(dst, glm::value_ptr(f), sizeof(dst));
}

QRhiVertexInputLayout meshLayout()
{
    QRhiVertexInputLayout layout;
    layout.setBindings({ { sizeof(MeshVertex) } });
    layout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, offsetof(MeshVertex, position) },
        { 0, 1, QRhiVertexInputAttribute::Float3, offsetof(MeshVertex, normal) },
        { 0, 2, QRhiVertexInputAttribute::Float, offsetof(MeshVertex, state) },
        { 0, 3, QRhiVertexInputAttribute::Float4, offsetof(MeshVertex, color) },
        { 0, 4, QRhiVertexInputAttribute::Float2, offsetof(MeshVertex, material) },
    });
    return layout;
}

// Colțurile unui quad din două triunghiuri.
constexpr std::array<float, 12> kLineCorners { 0, -1, 1, -1, 1, 1, 0, -1, 1, 1, 0, 1 };
constexpr std::array<float, 12> kPointCorners { -1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1 };

} // namespace

void ViewportRenderer::initialize(QRhiCommandBuffer* /*cb*/)
{
    if (m_rhi != rhi()) {
        releaseMaskTarget();
        m_maskSampler.reset();
        m_rhi = rhi();
        m_backgroundPipeline.reset();
        m_uniforms.reset();
        m_uploadedMeshRevision = ~0u;
    }
    if (m_sampleCount != renderTarget()->sampleCount()) {
        m_sampleCount = renderTarget()->sampleCount();
        m_backgroundPipeline.reset();
    }
    // Cu MSAA, colorTexture() e null; formatul render pass-ului acoperă ambele cazuri.
    const QVector<quint32> passFormat = renderTarget()->renderPassDescriptor()->serializedFormat();
    if (m_passFormat != passFormat) {
        m_passFormat = passFormat;
        m_backgroundPipeline.reset();
    }

    if (!m_uniforms) {
        createStaticResources();
    }
    if (!m_backgroundPipeline) {
        createPipelines();
        m_outlinePipeline.reset(); // depinde de render pass-ul principal
    }
}

void ViewportRenderer::createStaticResources()
{
    m_uniforms.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(FrameUniforms)));
    m_uniforms->create();

    // Harta de umbre (există mereu, ca legăturile shaderelor să fie aceleași; se desenează doar cu umbrele active).
    // Pe WebGL 2 fără EXT_color_buffer_float nu se poate randa în R32F: umbrele se dezactivează (textura rămâne,
    // mică, doar pentru legăturile shaderelor).
    m_shadowsSupported = m_rhi->isTextureFormatSupported(QRhiTexture::R32F);
    const QSize shadowSize = m_shadowsSupported ? QSize(kShadowMapSize, kShadowMapSize) : QSize(4, 4);
    m_shadowTexture.reset(m_rhi->newTexture(m_shadowsSupported ? QRhiTexture::R32F : QRhiTexture::RGBA8, shadowSize, 1,
        QRhiTexture::RenderTarget));
    m_shadowTexture->create();
    m_shadowDepth.reset(m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, shadowSize, 1));
    m_shadowDepth->create();
    QRhiTextureRenderTargetDescription shadowDesc { QRhiColorAttachment(m_shadowTexture.get()) };
    shadowDesc.setDepthStencilBuffer(m_shadowDepth.get());
    m_shadowTarget.reset(m_rhi->newTextureRenderTarget(shadowDesc));
    m_shadowPass.reset(m_shadowTarget->newCompatibleRenderPassDescriptor());
    m_shadowTarget->setRenderPassDescriptor(m_shadowPass.get());
    m_shadowTarget->create();
    m_shadowSampler.reset(m_rhi->newSampler(QRhiSampler::Nearest, QRhiSampler::Nearest, QRhiSampler::None,
        QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
    m_shadowSampler->create();

    m_bindings.reset(m_rhi->newShaderResourceBindings());
    m_bindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0,
            QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_uniforms.get()),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, m_shadowTexture.get(), m_shadowSampler.get()),
    });
    m_bindings->create();

    m_shadowPipeline.reset(m_rhi->newGraphicsPipeline());
    m_shadowPipeline->setShaderStages({
        { QRhiShaderStage::Vertex, loadShader(QStringLiteral("shadow.vert")) },
        { QRhiShaderStage::Fragment, loadShader(QStringLiteral("shadow.frag")) },
    });
    m_shadowPipeline->setVertexInputLayout(meshLayout());
    m_shadowPipeline->setShaderResourceBindings(m_bindings.get());
    m_shadowPipeline->setRenderPassDescriptor(m_shadowPass.get());
    m_shadowPipeline->setDepthTest(true);
    m_shadowPipeline->setDepthWrite(true);
    m_shadowPipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
    m_shadowPipeline->create();

    m_quadCorners.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kLineCorners)));
    m_quadCorners->create();
    m_pointCorners.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, sizeof(kPointCorners)));
    m_pointCorners->create();
    // Se încarcă odată cu primul cadru, în render().
    m_cornersUploaded = false;
}

std::unique_ptr<QRhiGraphicsPipeline> ViewportRenderer::makePipeline(const QString& vertexShader,
    const QString& fragmentShader, const QRhiVertexInputLayout& layout, Depth depth, bool blend)
{
    std::unique_ptr<QRhiGraphicsPipeline> pipeline(m_rhi->newGraphicsPipeline());
    pipeline->setShaderStages({
        { QRhiShaderStage::Vertex, loadShader(vertexShader) },
        { QRhiShaderStage::Fragment, loadShader(fragmentShader) },
    });
    pipeline->setVertexInputLayout(layout);
    pipeline->setShaderResourceBindings(m_bindings.get());
    pipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
    pipeline->setSampleCount(m_sampleCount);
    pipeline->setDepthTest(depth != Depth::None);
    pipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
    pipeline->setDepthWrite(depth == Depth::TestAndWrite);
    if (blend) {
        QRhiGraphicsPipeline::TargetBlend b;
        b.enable = true;
        b.srcColor = QRhiGraphicsPipeline::SrcAlpha;
        b.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        b.srcAlpha = QRhiGraphicsPipeline::One;
        b.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
        pipeline->setTargetBlends({ b });
    }
    pipeline->create();
    return pipeline;
}

void ViewportRenderer::createPipelines()
{
    m_backgroundPipeline = makePipeline(QStringLiteral("fullscreen.vert"), QStringLiteral("background.frag"),
        QRhiVertexInputLayout {}, Depth::None, false);

    m_meshPipeline = makePipeline(
        QStringLiteral("mesh.vert"), QStringLiteral("mesh.frag"), meshLayout(), Depth::TestAndWrite, false);

    m_gridPipeline = makePipeline(QStringLiteral("fullscreen.vert"), QStringLiteral("grid.frag"),
        QRhiVertexInputLayout {}, Depth::Test, true);
    m_groundPipeline = makePipeline(QStringLiteral("fullscreen.vert"), QStringLiteral("ground.frag"),
        QRhiVertexInputLayout {}, Depth::Test, true);
    m_transparentPipeline
        = makePipeline(QStringLiteral("mesh.vert"), QStringLiteral("mesh.frag"), meshLayout(), Depth::Test, true);

    QRhiVertexInputLayout lineLayout;
    lineLayout.setBindings({
        { 2 * sizeof(float) },
        { sizeof(LineInstance), QRhiVertexInputBinding::PerInstance },
    });
    lineLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 1, 1, QRhiVertexInputAttribute::Float3, offsetof(LineInstance, a) },
        { 1, 2, QRhiVertexInputAttribute::Float3, offsetof(LineInstance, b) },
        { 1, 3, QRhiVertexInputAttribute::Float4, offsetof(LineInstance, color) },
        { 1, 4, QRhiVertexInputAttribute::Float2, offsetof(LineInstance, style) },
    });
    m_linePipeline = makePipeline(QStringLiteral("line.vert"), QStringLiteral("line.frag"), lineLayout, Depth::Test, true);
    m_topLinePipeline
        = makePipeline(QStringLiteral("line.vert"), QStringLiteral("line.frag"), lineLayout, Depth::None, true);

    QRhiVertexInputLayout pointLayout;
    pointLayout.setBindings({
        { 2 * sizeof(float) },
        { sizeof(PointInstance), QRhiVertexInputBinding::PerInstance },
    });
    pointLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float2, 0 },
        { 1, 1, QRhiVertexInputAttribute::Float3, offsetof(PointInstance, position) },
        { 1, 2, QRhiVertexInputAttribute::Float4, offsetof(PointInstance, color) },
        { 1, 3, QRhiVertexInputAttribute::Float, offsetof(PointInstance, sizePx) },
    });
    m_pointPipeline
        = makePipeline(QStringLiteral("point.vert"), QStringLiteral("point.frag"), pointLayout, Depth::Test, true);
    m_topPointPipeline
        = makePipeline(QStringLiteral("point.vert"), QStringLiteral("point.frag"), pointLayout, Depth::None, true);
}

void ViewportRenderer::releaseMaskTarget()
{
    m_outlinePipeline.reset();
    m_outlineBindings.reset();
    m_maskPipeline.reset();
    m_maskTarget.reset();
    m_maskPass.reset();
    m_maskDepth.reset();
    m_maskTexture.reset();
    m_maskSize = {};
}

void ViewportRenderer::ensureMaskTarget(QSize size)
{
    if (m_maskTarget && size == m_maskSize) {
        return;
    }
    releaseMaskTarget();
    m_maskSize = size;

    m_maskTexture.reset(m_rhi->newTexture(QRhiTexture::RGBA8, size, 1, QRhiTexture::RenderTarget));
    m_maskTexture->create();
    m_maskDepth.reset(m_rhi->newRenderBuffer(QRhiRenderBuffer::DepthStencil, size, 1));
    m_maskDepth->create();

    QRhiTextureRenderTargetDescription desc { QRhiColorAttachment(m_maskTexture.get()) };
    desc.setDepthStencilBuffer(m_maskDepth.get());
    m_maskTarget.reset(m_rhi->newTextureRenderTarget(desc));
    m_maskPass.reset(m_maskTarget->newCompatibleRenderPassDescriptor());
    m_maskTarget->setRenderPassDescriptor(m_maskPass.get());
    m_maskTarget->create();

    m_maskPipeline.reset(m_rhi->newGraphicsPipeline());
    m_maskPipeline->setShaderStages({
        { QRhiShaderStage::Vertex, loadShader(QStringLiteral("mesh.vert")) },
        { QRhiShaderStage::Fragment, loadShader(QStringLiteral("mask.frag")) },
    });
    m_maskPipeline->setVertexInputLayout(meshLayout());
    m_maskPipeline->setShaderResourceBindings(m_bindings.get());
    m_maskPipeline->setRenderPassDescriptor(m_maskPass.get());
    m_maskPipeline->setDepthTest(true);
    m_maskPipeline->setDepthWrite(true);
    m_maskPipeline->setDepthOp(QRhiGraphicsPipeline::LessOrEqual);
    m_maskPipeline->create();

    if (!m_maskSampler) {
        m_maskSampler.reset(m_rhi->newSampler(QRhiSampler::Linear, QRhiSampler::Linear, QRhiSampler::None,
            QRhiSampler::ClampToEdge, QRhiSampler::ClampToEdge));
        m_maskSampler->create();
    }
    m_outlineBindings.reset(m_rhi->newShaderResourceBindings());
    m_outlineBindings->setBindings({
        QRhiShaderResourceBinding::uniformBuffer(0,
            QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_uniforms.get()),
        QRhiShaderResourceBinding::sampledTexture(
            1, QRhiShaderResourceBinding::FragmentStage, m_maskTexture.get(), m_maskSampler.get()),
    });
    m_outlineBindings->create();
}

void ViewportRenderer::synchronize(QQuickRhiItem* item)
{
    auto* viewport = static_cast<Viewport*>(item);
    m_frame = viewport->frameForRendering();
}

void ViewportRenderer::ensureInstanceBuffer(std::unique_ptr<QRhiBuffer>& buffer, quint32 bytes)
{
    bytes = std::max<quint32>(bytes, 256);
    if (buffer && buffer->size() >= bytes) {
        return;
    }
    buffer.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::VertexBuffer, bytes * 2));
    buffer->create();
}

void ViewportRenderer::updateUniforms(QRhiResourceUpdateBatch* batch)
{
    const QSize size = renderTarget()->pixelSize();

    QMatrix4x4 corr = m_rhi->clipSpaceCorrMatrix();
    glm::mat4 corrF;
    std::memcpy(glm::value_ptr(corrF), corr.constData(), sizeof(float) * 16);
    const glm::dmat4 viewProj = glm::dmat4(corrF) * m_frame.projection * m_frame.view;

    FrameUniforms u {};
    copyMatrix(u.viewProj, viewProj);
    copyMatrix(u.invViewProj, glm::inverse(viewProj));
    copy4(u.eye, m_frame.eye, m_frame.targetDistance);
    u.viewport[0] = static_cast<float>(size.width());
    u.viewport[1] = static_cast<float>(size.height());
    u.viewport[2] = m_frame.devicePixelRatio;
    u.viewport[3] = m_rhi->isClipDepthZeroToOne() ? 1.0f : 0.0f;
    copy4(u.bgTop, m_frame.colors.backgroundTop);
    copy4(u.bgBottom, m_frame.colors.backgroundBottom);
    copy4(u.gridMinor, m_frame.colors.gridMinor);
    copy4(u.gridMajor, m_frame.colors.gridMajor);
    copy4(u.lightDir, glm::normalize(m_frame.sunDirection), m_frame.sunIntensity);
    copy4(u.faceFront, m_frame.colors.faceFront);
    copy4(u.faceBack, m_frame.colors.faceBack);
    copy4(u.forward, m_frame.forward, m_frame.orthographic ? 1.0 : 0.0);
    copy4(u.accent, m_frame.colors.accent);
    u.params[0] = m_rhi->isYUpInNDC() ? 1.0f : 0.0f;
    u.params[1] = m_rhi->isYUpInFramebuffer() ? 1.0f : 0.0f;
    u.params[2] = static_cast<float>(m_frame.gridStep); // 0 = pasul automat

    // Umbrele: o proiecție ortografică din direcția soarelui care cuprinde toată scena.
    const glm::dvec3 center = (m_frame.sceneMin + m_frame.sceneMax) * 0.5;
    const double radius = std::max(glm::length(m_frame.sceneMax - m_frame.sceneMin) * 0.5, 1000.0);
    const glm::dvec3 dir = glm::normalize(m_frame.sunDirection);
    const glm::dvec3 up = std::abs(dir.z) > 0.99 ? glm::dvec3(0, 1, 0) : glm::dvec3(0, 0, 1);
    const glm::dmat4 lightView = glm::lookAt(center + dir * radius * 2.0, center, up);
    const glm::dmat4 lightProj = glm::ortho(-radius, radius, -radius, radius, radius * 0.5, radius * 3.5);
    m_lightViewProj = glm::dmat4(corrF) * lightProj * lightView;
    copyMatrix(u.lightViewProj, m_lightViewProj);
    u.shadow[0] = shadowsActive() ? 1.0f : 0.0f;
    u.shadow[1] = static_cast<float>(radius * 0.0015);
    u.shadow[2] = 1.0f / static_cast<float>(kShadowMapSize);
    u.shadow[3] = static_cast<float>(std::clamp(m_frame.sunIntensity, 0.0, 1.0)) * 0.85f;
    u.style[0] = static_cast<float>(m_frame.style);
    u.style[1] = m_frame.fog ? 1.0f : 0.0f;
    u.style[2] = static_cast<float>(m_frame.targetDistance * 1.2);
    u.style[3] = static_cast<float>(m_frame.targetDistance * 6.0);
    if (m_frame.section) {
        copy4(u.section, glm::normalize(glm::dvec3(*m_frame.section)), (*m_frame.section).w);
    }

    batch->updateDynamicBuffer(m_uniforms.get(), 0, sizeof(u), &u);
}

void ViewportRenderer::render(QRhiCommandBuffer* cb)
{
    QRhiResourceUpdateBatch* batch = m_rhi->nextResourceUpdateBatch();

    if (!m_cornersUploaded) {
        batch->uploadStaticBuffer(m_quadCorners.get(), kLineCorners.data());
        batch->uploadStaticBuffer(m_pointCorners.get(), kPointCorners.data());
        m_cornersUploaded = true;
    }

    updateUniforms(batch);

    // Instanțele „de deasupra” stau în același buffer, după cele obișnuite.
    const auto lineCount = static_cast<quint32>(m_frame.lines.size());
    const auto topLineCount = static_cast<quint32>(m_frame.topLines.size());
    const auto pointCount = static_cast<quint32>(m_frame.points.size());
    const auto topPointCount = static_cast<quint32>(m_frame.topPoints.size());
    if (lineCount + topLineCount > 0) {
        const auto bytes = static_cast<quint32>(lineCount * sizeof(LineInstance));
        const auto topBytes = static_cast<quint32>(topLineCount * sizeof(LineInstance));
        ensureInstanceBuffer(m_lineInstances, bytes + topBytes);
        if (bytes > 0) {
            batch->updateDynamicBuffer(m_lineInstances.get(), 0, bytes, m_frame.lines.data());
        }
        if (topBytes > 0) {
            batch->updateDynamicBuffer(m_lineInstances.get(), bytes, topBytes, m_frame.topLines.data());
        }
    }
    if (pointCount + topPointCount > 0) {
        const auto bytes = static_cast<quint32>(pointCount * sizeof(PointInstance));
        const auto topBytes = static_cast<quint32>(topPointCount * sizeof(PointInstance));
        ensureInstanceBuffer(m_pointInstances, bytes + topBytes);
        if (bytes > 0) {
            batch->updateDynamicBuffer(m_pointInstances.get(), 0, bytes, m_frame.points.data());
        }
        if (topBytes > 0) {
            batch->updateDynamicBuffer(m_pointInstances.get(), bytes, topBytes, m_frame.topPoints.data());
        }
    }
    if (m_uploadedMeshRevision != m_frame.meshRevision) {
        m_uploadedMeshRevision = m_frame.meshRevision;
        const auto upload = [&](std::unique_ptr<QRhiBuffer>& buffer, quint32& count, const std::vector<MeshVertex>& data) {
            count = static_cast<quint32>(data.size());
            if (count > 0) {
                const auto bytes = static_cast<quint32>(count * sizeof(MeshVertex));
                buffer.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, bytes));
                buffer->create();
                batch->uploadStaticBuffer(buffer.get(), data.data());
            } else {
                buffer.reset();
            }
        };
        upload(m_mesh, m_meshVertexCount, m_frame.triangles);
        upload(m_transparentMesh, m_transparentVertexCount, m_frame.transparent);
    }

    // Harta de umbre, înaintea celorlalte treceri.
    if (shadowsActive()) {
        cb->beginPass(m_shadowTarget.get(), QColor::fromRgbF(1, 1, 1, 1), { 1.0f, 0 }, batch);
        batch = nullptr;
        if (m_mesh && m_meshVertexCount > 0) {
            cb->setViewport({ 0, 0, float(kShadowMapSize), float(kShadowMapSize) });
            cb->setGraphicsPipeline(m_shadowPipeline.get());
            cb->setShaderResources();
            const QRhiCommandBuffer::VertexInput input(m_mesh.get(), 0);
            cb->setVertexInput(0, 1, &input);
            cb->draw(m_meshVertexCount);
        }
        cb->endPass();
    }

    const QSize size = renderTarget()->pixelSize();
    const bool outline = m_frame.hasSelectedFaces && m_mesh && m_meshVertexCount > 0;
    if (outline) {
        ensureMaskTarget(size);
        cb->beginPass(m_maskTarget.get(), Qt::black, { 1.0f, 0 }, batch); // batch e null dacă l-a trimis harta de umbre
        batch = nullptr; // trimis cu prima trecere
        cb->setViewport({ 0, 0, float(size.width()), float(size.height()) });
        cb->setGraphicsPipeline(m_maskPipeline.get());
        cb->setShaderResources();
        const QRhiCommandBuffer::VertexInput input(m_mesh.get(), 0);
        cb->setVertexInput(0, 1, &input);
        cb->draw(m_meshVertexCount);
        cb->endPass();

        if (!m_outlinePipeline) {
            m_outlinePipeline.reset(m_rhi->newGraphicsPipeline());
            m_outlinePipeline->setShaderStages({
                { QRhiShaderStage::Vertex, loadShader(QStringLiteral("fullscreen.vert")) },
                { QRhiShaderStage::Fragment, loadShader(QStringLiteral("outline.frag")) },
            });
            m_outlinePipeline->setVertexInputLayout({});
            m_outlinePipeline->setShaderResourceBindings(m_outlineBindings.get());
            m_outlinePipeline->setRenderPassDescriptor(renderTarget()->renderPassDescriptor());
            m_outlinePipeline->setSampleCount(m_sampleCount);
            QRhiGraphicsPipeline::TargetBlend b;
            b.enable = true;
            b.srcColor = QRhiGraphicsPipeline::SrcAlpha;
            b.dstColor = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            b.srcAlpha = QRhiGraphicsPipeline::One;
            b.dstAlpha = QRhiGraphicsPipeline::OneMinusSrcAlpha;
            m_outlinePipeline->setTargetBlends({ b });
            m_outlinePipeline->create();
        }
    }

    cb->beginPass(renderTarget(), Qt::black, { 1.0f, 0 }, batch);
    cb->setViewport({ 0, 0, float(size.width()), float(size.height()) });

    cb->setGraphicsPipeline(m_backgroundPipeline.get());
    cb->setShaderResources();
    cb->draw(3);

    if (m_mesh && m_meshVertexCount > 0) {
        cb->setGraphicsPipeline(m_meshPipeline.get());
        cb->setShaderResources();
        const QRhiCommandBuffer::VertexInput input(m_mesh.get(), 0);
        cb->setVertexInput(0, 1, &input);
        cb->draw(m_meshVertexCount);
    }

    if (shadowsActive()) {
        cb->setGraphicsPipeline(m_groundPipeline.get());
        cb->setShaderResources();
        cb->draw(3);
    }

    if (m_frame.gridVisible) {
        cb->setGraphicsPipeline(m_gridPipeline.get());
        cb->setShaderResources();
        cb->draw(3);
    }

    if (outline) {
        cb->setGraphicsPipeline(m_outlinePipeline.get());
        cb->setShaderResources(m_outlineBindings.get());
        cb->draw(3);
    }

    if (m_transparentMesh && m_transparentVertexCount > 0) {
        cb->setGraphicsPipeline(m_transparentPipeline.get());
        cb->setShaderResources();
        const QRhiCommandBuffer::VertexInput input(m_transparentMesh.get(), 0);
        cb->setVertexInput(0, 1, &input);
        cb->draw(m_transparentVertexCount);
    }

    if (lineCount > 0) {
        cb->setGraphicsPipeline(m_linePipeline.get());
        cb->setShaderResources();
        const QRhiCommandBuffer::VertexInput inputs[] = {
            { m_quadCorners.get(), 0 },
            { m_lineInstances.get(), 0 },
        };
        cb->setVertexInput(0, 2, inputs);
        cb->draw(6, lineCount);
    }

    if (pointCount > 0) {
        cb->setGraphicsPipeline(m_pointPipeline.get());
        cb->setShaderResources();
        const QRhiCommandBuffer::VertexInput inputs[] = {
            { m_pointCorners.get(), 0 },
            { m_pointInstances.get(), 0 },
        };
        cb->setVertexInput(0, 2, inputs);
        cb->draw(6, pointCount);
    }

    if (topLineCount > 0) {
        cb->setGraphicsPipeline(m_topLinePipeline.get());
        cb->setShaderResources();
        const QRhiCommandBuffer::VertexInput inputs[] = {
            { m_quadCorners.get(), 0 },
            { m_lineInstances.get(), lineCount * quint32(sizeof(LineInstance)) },
        };
        cb->setVertexInput(0, 2, inputs);
        cb->draw(6, topLineCount);
    }

    if (topPointCount > 0) {
        cb->setGraphicsPipeline(m_topPointPipeline.get());
        cb->setShaderResources();
        const QRhiCommandBuffer::VertexInput inputs[] = {
            { m_pointCorners.get(), 0 },
            { m_pointInstances.get(), pointCount * quint32(sizeof(PointInstance)) },
        };
        cb->setVertexInput(0, 2, inputs);
        cb->draw(6, topPointCount);
    }

    cb->endPass();
}

} // namespace aion::render
