
#include "stl_model.h"
#include "../mesh/mesh.h"

#include <iostream>
#include <QApplication>
#include <QOpenGLWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QRegularExpression>
#include <QSurfaceFormat>
#include <vector>
#include <cmath>
#include <unistd.h>
#include <QDebug>
#include <QOpenGLContext>
#include <QString>
#include <filesystem>
#include <QDir>
#include <GL/gl.h>
#include <QOpenGLFunctions>
#include <QDataStream>
#include <QByteArray>
#include <QWheelEvent>
#include <QQuaternion>
#include <QVector3D>
#include <QKeyEvent>
#include <QtMath>
#include <QMatrix4x4>
#include <QMouseEvent>

//--------------------------------------------------------------------
class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
private:
    Mesh mesh;
    
    // Параметры камеры
    float zoom = 1.0f;
    float panX = 0.0f;
    float panY = 0.0f;
    QQuaternion arcballRotation;
    
    // Состояние мыши
    QPoint lastMousePos;
    QVector3D lastPointOnSphere;
    bool isRotating = false;
    bool isPanning = false;

public:
    GLWidget(QWidget* parent = nullptr) : QOpenGLWidget(parent) {
        setFocusPolicy(Qt::StrongFocus);
    }

    void loadModel(const QString& filename) {
        if (!mesh.loadFromSTL(filename.toStdString())) {
            std::cerr << "[Error] Failed to load STL" << std::endl;
            return;
        }
        mesh.buildTopology();
        mesh.computeCurvature();
        update();
    }

protected:
    void initializeGL() override {
        initializeOpenGLFunctions();
        glClearColor(0.15f, 0.15f, 0.18f, 1.0f); // Тёмный CAD-фон
        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE); // Рисуем обе стороны треугольников
    }

    void resizeGL(int w, int h) override {
        glViewport(0, 0, w, h);
    }

    void paintGL() override {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        float aspect = (float)width() / (float)height();
        if (aspect < 0.1f) aspect = 0.1f; // Защита от деления на ноль
        
        float scale = static_cast<float>(mesh.radius * 1.5 / zoom);
    
        glOrtho(-scale * aspect, scale * aspect, -scale, scale, 
            -mesh.radius * 10.0, mesh.radius * 10.0);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();
        glTranslatef(mesh.center.x(), mesh.center.y(), mesh.center.z());
				QMatrix4x4 rotMatrix;
				rotMatrix.rotate(arcballRotation);
				glMultMatrixf(rotMatrix.constData());
        glTranslatef(-mesh.center.x(), -mesh.center.y(), -mesh.center.z());
        drawModel();
    }

    void drawModel() {
        if (mesh.V.rows() == 0) return;
        
        glBegin(GL_TRIANGLES);
        for (int f = 0; f < mesh.F.rows(); ++f) {
            for (int v = 0; v < 3; ++v) {
                int vertexIdx = mesh.F(f, v);
                Eigen::Vector3d pos = mesh.V.row(vertexIdx);
                double curvature = mesh.vertices[vertexIdx].curvature;
                
                // Тепловая карта: синий (0) -> красный (1)
                float r = static_cast<float>(curvature);
                float g = 0.0f;
                float b = 1.0f - r;
                
                glColor3f(r, g, b);
                glVertex3d(pos.x(), pos.y(), pos.z());
            }
        }
        glEnd();
    }

    void wheelEvent(QWheelEvent *event) override {
        float delta = event->angleDelta().y() / 120.0f;
        zoom *= (1.0f + delta * 0.1f);
        if (zoom < 0.1f) zoom = 0.1f;
        if (zoom > 10.0f) zoom = 10.0f;
        update();
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            lastMousePos = event->pos();
            lastPointOnSphere = mapToSphere(event->pos());
            isRotating = true;
        } else if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
            lastMousePos = event->pos();
            isPanning = true;
        }
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        if (isRotating && (event->buttons() & Qt::LeftButton)) {
            QVector3D currentPoint = mapToSphere(event->pos());
            QVector3D axis = QVector3D::crossProduct(lastPointOnSphere, currentPoint);
            float dot = QVector3D::dotProduct(lastPointOnSphere, currentPoint);
            
            // Ограничиваем dot диапазоном [-1, 1] для безопасности acos
            dot = std::max(-1.0f, std::min(1.0f, dot));
            
            if (axis.lengthSquared() > 1e-6f) {
                float angle = std::acos(dot) * 180.0f / 3.14159265358979323846f;
                QQuaternion deltaQ = QQuaternion::fromAxisAndAngle(axis.normalized(), angle);
                arcballRotation = deltaQ * arcballRotation;
                update();
            }
            lastPointOnSphere = currentPoint;
            lastMousePos = event->pos();
            
        } else if (isPanning && (event->buttons() & (Qt::RightButton | Qt::MiddleButton))) {
            QPoint delta = event->pos() - lastMousePos;
            // Скорость панорамирования зависит от размера модели и зума
            float panSpeed = 0.005f * (mesh.radius * 2.0f) / zoom;
            panX += delta.x() * panSpeed;
            panY -= delta.y() * panSpeed; // Y инвертирован в экранных координатах
            lastMousePos = event->pos();
            update();
        }
    }

    void mouseReleaseEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            isRotating = false;
        } else if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
            isPanning = false;
        }
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            // Сброс вида
            arcballRotation = QQuaternion();
            zoom = 1.0f;
            panX = 0.0f;
            panY = 0.0f;
            update();
        }
    }

private:
    // Проекция 2D-координат мыши на единичную сферу (классический Arcball Shoemake)
    QVector3D mapToSphere(const QPoint& p) {
        float x = (2.0f * p.x() - width()) / width();
        float y = (height() - 2.0f * p.y()) / height(); // Y инвертирован
        float r2 = x * x + y * y;
        float z;
        if (r2 <= 0.5f) {
            z = std::sqrt(1.0f - r2); // внутри сферы
        } else {
            z = 0.5f / std::sqrt(r2); // снаружи (гиперболический лист для избежания разрывов)
        }
        return QVector3D(x, y, z).normalized();
    }
};

//------------------------------------------------------------------
int stl_review(const QString& stl_title, int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    GLWidget widget;
    widget.resize(800, 600);
    widget.setWindowTitle(stl_title);
    
    // Загружаем модель
    QString assetPath = QString(ASSETS_DIR) + "/" + stl_title;
    widget.loadModel(assetPath);
    
    widget.show();
    
    return app.exec();
}