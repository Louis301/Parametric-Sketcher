#include "stl_model.h"

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
		glClearColor(0.15f, 0.15f, 0.20f, 1.0f);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glShadeModel(GL_SMOOTH);  // Плавное затенение (или GL_FLAT для гранёного)

		// --- Включаем освещение ---
		glEnable(GL_LIGHTING);
		glEnable(GL_LIGHT0);
		glEnable(GL_NORMALIZE);   // Автоматическая нормализация нормалей
															// (важно после поворотов модели!)

		// --- Параметры источника света ---
		GLfloat lightAmbient[]  = { 0.25f, 0.25f, 0.25f, 1.0f };  // Фоновый
		GLfloat lightDiffuse[]  = { 0.80f, 0.80f, 0.80f, 1.0f };  // Рассеянный
		GLfloat lightSpecular[] = { 0.60f, 0.60f, 0.60f, 1.0f };  // Бликовый
		// Свет "привязан к камере" — позиция задаётся в координатах наблюдателя
		GLfloat lightPosition[] = { 0.5f, 0.8f, 1.0f, 0.0f };     // Направленный (w=0)

		glLightfv(GL_LIGHT0, GL_AMBIENT,  lightAmbient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE,  lightDiffuse);
		glLightfv(GL_LIGHT0, GL_SPECULAR, lightSpecular);
		glLightfv(GL_LIGHT0, GL_POSITION, lightPosition);

		// Параметры материала (цвет модели)
		setupMaterial();

		// Двухстороннее освещение
		glLightModeli(GL_LIGHT_MODEL_TWO_SIDE, GL_TRUE);
	}

	
	void paintGL() override {
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();

    // окно
		int w = width();
		int h = height();
		
		// приближение
		float aspect = (h == 0) ? 1.0f : static_cast<float>(w) / h;
		float size = baseSize / zoom; 
		glOrtho(-size * aspect, size * aspect, -size, size, -500, 500);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		// Свет
		GLfloat lightPos[] = { 0.5f, 0.8f, 1.0f, 0.0f };
		glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
		
    // (Arcball)
		float angle;
		QVector3D axis;
		arcballRotation.getAxisAndAngle(&axis, &angle);
		glRotatef(angle, axis.x(), axis.y(), axis.z());

		// Диметрическая проекция
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
			lastMousePos = event->pos();
			lastPointOnSphere = mapToSphere(event->pos());
			isRotating = true;
		}
	}

	void mouseMoveEvent(QMouseEvent *event) override {
		if (isRotating && (event->buttons() & Qt::LeftButton)) {
			QVector3D currentPoint = mapToSphere(event->pos());

			// Кватернион вращения между двумя точками на сфере
			// q = (v1 × v2, v1 · v2)
			QVector3D axis = QVector3D::crossProduct(lastPointOnSphere, currentPoint);
			float dot = QVector3D::dotProduct(lastPointOnSphere, currentPoint);

			// Если точки близки — не вращаем
			if (axis.lengthSquared() > 1e-6f) {
				QQuaternion deltaQ = QQuaternion::fromAxisAndAngle(axis.normalized(), qRadiansToDegrees(acosf(dot)));
				// Накапливаем вращение: новое применяется ПЕРЕД текущим
				arcballRotation = deltaQ * arcballRotation;
				update();
			}

			lastPointOnSphere = currentPoint;
			lastMousePos = event->pos();
		}
	}

	void mouseReleaseEvent(QMouseEvent *event) override {
		if (event->button() == Qt::LeftButton) {
			isRotating = false;
		}
  }

	void mouseDoubleClickEvent(QMouseEvent *event) override {
		if (event->button() == Qt::LeftButton) {
			arcballRotation = QQuaternion();
			zoom = 5.0f;
			update();
    }
	}

private:
  void setupMaterial() {
		// Цвет модели — приятный "CAD-синий" (как в КОМПАС-3D)
		GLfloat matAmbient[]  = { 0.15f, 0.35f, 0.65f, 1.0f };
		GLfloat matDiffuse[]  = { 0.35f, 0.60f, 0.90f, 1.0f };
		GLfloat matSpecular[] = { 0.70f, 0.70f, 0.70f, 1.0f };
		GLfloat matEmission[] = { 0.00f, 0.00f, 0.00f, 1.0f };
		GLfloat matShininess  = 40.0f;

		glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT,  matAmbient);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE,  matDiffuse);
		glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, matSpecular);
		glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, matEmission);
		glMaterialf (GL_FRONT_AND_BACK, GL_SHININESS, matShininess);
	}


  // Проекция 2D-координат мыши на единичную сферу
	QVector3D mapToSphere(const QPoint& p) {
		// Нормируем координаты в диапазон [-1, 1]
		float x = (2.0f * p.x() - width())  / width();
		float y = (height() - 2.0f * p.y()) / height();  // Y инвертирован
		float r2 = x * x + y * y;
		float z;

		if (r2 <= 0.5f) {
			z = sqrtf(1.0f - r2);  // внутри сферы — обычная проекция
		} else {
			z = 0.5f / sqrtf(r2);  // снаружи — гиперболический лист (чтобы избежать разрывов)
		}
		return QVector3D(x, y, z).normalized();
	}


	// Применение дельты углов Эйлера к кватерниону (для клавиатуры)
	void applyEulerDelta(float dx, float dy, float dz) {
		QQuaternion qx = QQuaternion::fromAxisAndAngle(1, 0, 0, dx);
		QQuaternion qy = QQuaternion::fromAxisAndAngle(0, 1, 0, dy);
		QQuaternion qz = QQuaternion::fromAxisAndAngle(0, 0, 1, dz);
		arcballRotation = qx * qy * qz * arcballRotation;
	}


	void drawModel() {
		if (vertices.empty()) 
		  return;
		// Поиск min/max Y для градиента
		float minY = vertices[1], maxY = vertices[1];
		for (size_t i = 1; i < vertices.size(); i += 3) {
			if (vertices[i] < minY) minY = vertices[i];
			if (vertices[i] > maxY) maxY = vertices[i];
		}
		float rangeY = maxY - minY;
		if (rangeY < 0.001f) rangeY = 1.0f;
    // Отрисовка простая
		glBegin(GL_TRIANGLES);

		// Проходим по всем треугольникам (каждый — 9 float: 3 вершины × 3 координаты)
		for (size_t i = 0; i < vertices.size(); i += 9) {
			// Вершины треугольника
			float ax = vertices[i+0], ay = vertices[i+1], az = vertices[i+2];
			float bx = vertices[i+3], by = vertices[i+4], bz = vertices[i+5];
			float cx = vertices[i+6], cy = vertices[i+7], cz = vertices[i+8];

			// Два ребра треугольника
			float e1x = bx - ax, e1y = by - ay, e1z = bz - az;
			float e2x = cx - ax, e2y = cy - ay, e2z = cz - az;

			// Нормаль = e1 × e2
			float nx = e1y * e2z - e1z * e2y;
			float ny = e1z * e2x - e1x * e2z;
			float nz = e1x * e2y - e1y * e2x;

			// Нормализация (хотя GL_NORMALIZE делает это автоматически,
			// для GL_FLAT shading лучше задать нормаль один раз)
			float len = sqrtf(nx*nx + ny*ny + nz*nz);
			if (len > 1e-6f) {
					nx /= len; ny /= len; nz /= len;
			}

			// Задаём нормаль ОДИН РАЗ на треугольник (flat shading)
			glNormal3f(nx, ny, nz);

			// Три вершины треугольника
			glVertex3f(ax, ay, az);
			glVertex3f(bx, by, bz);
			glVertex3f(cx, cy, cz);
		}

		glEnd();
	}


	// Клавиатурное управление
	void keyPressEvent(QKeyEvent *event) override {
		const float rotStep = 5.0f;
		const float zoomStep = 1.1f;

		switch (event->key()) {
		case Qt::Key_Left:  applyEulerDelta(0, -rotStep, 0); break;
		case Qt::Key_Right: applyEulerDelta(0,  rotStep, 0); break;
		case Qt::Key_Up:    applyEulerDelta(-rotStep, 0, 0); break;
		case Qt::Key_Down:  applyEulerDelta( rotStep, 0, 0); break;
		case Qt::Key_Plus:
		case Qt::Key_Equal:
			zoom *= zoomStep;
			if (zoom > 50.0f) zoom = 50.0f;
			break;
		case Qt::Key_Minus:
			zoom /= zoomStep;
			if (zoom < 0.1f) zoom = 0.1f;
			break;
		case Qt::Key_Home:
			arcballRotation = QQuaternion();  // единичный кватернион
			zoom = 5.0f;
			break;
		default:
			QOpenGLWidget::keyPressEvent(event);
			return;
		}
		update();
	}

  //-------------------------------------------------------
	std::vector<float> vertices;
	float zoom = 5.0f;
	const float baseSize = 100.0f;

	// Параметры вращения
	float rotX = 0.0f;
	float rotY = 0.0f;
	QQuaternion arcballRotation;
	QVector3D lastPointOnSphere;
	QPoint lastMousePos;

	bool isRotating = false;
	bool lightingEnabled = true;
	float modelColor[4] = { 0.35f, 0.60f, 0.90f, 1.0f };
};

//============================================================= ТОЧКА ВХОДА
int c_main(int argc, char* argv[]) {
	QSurfaceFormat fmt;
	fmt.setVersion(2, 1);
	fmt.setProfile(QSurfaceFormat::CompatibilityProfile);
	fmt.setDepthBufferSize(24);
	QSurfaceFormat::setDefaultFormat(fmt);

	QApplication app(argc, argv);

	QString stl_path = getAssetPath("insert_wall_mount_vc.stl");
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
	
	GLWidget glWidget;
	glWidget.setVertices(vertices);
	layout.addWidget(&glWidget);
	window.show();

	return app.exec(); 
}