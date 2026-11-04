#pragma once

#include <Eigen/Dense>
#include <Eigen/Sparse>
#include <igl/cotmatrix.h>
#include <igl/massmatrix.h>
#include <igl/principal_curvature.h>
#include <vector>
#include <unordered_map>
#include <cmath>

// Хеш-функция для Eigen::Vector3d (для сварки вершин)
struct VectorHash {
    size_t operator()(const Eigen::Vector3d& v) const {
        // Квантование для устойчивости к плавающей точке
        const double eps = 1e-7;
        long long x = static_cast<long long>(std::round(v[0] / eps));
        long long y = static_cast<long long>(std::round(v[1] / eps));
        long long z = static_cast<long long>(std::round(v[2] / eps));
        
        size_t seed = 0;
        seed ^= std::hash<long long>{}(x) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<long long>{}(y) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        seed ^= std::hash<long long>{}(z) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        return seed;
    }
};

// Предикат сравнения для векторов (обязателен для unordered_map)
struct VectorEqual {
    bool operator()(const Eigen::Vector3d& a, const Eigen::Vector3d& b) const {
        const double eps = 1e-7;
        return (a - b).norm() < eps;
    }
};

// Хеш-функция для пары индексов (для Half-Edge)
struct PairHash {
    size_t operator()(const std::pair<int, int>& p) const {
        size_t h1 = std::hash<int>{}(p.first);
        size_t h2 = std::hash<int>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

struct HalfEdge {
    int vertex;      // Конечная вершина
    int face;        // Принадлежащая грань
    int next;        // Следующий half-edge в грани
    int prev;        // Предыдущий half-edge в грани
    int twin;        // Парный half-edge (-1 если граница)
};

struct Face {
    int he;          // Один из half-edges этой грани
};

struct Vertex {
    Eigen::Vector3d position;
    int he;          // Один из исходящих half-edges
    double curvature = 0.0;  // Для тепловой карты
    
    Vertex() : position(Eigen::Vector3d::Zero()), he(-1), curvature(0.0) {}
};

// ============================================================
class Mesh {
public:
    // Основные данные
    Eigen::MatrixXd V;           // Вершины (N x 3)
    Eigen::MatrixXi F;           // Грани (M x 3) - индексы вершин
    
    // Half-Edge структура
    std::vector<HalfEdge> halfEdges;
    std::vector<Face> faces;
    std::vector<Vertex> vertices;
    
    // Кривизны
    std::vector<double> meanCurvature;
    std::vector<double> gaussianCurvature;
    
    // Методы
    bool loadFromSTL(const std::string& filename);
    void buildTopology();
    void computeCurvature();

    // Новые поля для камеры    // interaction
    Eigen::Vector3d center;
    double radius;
    // новый метод
    void computeBoundingBox();
    
private:
    // Вспомогательные методы
    void weldVertices(const std::vector<Eigen::Vector3d>& rawVertices,
                      const std::vector<Eigen::Vector3i>& rawFaces);
    void buildHalfEdgeStructure();
};