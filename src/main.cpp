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
#include <QDebug>

struct Point { 
	double x {-1.0};
	double y {-1.0}; 
	double z {-1.0}; 
	// bool is_set {false};
};

bool point_set_mode = false;  // иструмент ТОЧКА

// ..передача событий в лямбду
class EventRouter : public QObject {
	Q_OBJECT
public:
	std::function<bool(QEvent*)> handler;
	bool eventFilter(QObject* obj, QEvent* event) override {
		return handler ? handler(event) : false;
	}
};

//=========================================================
int main(int argc, char* argv[]) 
{
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
	const int DIVIDER_Y {window_y_max / 2};  // y-координата горизонтального разделителя
	constexpr double EPS {5.0};  // 1e-9 // допуск по X для привязки к точке, пиксели
	double activeX {-1.0};  // текущая активная абсцисса для линии связи
	double activeY {-1.0};

	std::vector<Point> points;  // массив точек (верхняя зона)
	std::vector<Point> points_2;

	auto redraw = [&](double y = 0.0) 
	{
		QPainter p(&pixmap);
		p.setRenderHint(QPainter::Antialiasing);
		p.fillRect(pixmap.rect(), Qt::white);
		
		// -- Ось проекций --
		p.setPen(QPen(Qt::black, 2));
		p.drawLine(0, DIVIDER_Y, pixmap.width(), DIVIDER_Y);
		p.drawText(10, DIVIDER_Y - 10, "П2 (фронт)");
		p.drawText(10, DIVIDER_Y + 20, "П1 (план)");
		
		// -- Отрисовка точек на видах --
		for (const auto& pt : points) 
		{
			p.setPen(Qt::NoPen);
			p.setBrush(QBrush(Qt::blue));
			p.drawEllipse(QPointF(pt.x, pt.y), 4, 4);

			if (pt.z != -1.0)
			{
				p.setBrush(QBrush(Qt::green));
        p.drawEllipse(QPointF(pt.x, pt.z), 4, 4);
				
				p.setPen(QPen(Qt::green, 2, Qt::DashLine));
				p.drawLine(QPointF(pt.x, DIVIDER_Y), QPointF(pt.x, pt.z));

				p.setPen(QPen(Qt::blue, 2, Qt::DashLine));
				p.drawLine(QPointF(pt.x, pt.y), QPointF(pt.x, DIVIDER_Y));
			}
		}

		// -- режим установки точки  ПРИВЯЗКА выравнивание
		p.setPen(Qt::NoPen);
		if (activeX >= 0) 
		{
			p.setPen(Qt::black);
      p.drawEllipse(QPointF(activeX, y), 3, 3);
			p.setPen(Qt::NoPen);
		}

		panel->setPixmap(pixmap);
	};

	redraw();  // отрисовка
	qDebug() << "Укажите проекцию на П2 (вид спереди)";
	
	// -------------------------------------- СОБЫТИЯ
	EventRouter router;

	router.handler = [&](QEvent* e) 
	{
		// -- Установка и сохранение точек -- 
		if (e->type() == QEvent::MouseButtonPress) 
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(e);
			
			if (me->button() == Qt::LeftButton)
			{
				double mause_x = static_cast<double>(me->x());
				double mause_y = static_cast<double>(me->y());

				if (me->y() <= DIVIDER_Y  &&  !point_set_mode)  // вид спереди
				{
					points.push_back({mause_x, mause_y});  
					point_set_mode = true;
          qDebug() << "Укажите проекцию на П1 (вид сверху)";
					redraw(mause_y);
					return true;
				}
				
				if ( me->y() > DIVIDER_Y  &&  point_set_mode) // П1, вид сверху
				{
				  auto it = std::find_if(points.begin(), points.end(), 
					  [&](const Point& p) {return  p.x == activeX && p.y == activeY;});

					if (it != points.end())
					{
						it->z = mause_y;
						
						// qDebug() << "== задана точка";
						
						point_set_mode = false;
						redraw(mause_y);

            qDebug() << "Укажите проекцию на П2 (вид спереди)";

						activeX = activeY = -1;
						return true;
					} 
				}
			}
		}

		// -- Наведение на установленную точку --
		else if (e->type() == QEvent::MouseMove) 
		{
			QMouseEvent* me = static_cast<QMouseEvent*>(e);
			QPointF cursorPos = me->pos();

			if (point_set_mode)
			{
        Point last_point =  points[points.size() - 1];   // ПОЧЕМУ c итератором не работает??
			
				if (
					 std::abs(cursorPos.x() - last_point.x) <= EPS  && 
					cursorPos.y() >= DIVIDER_Y
				)
				{
					activeX = last_point.x;
			 		activeY = last_point.y;
				}
				else
				{
					activeX = -1.0;
				}
			}
			
			redraw(cursorPos.y());
			return false;
		}

		// -- Выход за пределы панели -- 
		else if (e->type() == QEvent::Leave) 
		{
			activeX = activeY = -1.0;
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