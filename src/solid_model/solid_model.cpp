// main.cpp
// STL Viewer: Dimetric Projection, Flat/Procedural Style, Qt5 + OpenGL 3.3 Core
// Compile: g++ -std=c++20 main.cpp -o stl_viewer -lGL -lQt5Core -lQt5Gui -lQt5Widgets -lQt5OpenGL

#include <QApplication>
#include <QOpenGLWidget>
#include <QOpenGLFunctions_3_3_Core>
#include <QOpenGLShaderProgram>
#include <QOpenGLBuffer>
#include <QOpenGLVertexArrayObject>
#include <QMatrix4x4>
#include <QVector3D>
#include <QFile>
#include <QMouseEvent>
#include <QKeyEvent>
#include <iostream>
#include <vector>
#include <cstdint>

// Глобальное состояние (процедурный стиль)
static std::vector<float> g_vertices;
static GLuint g_vertexCount = 0;
static bool g_vboUploaded = false;
static QOpenGLShaderProgram* g_shader = nullptr;
static QOpenGLVertexArrayObject* g_vao = nullptr;
static QOpenGLBuffer* g_vbo = nullptr;

// Минимальный каркас виджета (обязателен для Qt OpenGL-контекста)
struct Viewer : QOpenGLWidget, protected QOpenGLFunctions_3_3_Core {
    void initializeGL() override {
        initializeOpenGLFunctions();
        glEnable(GL_DEPTH_TEST);
        glEnable(GL_CULL_FACE);
        glCullFace(GL_BACK);
        glClearColor(0.95f, 0.95f, 0.95f, 1.0f);

        g_shader = new QOpenGLShaderProgram(this);
        g_shader->addShaderFromSourceCode(QOpenGLShader::Vertex,
            "#version 330 core\n"
            "layout(location=0) in vec3 aPos;\n"
            "uniform mat4 uMVP;\n"
            "void main(){ gl_Position = uMVP * vec4(aPos, 1.0); }");
        g_shader->addShaderFromSourceCode(QOpenGLShader::Fragment,
            "#version 330 core\n"
            "out vec4 c;\n"
            "void main(){ c = vec4(0.25, 0.55, 0.85, 1.0); }");
        g_shader->link();

        g_vao = new QOpenGLVertexArrayObject();
        g_vao->create();
        g_vbo = new QOpenGLBuffer(QOpenGLBuffer::VertexBuffer);
        g_vbo->create();
    }

    void resizeGL(int w, int h) override {
        glViewport(0, 0, w, h);
    }

    void paintGL() override {
        if (g_vertices.empty() || !g_shader->isLinked()) return;
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Диметрическая матрица (ГОСТ 2.317-2011)
        QMatrix4x4 proj, view;
        proj.ortho(-width() * 0.01f, width() * 0.01f, 
                   -height() * 0.01f, height() * 0.01f, 
                   -100.f, 100.f);
        view.lookAt(QVector3D(2.f, 1.f, 2.f), 
                    QVector3D(0.f, 0.f, 0.f), 
                    QVector3D(0.f, 1.f, 0.f));
        QMatrix4x4 mvp = proj * view;

        g_shader->bind();
        g_shader->setUniformValue("uMVP", mvp);

        g_vao->bind();
        if (!g_vboUploaded) {
            g_vbo->bind();
            g_vbo->allocate(g_vertices.data(), g_vertices.size() * sizeof(float));
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
            glEnableVertexAttribArray(0);
            g_vboUploaded = true;
        }
        glDrawArrays(GL_TRIANGLES, 0, g_vertexCount);
        g_vao->release();
        g_shader->release();
    }

    // Полное игнорирование ввода
    void mousePressEvent(QMouseEvent*) override {}
    void mouseMoveEvent(QMouseEvent*) override {}
    void wheelEvent(QWheelEvent*) override {}
    void keyPressEvent(QKeyEvent*) override {}
};

int p_main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    
    // Парсинг STL (inline в main)
    QString path = (argc > 1) ? argv[1] : "model.stl";
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        std::cerr << "Cannot open: " << path.toStdString() << "\n";
        return 1;
    }
    f.seek(80); // skip header
    uint32_t numTris = 0;
    f.read(reinterpret_cast<char*>(&numTris), 4);
    
    g_vertices.reserve(numTris * 9);
    struct Tri { float n[3]; float v[3][3]; uint16_t pad; };
    Tri t;
    for (uint32_t i = 0; i < numTris; ++i) {
        f.read(reinterpret_cast<char*>(&t), 50);
        for (int j = 0; j < 3; ++j) {
            g_vertices.push_back(t.v[j][0]);
            g_vertices.push_back(t.v[j][1]);
            g_vertices.push_back(t.v[j][2]);
        }
    }
    g_vertexCount = g_vertices.size() / 3;

    // Запуск UI
    Viewer w;
    w.setWindowTitle("STL Dimetric Viewer");
    w.resize(1024, 768);
    w.show();
    return app.exec();
}