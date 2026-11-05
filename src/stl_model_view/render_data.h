#pragma once

#include <vector>
#include <cstdint>
#include <Eigen/Dense>
#include <QSharedPointer>

// Формат вершины для VBO: позиция + цвет (interleaved)
struct RenderVertex {
    float x, y, z;
    float r, g, b;
};

// Результат работы фонового потока
struct RenderData {
    std::vector<RenderVertex>   vertices;  // N вершин
    std::vector<uint32_t> indices;   // 3*M индексов (треугольники)

    Eigen::Vector3d center = Eigen::Vector3d::Zero();
    double          radius = 1.0;
    bool            ready  = false;
};

using RenderDataPtr = QSharedPointer<RenderData>;
Q_DECLARE_METATYPE(RenderDataPtr)   // требуется для передачи через сигнал/слот