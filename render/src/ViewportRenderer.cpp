#include "ViewportRenderer.h"

#include "aion/render/Viewport.h"

#include <QFile>
#include <QMatrix4x4>
#include <rhi/qshader.h>

#include <glm/geometric.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/matrix.hpp>

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
    float params[4];
};

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

// Colțurile unui quad din două triunghiuri.
constexpr std::array<float, 12> kLineCorners { 0, -1, 1, -1, 1, 1, 0, -1, 1, 1, 0, 1 };
constexpr std::array<float, 12> kPointCorners { -1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1 };

} // namespace

void ViewportRenderer::initialize(QRhiCommandBuffer* /*cb*/)
{
    if (m_rhi != rhi()) {
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
    }
}

void ViewportRenderer::createStaticResources()
{
    m_uniforms.reset(m_rhi->newBuffer(QRhiBuffer::Dynamic, QRhiBuffer::UniformBuffer, sizeof(FrameUniforms)));
    m_uniforms->create();

    m_bindings.reset(m_rhi->newShaderResourceBindings());
    m_bindings->setBindings({ QRhiShaderResourceBinding::uniformBuffer(0,
        QRhiShaderResourceBinding::VertexStage | QRhiShaderResourceBinding::FragmentStage, m_uniforms.get()) });
    m_bindings->create();

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

    QRhiVertexInputLayout meshLayout;
    meshLayout.setBindings({ { sizeof(MeshVertex) } });
    meshLayout.setAttributes({
        { 0, 0, QRhiVertexInputAttribute::Float3, offsetof(MeshVertex, position) },
        { 0, 1, QRhiVertexInputAttribute::Float3, offsetof(MeshVertex, normal) },
    });
    m_meshPipeline = makePipeline(QStringLiteral("mesh.vert"), QStringLiteral("mesh.frag"), meshLayout, Depth::TestAndWrite, false);

    m_gridPipeline = makePipeline(QStringLiteral("fullscreen.vert"), QStringLiteral("grid.frag"),
        QRhiVertexInputLayout {}, Depth::Test, true);

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
    copy4(u.lightDir, glm::normalize(glm::dvec3(-0.4, -0.6, 0.7)), 0.0);
    copy4(u.faceFront, m_frame.colors.faceFront);
    copy4(u.faceBack, m_frame.colors.faceBack);
    copy4(u.forward, m_frame.forward, m_frame.orthographic ? 1.0 : 0.0);
    u.params[0] = m_rhi->isYUpInNDC() ? 1.0f : 0.0f;

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

    const auto lineCount = static_cast<quint32>(m_frame.lines.size());
    const auto pointCount = static_cast<quint32>(m_frame.points.size());
    if (lineCount > 0) {
        const auto bytes = static_cast<quint32>(lineCount * sizeof(LineInstance));
        ensureInstanceBuffer(m_lineInstances, bytes);
        batch->updateDynamicBuffer(m_lineInstances.get(), 0, bytes, m_frame.lines.data());
    }
    if (pointCount > 0) {
        const auto bytes = static_cast<quint32>(pointCount * sizeof(PointInstance));
        ensureInstanceBuffer(m_pointInstances, bytes);
        batch->updateDynamicBuffer(m_pointInstances.get(), 0, bytes, m_frame.points.data());
    }
    if (m_uploadedMeshRevision != m_frame.meshRevision) {
        m_uploadedMeshRevision = m_frame.meshRevision;
        m_meshVertexCount = static_cast<quint32>(m_frame.triangles.size());
        if (m_meshVertexCount > 0) {
            const auto bytes = static_cast<quint32>(m_meshVertexCount * sizeof(MeshVertex));
            m_mesh.reset(m_rhi->newBuffer(QRhiBuffer::Immutable, QRhiBuffer::VertexBuffer, bytes));
            m_mesh->create();
            batch->uploadStaticBuffer(m_mesh.get(), m_frame.triangles.data());
        } else {
            m_mesh.reset();
        }
    }

    const QSize size = renderTarget()->pixelSize();
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

    if (m_frame.gridVisible) {
        cb->setGraphicsPipeline(m_gridPipeline.get());
        cb->setShaderResources();
        cb->draw(3);
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

    cb->endPass();
}

} // namespace aion::render
