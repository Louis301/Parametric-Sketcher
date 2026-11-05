#include "mesh_worker.h"

#include <algorithm>
#include <cmath>

void MeshWorker::process(const QString& stlPath) {
    // Переиспользование меша при повторном вызове с тем же файлом
    if (m_cachedPath != stlPath || !m_cachedMesh) {
        m_cachedMesh = std::make_unique<Mesh>();

        emit progress(5, "Чтение STL");
        if (m_cancelled.load()) { emit failed("Отменено"); return; }
        if (!m_cachedMesh->loadFromSTL(stlPath.toStdString())) {
            emit failed("Ошибка чтения STL"); return;
        }

        emit progress(25, "Топология");
        if (m_cancelled.load()) { emit failed("Отменено"); return; }
        m_cachedMesh->buildTopology();

        emit progress(55, "Кривизны");
        if (m_cancelled.load()) { emit failed("Отменено"); return; }
        m_cachedMesh->computeCurvature();
        m_cachedMesh->computeBoundingBox();

        m_cachedPath = stlPath;
    }

    emit progress(80, "Буферы");
    if (m_cancelled.load()) { emit failed("Отменено"); return; }

    auto data = QSharedPointer<RenderData>::create();
    buildRenderData(*m_cachedMesh, *data);
    data->ready = true;

    emit progress(100, "Готово");
    emit finished(data);
}

void MeshWorker::buildRenderData(const Mesh& mesh, RenderData& out) {
    const Eigen::Index nV = mesh.V.rows();
    const Eigen::Index nF = mesh.F.rows();

    out.vertices.resize(nV);
    out.indices.resize(nF * 3);

    // --- Позиции ---
    for (Eigen::Index i = 0; i < nV; ++i) {
        out.vertices[i].x = static_cast<float>(mesh.V(i, 0));
        out.vertices[i].y = static_cast<float>(mesh.V(i, 1));
        out.vertices[i].z = static_cast<float>(mesh.V(i, 2));
    }

    // --- Цвета (тепловая карта по mean curvature) ---
    // Синий (вогнутость) → красный (выпуклость), зелёный = 0
    if (!mesh.meanCurvature.empty() &&
        mesh.meanCurvature.size() == static_cast<size_t>(nV)) {
        double mn = *std::min_element(mesh.meanCurvature.begin(),
                                      mesh.meanCurvature.end());
        double mx = *std::max_element(mesh.meanCurvature.begin(),
                                      mesh.meanCurvature.end());
        double range = mx - mn;
        if (range < 1e-12) range = 1.0;

        for (Eigen::Index i = 0; i < nV; ++i) {
            double k = (mesh.meanCurvature[i] - mn) / range;  // [0, 1]
            out.vertices[i].r = static_cast<float>(k);
            out.vertices[i].g = 0.0f;
            out.vertices[i].b = static_cast<float>(1.0 - k);
        }
    } else {
        // Fallback — серый, если кривизны не рассчитаны
        for (auto& v : out.vertices) {
            v.r = v.g = v.b = 0.6f;
        }
    }

    // --- Индексы ---
    for (Eigen::Index f = 0; f < nF; ++f) {
        out.indices[f * 3 + 0] = static_cast<uint32_t>(mesh.F(f, 0));
        out.indices[f * 3 + 1] = static_cast<uint32_t>(mesh.F(f, 1));
        out.indices[f * 3 + 2] = static_cast<uint32_t>(mesh.F(f, 2));
    }

    out.center = mesh.center;
    out.radius = mesh.radius;
}