
#pragma once

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QThread>
#include <QQuaternion>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QDialog>

#include "render_data.h"
#include "mesh_worker.h"


#include <QString>

int stl_review(const QString& stl_title, int argc, char* argv[]);

//-----------------------------------------------------------
class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
    Q_OBJECT
public:
    explicit GLWidget(QWidget* parent = nullptr);
    ~GLWidget() override;

    void loadAsync(const QString& stlPath);

signals:
    void progress(int percent, QString stage);
    void startWork(const QString& path);
    void loadFailed(const QString& reason);

protected:
    void initializeGL() override;
    void resizeGL(int w, int h) override;
    void paintGL() override;

    void wheelEvent(QWheelEvent* e) override;
    void mousePressEvent(QMouseEvent* e) override;
    void mouseMoveEvent(QMouseEvent* e) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void mouseDoubleClickEvent(QMouseEvent* e) override;

private slots:
    void onDataReady(RenderDataPtr data);

private:
    void initShaders();
    void uploadToGpu();
    void rebuildMvp();
    QVector3D mapToSphere(const QPoint& p) const;

    // Данные
    RenderDataPtr m_data;
    bool          m_glReady = false;
    bool          isPanning = false;
    Mesh mesh;
    float panX = 0.0f;
    float panY = 0.0f;

    // GL-ресурсы
    QOpenGLShaderProgram    m_shader;
    QOpenGLVertexArrayObject m_vao;
    QOpenGLBuffer           m_vbo{QOpenGLBuffer::VertexBuffer};
    QOpenGLBuffer           m_ebo{QOpenGLBuffer::IndexBuffer};
    GLsizei                 m_indexCount = 0;

    // Камера
    float       zoom = 1.0f;
    QQuaternion arcballRotation;
    QMatrix4x4  m_mvp;
    QPoint      lastMousePos;
    QVector3D   lastPointOnSphere;
    bool        isRotating = false;

    // Многопоточность
    QThread      m_workerThread;
    MeshWorker*  m_worker = nullptr;
};