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
  const int window_x_max {400};
	const int window_y_max {800};

	QWidget window;
	window.resize(window_x_max, window_y_max);
	window.setWindowTitle("Canvas");

  // -- Панель холста --
	QVBoxLayout* layout = new QVBoxLayout(&window);
	QLabel* panel = new QLabel(&window);
	panel->setMinimumSize(window_x_max, window_y_max);
	panel->setMaximumSize(window_x_max, window_y_max);
	panel->setStyleSheet("background-color: white; border: 1px solid #ff6060;");
  panel->setMouseTracking(true);
	layout->addWidget(panel);

	// -- Холст --
	QPixmap pixmap(panel->size());
	pixmap.fill(Qt::white);
	panel->setPixmap(pixmap);

	// -------------------------------------- ЭПЮР
	const int DIVIDER_Y = window_y_max / 2;  // y-координата горизонтального разделителя
	const double HOVER_TOLERANCE = 10.0;  // допуск по X для привязки к точке, пиксели
	std::vector<Point> points;  // массив точек (план: верхняя зона)
	double activeX = -1.0;  // текущая активная абсцисса для линии связи

	auto redraw = [&](double y=0.0) 
	{
		QPainter p(&pixmap);
		p.setRenderHint(QPainter::Antialiasing);
		p.fillRect(pixmap.rect(), Qt::white);
		// -- Ось проекций --
		p.setPen(QPen(Qt::black, 2));
		p.drawLine(0, DIVIDER_Y, pixmap.width(), DIVIDER_Y);
		p.drawText(10, DIVIDER_Y - 10, "П2 (фронт)");
		p.drawText(10, DIVIDER_Y + 20, "П1 (план)");
		// -- Точки на "плане" --
		p.setPen(Qt::NoPen);
		p.setBrush(QBrush(Qt::blue));
		for (const auto& pt : points) {
			p.drawEllipse(QPointF(pt.x, pt.y), 4, 4);
		}		
		// -- Вертикальная направляющая (область фронтали) --
		if (activeX >= 0 && y > DIVIDER_Y) {
			p.setPen(QPen(Qt::gray, 2, Qt::DashLine));
			p.drawLine(activeX, 0, activeX, pixmap.height());
			p.setPen(Qt::black);
		}
		panel->setPixmap(pixmap);
	};

	redraw();  // отрисовка
	
	// -------------------------------------- СОБЫТИЯ
	EventRouter router;
	const double HOVER_RADIUS = 5.0;

	router.handler = [&](QEvent* e) 
	{
		// -- Установка и сохранение точек -- 
		if (e->type() == QEvent::MouseButtonPress) 
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(e);
			
			if (me->button() == Qt::LeftButton && me->y() <= DIVIDER_Y)  
			{
				double x = static_cast<double>(me->x());
				double y = static_cast<double>(me->y());
				points.push_back({x, y});  // локализация точек
				redraw(y);
				return true;
			}
		}
		// -- Наведение на установленную точку --
		else if (e->type() == QEvent::MouseMove) {
			QMouseEvent* me = static_cast<QMouseEvent*>(e);
			QPointF cursorPos = me->pos();
      bool found = false;
			for (const auto& pt : points) 
			{
				double dx = cursorPos.x() - pt.x;
				double dy = cursorPos.y() - pt.y;
				double dist = std::sqrt(dx*dx + dy*dy);
				bool found = false;
				for (const auto& pt : points) {
					if (std::abs(cursorPos.x() - pt.x) <= HOVER_TOLERANCE) {
						activeX = pt.x;
						found = true;
						break;
					}
				}
				if (!found) {
					activeX = -1.0;
				}
				redraw(cursorPos.y());
			}
			return false;
		}
		// -- Выход за пределы панели -- 
		else if (e->type() == QEvent::Leave) {
			activeX = -1.0;
      redraw();
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