
#include "stl_model.h"
#include "../mesh/mesh.h"

#include <iostream>
#include <QApplication>
#include <QOpenGLWidget>
#include <QVBoxLayout>
#include <QWidget>
#include <QFile>
#include <QSurfaceFormat>
#include <vector>
#include <cmath>
#include <QDebug>
#include <QString>
#include <QDir>
#include <QOpenGLFunctions>
#include <QWheelEvent>
#include <QQuaternion>
#include <QVector3D>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QFileInfo>
#include <QTimer>
#include <QMessageBox>

// --- Конструктор: создание потока и связывание сигналов ---
GLWidget::GLWidget(QWidget* parent) : QOpenGLWidget(parent) {
    setFocusPolicy(Qt::StrongFocus);

    m_worker = new MeshWorker();
    m_worker->moveToThread(&m_workerThread);

    connect(&m_workerThread, &QThread::finished, m_worker, &QObject::deleteLater);

    // UI -> Worker (запуск задачи)
    connect(this,        &GLWidget::startWork,
            m_worker,    &MeshWorker::process);
    // Worker -> UI (результат через очередь)
    connect(m_worker,    &MeshWorker::finished,
            this,        &GLWidget::onDataReady,
            Qt::QueuedConnection);
    // Прогресс — тоже через очередь
    connect(m_worker,    &MeshWorker::progress,
            this,        &GLWidget::progress,
            Qt::QueuedConnection);
    connect(m_worker, &MeshWorker::failed, this, [this](const QString& reason){
        qWarning() << "[Worker] Failed:" << reason;
        emit loadFailed(reason);
    }, Qt::QueuedConnection);

    m_workerThread.start();
}

// --- Деструктор: корректное завершение потока ---
GLWidget::~GLWidget() {
    m_worker->requestCancel();
    m_workerThread.quit();
    m_workerThread.wait();

    if (m_glReady) {
        makeCurrent();
        m_vao.destroy();
        m_vbo.destroy();
        m_ebo.destroy();
        doneCurrent();
    }
}

// --- Публичный метод запуска загрузки ---
void GLWidget::loadAsync(const QString& stlPath) {
    emit startWork(stlPath);
}

// --- Инициализация OpenGL ---
void GLWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.15f, 0.15f, 0.20f, 1.0f);
    glEnable(GL_DEPTH_TEST);

    initShaders();

    m_vao.create();
    m_vbo.create();
    m_ebo.create();
    m_vbo.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_ebo.setUsagePattern(QOpenGLBuffer::StaticDraw);

    m_glReady = true;

    if (isPanning && m_data) {
        uploadToGpu();
        isPanning = false;
    }
}

void GLWidget::resizeGL(int w, int h) {
    glViewport(0, 0, w, h);
}

// --- Отрисовка ---
void GLWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_data || m_indexCount == 0) return;

    rebuildMvp();

    m_shader.bind();
    m_shader.setUniformValue("mvp", m_mvp);

    m_vao.bind();
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, nullptr);
    m_vao.release();
}

void GLWidget::wheelEvent(QWheelEvent *event)  {
    float delta = event->angleDelta().y() / 120.0f;
    zoom *= (1.0f + delta * 0.1f);
    if (zoom < 0.1f) zoom = 0.1f;
    if (zoom > 10.0f) zoom = 10.0f;
    update();
}

void GLWidget:: mousePressEvent(QMouseEvent *event)  {
    if (event->button() == Qt::LeftButton) {
        lastMousePos = event->pos();
        lastPointOnSphere = mapToSphere(event->pos());
        isRotating = true;
    } else if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        lastMousePos = event->pos();
        isPanning = true;
    }
}

void GLWidget:: mouseMoveEvent(QMouseEvent *event)  {
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
        // float panSpeed = 0.005f * (mesh.radius * 2.0f) / zoom;
        // panX += delta.x() * panSpeed;
        // panY -= delta.y() * panSpeed; // Y инвертирован в экранных координатах
        lastMousePos = event->pos();
        update();
    }
}

void GLWidget:: mouseReleaseEvent(QMouseEvent *event)  {
    if (event->button() == Qt::LeftButton) {
        isRotating = false;
    } else if (event->button() == Qt::RightButton || event->button() == Qt::MiddleButton) {
        isPanning = false;
    }
}

void GLWidget:: mouseDoubleClickEvent(QMouseEvent *event)  {
    if (event->button() == Qt::LeftButton) {
        // Сброс вида
        arcballRotation = QQuaternion();
        zoom = 1.0f;
        // panX = 0.0f;
        // panY = 0.0f;
        update();
    }
}

// --- Получение данных из worker'а ---
void GLWidget::onDataReady(RenderDataPtr data) {
    m_data = data;
    if (m_glReady) {
        uploadToGpu();
    } else {
        isPanning  = true;
    }
    update();
}

void GLWidget::initShaders() {
    const QString dir = QString(ASSETS_DIR) + "/../src/shaders/";
    if (!m_shader.addShaderFromSourceFile(QOpenGLShader::Vertex,
                                          dir + "heatmap.vert"))
        qFatal("Vertex shader: %s", qPrintable(m_shader.log()));
    if (!m_shader.addShaderFromSourceFile(QOpenGLShader::Fragment,
                                          dir + "heatmap.frag"))
        qFatal("Fragment shader: %s", qPrintable(m_shader.log()));
    if (!m_shader.link())
        qFatal("Shader link: %s", qPrintable(m_shader.log()));
}

// --- Загрузка в GPU (выполняется ТОЛЬКО в UI-потоке) ---
void GLWidget::uploadToGpu() {
    if (!m_data || m_data->vertices.empty() || m_data->indices.empty()) return;

    makeCurrent();
    m_vao.bind();

    // VBO: interleaved [x,y,z,r,g,b]
    m_vbo.bind();
    m_vbo.allocate(m_data->vertices.data(),
                   static_cast<int>(m_data->vertices.size() * sizeof(RenderVertex)));

    // Атрибут 0: aPos (vec3)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertex),
                          reinterpret_cast<void*>(0));
    // Атрибут 1: aColor (vec3)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(RenderVertex),
                          reinterpret_cast<void*>(3 * sizeof(float)));

    // EBO: индексы треугольников
    m_ebo.bind();
    m_ebo.allocate(m_data->indices.data(),
                   static_cast<int>(m_data->indices.size() * sizeof(uint32_t)));
    m_indexCount = static_cast<GLsizei>(m_data->indices.size());

    m_vao.release();
    doneCurrent();
}

// --- Матрица mvp ---
void GLWidget::rebuildMvp() {
    const float aspect = (height() == 0) ? 1.0f
                                         : float(width()) / float(height());

    QMatrix4x4 proj;
    proj.perspective(45.0f, aspect, 0.1f, 1000.0f);

    QMatrix4x4 view;
    const float camDist = (m_data ? 3.0f * m_data->radius : 3.0f) / zoom;
    view.translate(0.0f, 0.0f, -camDist);

    QMatrix4x4 model;
    if (m_data) {
        model.translate(-m_data->center.x(),
                        -m_data->center.y(),
                        -m_data->center.z());
    }
    QMatrix4x4 rot;
    float angle; QVector3D axis;
    arcballRotation.getAxisAndAngle(&axis, &angle);  
    rot.rotate(angle, axis);
    model = rot * model;

    m_mvp = proj * view * model;
}

// Проекция 2D-координат мыши на единичную сферу (классический Arcball Shoemake)
QVector3D GLWidget::mapToSphere(const QPoint& p) const{
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

//--------------------------------------------------------------------
int stl_review(const QString& initial_stl_title, int argc, char* argv[]) {
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(0);
    QSurfaceFormat::setDefaultFormat(fmt);

    qRegisterMetaType<RenderDataPtr>("RenderDataPtr");

    QApplication app(argc, argv);

    // 1. Основной виджет рендеринга
    GLWidget* glWidget = new GLWidget();
    glWidget->setMinimumSize(800, 600);
    glWidget->setWindowTitle("ISEPT: Анализ полигональной модели");

    // 2. Компоновка главного окна
    QWidget* mainWindow = new QWidget();
    QVBoxLayout* mainLayout = new QVBoxLayout(mainWindow);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(4);

    // 3. Верхняя панель управления
    QHBoxLayout* topBar = new QHBoxLayout();
    topBar->setContentsMargins(0, 0, 0, 0);
    
    QPushButton* loadBtn = new QPushButton("Загрузить STL...");
    loadBtn->setFixedSize(160, 32);
    loadBtn->setCursor(Qt::PointingHandCursor);
    
    QLabel* fileInfoLabel = new QLabel("Файл не выбран");
    fileInfoLabel->setStyleSheet("color: #888; font-style: italic; padding-left: 10px;");
    
    topBar->addWidget(loadBtn);
    topBar->addWidget(fileInfoLabel);
    topBar->addStretch(); // Прижимает элементы влево

    mainLayout->addLayout(topBar);
    mainLayout->addWidget(glWidget, 1);

    // 4. Отдельное окно прогресса
    QDialog* progressDlg = new QDialog(mainWindow);
    progressDlg->setWindowTitle("Обработка геометрии");
    progressDlg->setFixedSize(350, 100);
    progressDlg->setWindowFlags(progressDlg->windowFlags() | Qt::WindowStaysOnTopHint | Qt::Dialog);
    
    QVBoxLayout* dlgLayout = new QVBoxLayout(progressDlg);
    QLabel* statusLabel = new QLabel("Ожидание запуска...");
    statusLabel->setAlignment(Qt::AlignCenter);
    
    QProgressBar* progressBar = new QProgressBar();
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    
    dlgLayout->addWidget(statusLabel);
    dlgLayout->addWidget(progressBar);

    // 5. Лямбда-функция для запуска загрузки
    auto loadFile = [&](const QString& filePath) {
        if (filePath.isEmpty()) return;
        
        fileInfoLabel->setText(QString("<b>Текущий файл:</b> %1").arg(QFileInfo(filePath).fileName()));
        fileInfoLabel->setStyleSheet("color: #222; font-style: normal; font-weight: bold;");
        
        progressBar->setValue(0);
        statusLabel->setText("Инициализация...");
        progressDlg->show(); // Показываем диалог
        
        glWidget->loadAsync(filePath);
    };

    // 6. Связь прогресса с диалогом
    QObject::connect(glWidget, &GLWidget::progress,
        [=](int percent, const QString& stage) {
            progressBar->setValue(percent);
            statusLabel->setText(QString("%1% — %2").arg(percent).arg(stage));
            
            if (percent >= 100) {
                // Небольшая задержка, чтобы пользователь успел увидеть "100% — Готово"
                QTimer::singleShot(600, progressDlg, &QDialog::hide);
            }
        });

    // 7. Обработка ошибок (чтобы диалог не зависал при битом STL)
    QObject::connect(glWidget, &GLWidget::loadFailed,
        [=](const QString& reason) {
            progressDlg->hide();
            QMessageBox::warning(mainWindow, "Ошибка загрузки", 
                QString("Не удалось обработать модель:\n%1").arg(reason));
            fileInfoLabel->setText("Ошибка загрузки");
            fileInfoLabel->setStyleSheet("color: red; font-style: italic;");
        });

    // 8. Обработчик нажатия на кнопку
    QObject::connect(loadBtn, &QPushButton::clicked, [&]() {
        // Начальная директория: папка assets или домашняя директория пользователя
        QString startDir = QString(ASSETS_DIR);
        if (!QDir(startDir).exists()) {
            startDir = QDir::homePath();
        }

        QString filePath = QFileDialog::getOpenFileName(
            mainWindow,
            "Выберите полигональную модель (STL)",
            startDir,
            "STL файлы (*.stl *.STL);;Все файлы (*.*)"
        );
        
        if (!filePath.isEmpty()) {
            loadFile(filePath);
        }
    });

    // 9. Финальная настройка окна
    mainWindow->resize(1024, 800);
    mainWindow->show();

    // 10. Автозагрузка начального файла, если он передан и существует
    QString initialPath = QString(ASSETS_DIR) + "/" + initial_stl_title;
    if (QFile::exists(initialPath)) {
        loadFile(initialPath);
    }

    return app.exec();
}