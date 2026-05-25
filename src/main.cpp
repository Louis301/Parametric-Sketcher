#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>
#include <QToolTip>
#include <QVBoxLayout>
#include <QPixmap>
#include <vector>
#include <cmath>
#include <QString>

struct Point {
	double x; double y; };

// ..передача событий в лямбду
class EventRouter : public QObject {
	Q_OBJECT
public:
	std::function<bool(QEvent*)> handler;
	bool eventFilter(QObject* obj, QEvent* event) override {
		return handler ? handler(event) : false;
	}
};

// =======================================================
int main(int argc, char* argv[]) {
	QApplication app(argc, argv);

	// -- Иниц-ция диалогового окна --
	QWidget window;
	window.resize(400, 400);
	window.setWindowTitle("Canvas");

  // -- Панель холста --
	QVBoxLayout* layout = new QVBoxLayout(&window);
	QLabel* panel = new QLabel(&window);
	panel->setMinimumSize(400, 400);
	panel->setMaximumSize(400, 400);
	panel->setStyleSheet("background-color: white; border: 1px solid #ff6060;");
  panel->setMouseTracking(true);  // ⚠️ Важно: отслеживание движения мыши без нажатия
	layout->addWidget(panel);

	// -- Холст --
	QPixmap pixmap(panel->size());
	pixmap.fill(Qt::white);
	panel->setPixmap(pixmap);

	// -------------------------------------- СОБЫТИЯ
	std::vector<Point> points;
	EventRouter router;
	const double HOVER_RADIUS = 5.0;

	router.handler = [&](QEvent* e) 
	{
		// -- Установка и сохранение точек -- 
		if (e->type() == QEvent::MouseButtonPress) 
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(e);
			
			if (me->button() == Qt::LeftButton)  
			{
				double x = static_cast<double>(me->x());
				double y = static_cast<double>(me->y());

				points.push_back({x, y});  // локализация точек
				// qDebug() << "[SAVE] Point #" << points.size() << " | x:" << points.back().x << " y:" << points.back().y;

				// -- Отрисовка точек -- 
				QPainter p(&pixmap);
				p.setRenderHint(QPainter::Antialiasing);
				p.setPen(Qt::NoPen);
				p.setBrush(QBrush(Qt::blue));
				p.drawEllipse(QPointF(x, y), 5, 5);
				panel->setPixmap(pixmap);
				
				return true;
			}
		}
		// -- Наведение на установленную точку --
		else if (e->type() == QEvent::MouseMove) {
			QMouseEvent* me = static_cast<QMouseEvent*>(e);
			QPointF cursorPos = me->pos();
				
			for (const auto& pt : points) {
				double dx = cursorPos.x() - pt.x;
				double dy = cursorPos.y() - pt.y;
				double dist = std::sqrt(dx*dx + dy*dy);
				
				if (dist <= HOVER_RADIUS) {
					QString hint = QString("Point: (%1, %2)")  // отрисовка хинта
						.arg(pt.x, 0, 'f', 1)
						.arg(pt.y, 0, 'f', 1);
					QToolTip::showText(me->globalPos(), hint, panel);
					return true;
				}
			}
			QToolTip::hideText();  // ..точка не найдена
			return false;
		}
		// -- Выход за пределы панели -- 
		else if (e->type() == QEvent::Leave) {
			QToolTip::hideText();
			return false;
		}

		return false;
	};

	// -- Запуск приложения --
	panel->installEventFilter(&router);  // инициализация обработчика событий
	window.show();  // вывод GUI
	return app.exec();  // обработка действий пользователя
}

// ..moc для EventRouter
#include "main.moc"