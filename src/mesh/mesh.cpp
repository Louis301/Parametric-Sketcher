#include "mesh.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <sstream>
#include <cstring>


static constexpr uint32_t MAX_TRIANGLES = 50'000'000;

enum class STLFormat { Binary, ASCII, Unknown };

static STLFormat detectSTLFormat(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) return STLFormat::Unknown;
    
    // Читаем первые 80 байт как заголовок
    char header[80] = {0};
    file.read(header, 80);
    
    // Читаем поле triangleCount (4 байта)
    uint32_t triangleCount = 0;
    file.read(reinterpret_cast<char*>(&triangleCount), 4);
    
    // Получаем размер файла
    file.seekg(0, std::ios::end);
    std::streamsize fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // Проверка 1: ASCII начинается с "solid"
    std::string headerStr(header, 80);
    bool startsWithSolid = (headerStr.find("solid") != std::string::npos);
    
    // Проверка 2: размер файла соответствует бинарному формату
    // Бинарный STL: 84 байта заголовка + N * 50 байт на треугольник
    bool sizeMatchesBinary = (fileSize == 84 + static_cast<std::streamsize>(triangleCount) * 50);
    
    // Проверка 3: triangleCount должен быть разумным
    bool countIsReasonable = (triangleCount > 0 && triangleCount <= MAX_TRIANGLES);
    
    // Логика определения:
    // - Если размер точно совпадает с бинарным И count разумный → бинарный
    // - Если начинается с "solid" И размер НЕ совпадает с бинарным → ASCII
    // - Иначе → бинарный (по умолчанию)
    
    if (sizeMatchesBinary && countIsReasonable) {
        return STLFormat::Binary;
    }
    
    if (startsWithSolid && !sizeMatchesBinary) {
        return STLFormat::ASCII;
    }
    
    // Файлы из некоторых CAD-систем начинаются с "solid", но являются бинарными
    // Дополнительная проверка: ищем ключевые слова ASCII STL
    if (startsWithSolid) {
        // Читаем первые 500 байт и ищем "facet normal" или "vertex"
        file.seekg(0);
        char buffer[500] = {0};
        file.read(buffer, std::min(std::streamsize(500), fileSize));
        std::string sample(buffer);
        
        if (sample.find("facet") != std::string::npos || 
            sample.find("vertex") != std::string::npos) {
            return STLFormat::ASCII;
        }
    }
    
    return STLFormat::Binary;
}

// ============================================================
// Загрузка бинарного STL
// ============================================================
static bool loadBinarySTL(std::ifstream& file,
                          std::vector<Eigen::Vector3d>& rawVertices,
                          std::vector<Eigen::Vector3i>& rawFaces,
                          uint32_t triangleCount) {
    rawVertices.reserve(static_cast<size_t>(triangleCount) * 3);
    rawFaces.reserve(triangleCount);
    
    for (uint32_t i = 0; i < triangleCount; ++i) {
        // Нормаль (12 байт) — пропускаем
        file.seekg(12, std::ios::cur);
        
        Eigen::Vector3i faceIndices;
        for (int v = 0; v < 3; ++v) {
            float xyz[3];
            file.read(reinterpret_cast<char*>(xyz), 12);
            if (!file) {
                std::cerr << "[Error] Unexpected EOF at triangle " << i << std::endl;
                return false;
            }
            rawVertices.emplace_back(xyz[0], xyz[1], xyz[2]);
            faceIndices[v] = static_cast<int>(rawVertices.size() - 1);
        }
        rawFaces.push_back(faceIndices);
        
        // Attribute byte count (2 байта) — пропускаем
        file.seekg(2, std::ios::cur);
    }
    
    return true;
}

// ============================================================
// Загрузка ASCII STL
// ============================================================
static bool loadAsciiSTL(std::ifstream& file,
                         std::vector<Eigen::Vector3d>& rawVertices,
                         std::vector<Eigen::Vector3i>& rawFaces) {
    std::string line;
    Eigen::Vector3d currentFace[3];
    int vertexIndex = 0;
    int triangleCount = 0;
    
    while (std::getline(file, line)) {
        // Удаляем ведущие пробелы
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) continue;
        line = line.substr(start);
        
        // Ищем строки "vertex x y z"
        if (line.rfind("vertex", 0) == 0) {
            std::istringstream iss(line.substr(6));
            double x, y, z;
            if (iss >> x >> y >> z) {
                if (vertexIndex < 3) {
                    currentFace[vertexIndex] = Eigen::Vector3d(x, y, z);
                    vertexIndex++;
                }
                
                if (vertexIndex == 3) {
                    // Добавляем треугольник
                    int baseIdx = static_cast<int>(rawVertices.size());
                    for (int v = 0; v < 3; ++v) {
                        rawVertices.push_back(currentFace[v]);
                    }
                    rawFaces.emplace_back(baseIdx, baseIdx + 1, baseIdx + 2);
                    
                    vertexIndex = 0;
                    triangleCount++;
                    
                    if (triangleCount % 100000 == 0) {
                        std::cout << "[Info] Parsed " << triangleCount 
                                  << " triangles..." << std::endl;
                    }
                }
            }
        }
    }
    
    std::cout << "[Info] ASCII STL: " << triangleCount << " triangles parsed" << std::endl;
    return triangleCount > 0;
}

// ============================================================
// Основная функция загрузки
// ============================================================
bool Mesh::loadFromSTL(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "[Error] Cannot open " << filename << std::endl;
        return false;
    }
    
    // Определяем формат
    STLFormat format = detectSTLFormat(filename);
    
    std::vector<Eigen::Vector3d> rawVertices;
    std::vector<Eigen::Vector3i> rawFaces;
    
    if (format == STLFormat::ASCII) {
        std::cout << "[Info] ASCII STL detected. Parsing..." << std::endl;
        file.seekg(0);  // Возвращаемся в начало
        if (!loadAsciiSTL(file, rawVertices, rawFaces)) {
            std::cerr << "[Error] Failed to parse ASCII STL" << std::endl;
            return false;
        }
    } else if (format == STLFormat::Binary) {
        // Читаем заголовок и количество треугольников
        char header[80];
        file.read(header, 80);
        
        uint32_t triangleCount = 0;
        file.read(reinterpret_cast<char*>(&triangleCount), 4);
        
        std::cout << "[Info] Binary STL detected. Triangles: " 
                  << triangleCount << std::endl;
        
        // Защита от некорректных данных
        if (triangleCount == 0 || triangleCount > MAX_TRIANGLES) {
            std::cerr << "[Error] Invalid triangle count: " << triangleCount 
                      << " (max allowed: " << MAX_TRIANGLES << ")" << std::endl;
            return false;
        }
        
        if (!loadBinarySTL(file, rawVertices, rawFaces, triangleCount)) {
            std::cerr << "[Error] Failed to parse binary STL" << std::endl;
            return false;
        }
    } else {
        std::cerr << "[Error] Unknown STL format" << std::endl;
        return false;
    }
    
    file.close();
    
    if (rawVertices.empty() || rawFaces.empty()) {
        std::cerr << "[Error] No geometry loaded" << std::endl;
        return false;
    }
    
    std::cout << "[Info] Raw data: " << rawVertices.size() 
              << " vertices, " << rawFaces.size() << " faces" << std::endl;
    
    // Сварка вершин и построение топологии
    weldVertices(rawVertices, rawFaces);

        computeBoundingBox();
    std::cout << "[Info] Bounding box: center=(" << center.x() << ", " << center.y() << ", " << center.z() 
              << "), radius=" << radius << std::endl;
    
    std::cout << "[Info] After welding: " << V.rows() 
              << " unique vertices, " << F.rows() << " faces" << std::endl;
    
    return true;
}

void Mesh::weldVertices(const std::vector<Eigen::Vector3d>& rawVertices,
                        const std::vector<Eigen::Vector3i>& rawFaces) {
    // Словарь для поиска дубликатов вершин
    std::unordered_map<Eigen::Vector3d, int, VectorHash> vertexMap;

    vertexMap.reserve(rawVertices.size() / 2);
    
    // Новый индекс для каждой вершины
    std::vector<int> newIndexMap(rawVertices.size());
    
    int uniqueVertexCount = 0;
    
    // Проходим по всем сырым вершинам
    for (size_t i = 0; i < rawVertices.size(); ++i) {
        const Eigen::Vector3d& pos = rawVertices[i];
        
        // Ищем существующую вершину с такой же позицией
        auto it = vertexMap.find(pos);
        
        if (it != vertexMap.end()) {
            // Вершина уже существует
            newIndexMap[i] = it->second;
        } else {
            // Новая уникальная вершина
            newIndexMap[i] = uniqueVertexCount;
            vertexMap[pos] = uniqueVertexCount;
            uniqueVertexCount++;
        }
    }
    
    // Создаём матрицу вершин V
    V.resize(uniqueVertexCount, 3);
    for (const auto& [pos, idx] : vertexMap) {
        V.row(idx) = pos.transpose();
    }
    
    // Создаём матрицу граней F
    F.resize(rawFaces.size(), 3);
    for (size_t i = 0; i < rawFaces.size(); ++i) {
        F(i, 0) = newIndexMap[rawFaces[i][0]];
        F(i, 1) = newIndexMap[rawFaces[i][1]];
        F(i, 2) = newIndexMap[rawFaces[i][2]];
    }
}

//------------------------------------------------------------
void Mesh::buildHalfEdgeStructure() {
    if (F.rows() == 0) return;
    
    // Предварительное выделение памяти
    int numFaces = F.rows();
    int numHalfEdges = numFaces * 3;
    
    halfEdges.resize(numHalfEdges);
    faces.resize(numFaces);
    vertices.resize(V.rows());
    
    // Шаг 1: Создаём half-edges для каждой грани
    std::unordered_map<std::pair<int, int>, int, PairHash> edgeToHalfEdge;
    
    for (int f = 0; f < numFaces; ++f) {
        faces[f].he = f * 3;  // Первый half-edge этой грани
        
        for (int i = 0; i < 3; ++i) {
            int heIdx = f * 3 + i;
            int v0 = F(f, i);
            int v1 = F(f, (i + 1) % 3);
            
            halfEdges[heIdx].vertex = v1;
            halfEdges[heIdx].face = f;
            halfEdges[heIdx].next = f * 3 + (i + 1) % 3;
            halfEdges[heIdx].prev = f * 3 + (i + 2) % 3;
            halfEdges[heIdx].twin = -1;  // Пока не знаем
            
            // Сохраняем для поиска twin
            edgeToHalfEdge[{v0, v1}] = heIdx;
            
            // Связываем вершину с одним из исходящих half-edges
            if (vertices[v0].he == -1) {
                vertices[v0].he = heIdx;
                vertices[v0].position = V.row(v0);
            }
        }
    }
    
    // Шаг 2: Находим twin для каждого half-edge
    for (int heIdx = 0; heIdx < numHalfEdges; ++heIdx) {
        if (halfEdges[heIdx].twin != -1) continue;  // Уже установлен
        
        int v0 = halfEdges[halfEdges[heIdx].prev].vertex;  // Начало текущего ребра
        int v1 = halfEdges[heIdx].vertex;                   // Конец текущего ребра
        
        // Ищем обратное ребро (v1 -> v0)
        auto it = edgeToHalfEdge.find({v1, v0});
        if (it != edgeToHalfEdge.end()) {
            int twinIdx = it->second;
            halfEdges[heIdx].twin = twinIdx;
            halfEdges[twinIdx].twin = heIdx;
        }
    }
    
    std::cout << "[Info] Built Half-Edge structure: " 
              << halfEdges.size() << " half-edges" << std::endl;
}


//------------------------------------------------------
void Mesh::buildTopology() {
    if (V.rows() == 0 || F.rows() == 0) {
        std::cerr << "[Error] Cannot build topology: mesh is empty" << std::endl;
        return;
    }
    
    std::cout << "[Info] Building Half-Edge topology..." << std::endl;
    buildHalfEdgeStructure();
    std::cout << "[Info] Topology built: " << vertices.size() << " vertices, "
              << halfEdges.size() << " half-edges, "
              << faces.size() << " faces" << std::endl;
}

//------------------------------------------------------
void Mesh::computeCurvature() {
    if (V.rows() == 0 || F.rows() == 0) return;
    
    std::cout << "[Info] Computing curvature via Laplace-Beltrami operator..." << std::endl;
    
    int n = V.rows();
    
    // 1. Матрица Лапласа-Бельтрами (котангенс-веса)
    Eigen::SparseMatrix<double> L;
    igl::cotmatrix(V, F, L);
    
    // 2. Массовая матрица (Voronoi area)
    Eigen::SparseMatrix<double> M;
    igl::massmatrix(V, F, igl::MASSMATRIX_TYPE_VORONOI, M);
    
    // 3. Mean curvature: H_i = -0.5 * (L * V_i) / M_ii
    //    Это дискретный аналог: Δp = 2H * n
    Eigen::MatrixXd LV = L * V;  // Лапласиан координат вершин
    
    meanCurvature.resize(n);
    gaussianCurvature.resize(n);
    
    for (int i = 0; i < n; ++i) {
        double area = M.coeff(i, i);  // Voronoi area вершины
        
        if (area < 1e-12) {
            meanCurvature[i] = 0.0;
            continue;
        }
        
        // |Δp| ≈ 2|H|
        double laplacianNorm = LV.row(i).norm();
        meanCurvature[i] = 0.5 * laplacianNorm / area;
    }
    
    // 4. Gaussian curvature через угловой дефект
    //    K_i = (2π - Σθ_j) / A_i
    Eigen::VectorXd angleSum = Eigen::VectorXd::Zero(n);
    
    for (int f = 0; f < F.rows(); ++f) {
        for (int v = 0; v < 3; ++v) {
            int i0 = F(f, v);
            int i1 = F(f, (v + 1) % 3);
            int i2 = F(f, (v + 2) % 3);
            
            Eigen::Vector3d e1 = V.row(i1) - V.row(i0);
            Eigen::Vector3d e2 = V.row(i2) - V.row(i0);
            
            double angle = atan2(e1.cross(e2).norm(), e1.dot(e2));
            angleSum(i0) += angle;
        }
    }
    
    for (int i = 0; i < n; ++i) {
        double area = M.coeff(i, i);
        if (area > 1e-12) {
            gaussianCurvature[i] = (2.0 * M_PI - angleSum(i)) / area;
        } else {
            gaussianCurvature[i] = 0.0;
        }
    }
    
    // 5. Восстановление κ₁, κ₂ из H и K
    //    H = (κ₁ + κ₂) / 2,  K = κ₁ * κ₂
    //    κ = H ± √(H² - K)
    for (int i = 0; i < n; ++i) {
        double H = meanCurvature[i];
        double K = gaussianCurvature[i];
        double discriminant = H * H - K;
        
        if (discriminant < 0.0) discriminant = 0.0;
        double sqrtD = std::sqrt(discriminant);
        
        double k1 = H + sqrtD;
        double k2 = H - sqrtD;
        
      if (!vertices.empty() && static_cast<int>(vertices.size()) == n) {
          for (int i = 0; i < n; ++i) {
              vertices[i].curvature = meanCurvature[i];//(H - minH) / range;
          }
      } else {
          std::cerr << "[Warning] vertices vector is empty or size mismatch ("
                    << vertices.size() << " vs " << n 
                    << "). Call buildTopology() before computeCurvature()." << std::endl;
          std::cerr << "[Warning] Curvature computed but not mapped to vertices." << std::endl;
      }
    }
    
    // Нормализация для тепловой карты
    double minH = *std::min_element(meanCurvature.begin(), meanCurvature.end());
    double maxH = *std::max_element(meanCurvature.begin(), meanCurvature.end());
    double range = maxH - minH;
    
    if (range < 1e-12) range = 1.0;
    
    for (int i = 0; i < n; ++i) {
        vertices[i].curvature = (meanCurvature[i] - minH) / range;
    }
    
    std::cout << "[Info] Curvature computed: H ∈ [" << minH << ", " << maxH << "]" << std::endl;
}

// ---------------------------------------------------
void Mesh::computeBoundingBox() {
    if (V.rows() == 0) {
        center = Eigen::Vector3d::Zero();
        radius = 1.0;
        return;
    }
    // Находим минимальные и максимальные координаты по осям
    Eigen::Vector3d minBound = V.colwise().minCoeff();
    Eigen::Vector3d maxBound = V.colwise().maxCoeff();
    
    center = (minBound + maxBound) / 2.0;
    radius = (maxBound - minBound).norm() / 2.0;
    
    // Защита от вырожденных моделей
    if (radius < 1e-6) radius = 1.0;
}

