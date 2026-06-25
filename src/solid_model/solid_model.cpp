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
#include <QWheelEvent>


struct Vec3 { float x, y, z; };

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


// = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =  ООП реализация
class GLWidget : public QOpenGLWidget, protected QOpenGLFunctions {
public:
	explicit GLWidget(QWidget *parent = nullptr) : QOpenGLWidget(parent) {
		setFocusPolicy(Qt::StrongFocus); 
	}

	void setVertices(const std::vector<float>& verts) {
		vertices = verts;
	}

protected:
	void initializeGL() override {
		initializeOpenGLFunctions();
		glClearColor(0.12f, 0.12f, 0.18f, 1.0f);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glShadeModel(GL_FLAT);
	}

	void paintGL() override {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		
		int w = width();
		int h = height();
		float aspect = (h == 0) ? 1.0f : static_cast<float>(w) / h;
		float size = baseSize / zoom; 
		glOrtho(-size * aspect, size * aspect, -size, size, -500, 500);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();
		glRotatef(rotX, 1.0f, 0.0f, 0.0f);
    glRotatef(rotY, 0.0f, 1.0f, 0.0f);
    glRotatef(35.264f, 1.0f, 0.0f, 0.0f);
    glRotatef(45.0f,   0.0f, 1.0f, 0.0f);

		drawModel();
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
			lastPos = event->pos();
			isRotating = true;
		}
	}

	void mouseMoveEvent(QMouseEvent *event) override {
		if (isRotating && (event->buttons() & Qt::LeftButton)) {
			QPoint delta = event->pos() - lastPos;
			const float sensitivity = 0.5f;
			rotY += delta.x() * sensitivity;
			rotX += delta.y() * sensitivity;
			lastPos = event->pos();
			update(); // перерисовать кадр
		}
	}

	void mouseReleaseEvent(QMouseEvent *event) override {
		if (event->button() == Qt::LeftButton) {
			isRotating = false;
		}
	}

	void mouseDoubleClickEvent(QMouseEvent *event) override {
		if (event->button() == Qt::LeftButton) {
			rotX = 0.0f;
			rotY = 0.0f;
			zoom = 5.0f;
			update();
		}
	}

private:
	void drawModel() {
		if (vertices.empty()) return;

		// Поиск min/max Y для градиента
		float minY = vertices[1], maxY = vertices[1];
		for (size_t i = 1; i < vertices.size(); i += 3) {
			if (vertices[i] < minY) minY = vertices[i];
			if (vertices[i] > maxY) maxY = vertices[i];
		}
		float rangeY = maxY - minY;
		if (rangeY < 0.001f) rangeY = 1.0f;

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
	}

	std::vector<float> vertices;
	float zoom = 5.0f;
	const float baseSize = 100.0f;

	// Параметры вращения
	float rotX = 0.0f;   // угол вокруг оси X (наклон вверх-вниз)
	float rotY = 0.0f;   // угол вокруг оси Y (поворот влево-вправо)
	QPoint lastPos;      // предыдущая позиция курсора
	bool isRotating = false;
};


//========================================================================= ТОЧКА ВХОДА
int c_main(int argc, char* argv[]) {
	QSurfaceFormat fmt;
	fmt.setVersion(2, 1);
	fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
	fmt.setDepthBufferSize(24);
	QSurfaceFormat::setDefaultFormat(fmt);

	QApplication app(argc, argv);

	QString stl_path = getAssetPath("base_wall_mount_vc.stl");
	std::vector<float> vertices;
	parseSTL(stl_path, vertices);
	if (vertices.empty()) {
		qWarning() << "[Error] No vertices found in STL.";
		return 1;
	}

	QWidget window;
	window.setWindowTitle("STL Viewer (Dimetric Projection)");
	window.resize(800, 600);
	
	QVBoxLayout layout(&window);
	layout.setContentsMargins(0, 0, 0, 0);
	
	GLWidget glWidget; // Используем наш ООП-класс
	glWidget.setVertices(vertices); // Передаем данные
	layout.addWidget(&glWidget);
	window.show();

	return app.exec(); 
}