#include "solid_model.h"

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

struct Vec3 { float x, y, z; };
float Zoom {5.0};
float aspect{};
const float baseSize = 100.0f;

//------------------------------------------------------------ обработчик зума
#include <QWheelEvent>

class WheelEventFilter : public QObject {
  protected:
    bool eventFilter(QObject* obj, QEvent* event) override {
      if (event->type() == QEvent::Wheel) {
				QWheelEvent* wheelEvent = static_cast<QWheelEvent*>(event);
				float delta = wheelEvent->angleDelta().y() / 120.0f;
				Zoom *= (1.0f + delta * 0.1f);    // Zoom - глобальная переменная
				if (Zoom < 0.1f) Zoom = 0.1f;
				if (Zoom > 10.0f) Zoom = 10.0f;
				return true;
			}
			return QObject::eventFilter(obj, event);
    }
};


//------------------------------------------------------------ градиент, свет
void drawModel(std::vector<float> &vertices) {
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// qWarning() << Zoom << " | drawModel";
	
	// Диметрическая проекция
	float rotY = 0.0f;
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glRotatef(35.264f, 1.0f, 0.0f, 0.0f);
	glRotatef(45.0f + rotY, 0.0f, 1.0f, 0.0f);
	rotY += 0.25f; // Плавное вращение для обзора  ЗАЧЕМ??

  // приближение камеры
	float size = baseSize / Zoom;
	glOrtho(-size * aspect, size * aspect, -size, size, -500, 500);

	// тип шейдера
	glShadeModel(GL_FLAT  ); 
	
	// min/max Y
	float minY = vertices[1], maxY = vertices[1];
	for (size_t i = 1; i < vertices.size(); i += 3) {
		if (vertices[i] < minY) minY = vertices[i];
		if (vertices[i] > maxY) maxY = vertices[i];
	}
	float rangeY = maxY - minY;
	if (rangeY < 0.001f) rangeY = 1.0f;
	
	// отрисовка модели с градиентом
	glBegin(GL_TRIANGLES);
	for (size_t i = 0; i < vertices.size(); i += 9) {
		float avgY = (vertices[i+1] + vertices[i+4] + vertices[i+7]) / 3.0f;
		float t = (avgY - minY) / rangeY;
		glColor3f(t, 1.0f - fabs(t - 0.5f) * 2.0f, 1.0f - t);
		for (int v = 0; v < 3; ++v) {
			glVertex3f(vertices[i + v*3], vertices[i + v*3 + 1], vertices[i + v*3 + 2]);
		}
	}
	glEnd();
	glDisableClientState(GL_VERTEX_ARRAY);
}

//------------------------------------------------------------ парсер бинарных STL
void parseSTL(const QString& filePath, std::vector<float> &vertices) {
	QFile file(filePath);
	if (!file.open(QIODevice::ReadOnly)) {
		qWarning() << "[Error] Cannot open" << filePath;
		return;
	}
    
	QByteArray header = file.read(80);
	QByteArray triangleCountBytes = file.read(4);
	
	if (triangleCountBytes.size() < 4) {
		qWarning() << "[Error] Failed to read triangle count";
		file.close();
		return;
	}
    
	quint32 triangleCount = *reinterpret_cast<quint32*>(triangleCountBytes.data());
  
	// Определяем формат: если количество треугольников не слишком большое
	//   и размер файла соответствует бинарному формату, то это бинарный STL
	quint64 fileSize = file.size();
	bool isBinary = false;
	
	if (fileSize == 84 + triangleCount * 50) {
		isBinary = true;
	} 
	else 
	{
		file.seek(0);
		QByteArray firstLine = file.readLine();
		if (firstLine.trimmed().startsWith("solid")) 
			isBinary = false;
		else 
			isBinary = true;
	}
    
	file.seek(0);
    
	if (isBinary) {
		qDebug() << "Binary STL detected. Triangles:" << triangleCount;

		file.skip(80);
		file.read(4);

		for (quint32 i = 0; i < triangleCount; ++i) {
			file.skip(12);
			for (int v = 0; v < 3; ++v) {
				float x, y, z;
				file.read(reinterpret_cast<char*>(&x), 4);
				file.read(reinterpret_cast<char*>(&y), 4);
				file.read(reinterpret_cast<char*>(&z), 4);
				vertices.push_back(x);
				vertices.push_back(y);
				vertices.push_back(z);
			}
			
			file.skip(2);
		}
		qDebug() << "Extracted" << vertices.size() / 3 << "vertices";
	} 
	else 
	{
		qDebug() << "ASCII STL detected";
		
		// Существующий код для ASCII STL
		QTextStream in(&file);
		QString line;
		while (in.readLineInto(&line)) {
			QString trimmed = line.trimmed();
			if (trimmed.startsWith("vertex", Qt::CaseInsensitive)) {
				QStringList parts = trimmed.split(QRegularExpression("\\s+"));
				if (parts.size() >= 4) {
					vertices.push_back(parts[1].toFloat());
					vertices.push_back(parts[2].toFloat());
					vertices.push_back(parts[3].toFloat());
				}
			}
		}
	}
	file.close();
}

// ------------------------------------------------------------------------------
// Функция получения полного пути к файлу в assets
QString getAssetPath(const QString& filename) {
    return QDir(QString(ASSETS_DIR)).filePath(filename);
}

//==========================================================================
int c_main(int argc, char* argv[]) {
	// 1. Настройка формата OpenGL (Compatibility Profile для legacy-функций)
	QSurfaceFormat fmt;
	fmt.setVersion(2, 1);
	fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
	fmt.setDepthBufferSize(24);
	QSurfaceFormat::setDefaultFormat(fmt);

	QApplication app(argc, argv);

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -  возврат вертексов
	// 2. Парсинг ASCII STL (упрощённый, но достаточный для демо)
	QString stl_path = getAssetPath("base_wall_mount_vc.stl");
	std::vector<float> vertices;
	parseSTL(stl_path, vertices);

	if (vertices.empty()) {
			qWarning() << "[Error] No vertices found in STL.";
			return 1;
	}
	
	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - Создание GUI
	QWidget window;
	window.setWindowTitle("STL Viewer (Dimetric Projection)");
	window.resize(800, 600);
	
	QVBoxLayout layout(&window);
	layout.setContentsMargins(0, 0, 0, 0);
	
	QOpenGLWidget glWidget;
	glWidget.setAttribute(Qt::WA_NoSystemBackground);
	glWidget.setAutoFillBackground(false);
	layout.addWidget(&glWidget);

	// Устанавливаем фильтр событий
  WheelEventFilter wheelFilter;
  glWidget.installEventFilter(&wheelFilter);

	window.show();
	app.processEvents(); // создание OpenGL-контекста

	// 4. Инициализация OpenGL
	glWidget.makeCurrent();
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glClearColor(0.12f, 0.12f, 0.18f, 1.0f);

	// 5. Ручной цикл рендера (чтобы избежать лямбд/slots и строго соблюсти условие)
	int lastW = 0, lastH = 0;
	bool running = true;

	// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - ОБРАБОКА СОБЫТИЙ
	while (running) {
		app.processEvents();
		if (window.isHidden()) break;

		glWidget.makeCurrent();
		int w = glWidget.width();
		int h = glWidget.height();

		// Обновление проекции
		glViewport(0, 0, w, h);
		aspect = static_cast<float>(w) / h;
		drawModel(vertices);
				
		glWidget.context()->swapBuffers(glWidget.context()->surface());
		usleep(16000); // ~60 FPS
	}

	return 0;
}