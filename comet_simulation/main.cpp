#include <QApplication>
#include <QLabel>
#include <QPainter>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <cmath>

// Таймер і всі обчислення моделі працюють у дочірньому потоці.
class Engine : public QObject {
    Q_OBJECT
public slots:
    void start() {
        auto *timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &Engine::step);
        timer->start(30);
    }
signals:
    void positionChanged(double x, double y, double z);
private:
    void step() {
        constexpr double pi = 3.141592653589793;
        constexpr double eccentricity = 0.75;
        constexpr double a = 150.0;
        // Середня аномалія; розв'язуємо E - e sin(E) = M методом Ньютона.
        meanAnomaly = std::fmod(meanAnomaly + 0.012, 2 * pi);
        double E = meanAnomaly;
        for (int i = 0; i < 15; ++i)
            E -= (E - eccentricity * std::sin(E) - meanAnomaly) / (1 - eccentricity * std::cos(E));
        double x = a * (std::cos(E) - eccentricity);
        double y = a * std::sqrt(1 - eccentricity * eccentricity) * std::sin(E);
        // Поворот орбіти на аргумент перицентра, нахил та довготу вузла.
        const double w = 0.5, inclination = 0.6, node = 0.4;
        double x1 = x * std::cos(w) - y * std::sin(w);
        double y1 = x * std::sin(w) + y * std::cos(w);
        double y2 = y1 * std::cos(inclination);
        double z2 = y1 * std::sin(inclination);
        emit positionChanged(x1 * std::cos(node) - y2 * std::sin(node),
                             x1 * std::sin(node) + y2 * std::cos(node), z2);
    }
    double meanAnomaly = 0;
};

class OrbitView : public QWidget {
public:
    void setPosition(double newX, double newY) { x = newX; y = newY; update(); }
protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor(15, 20, 40));
        QPointF sun(width() / 2.0, height() / 2.0);
        painter.setPen(Qt::NoPen);
        painter.setBrush(Qt::yellow);
        painter.drawEllipse(sun, 9, 9);
        painter.setBrush(Qt::cyan);
        painter.drawEllipse(sun + QPointF(x, -y), 5, 5);
    }
private:
    double x = 0, y = 0;
};

int main(int argc, char *argv[]) {
    QApplication app(argc, argv);
    QWidget window;
    window.setWindowTitle("Рух комети");
    auto *layout = new QVBoxLayout(&window);
    auto *view = new OrbitView;
    auto *coordinates = new QLabel("Координати комети");
    layout->addWidget(view, 1);
    layout->addWidget(coordinates);
    window.resize(650, 500);

    QThread thread;
    auto *engine = new Engine;
    engine->moveToThread(&thread);
    QObject::connect(&thread, &QThread::started, engine, &Engine::start);
    QObject::connect(engine, &Engine::positionChanged, &window, [=](double x, double y, double z) {
        view->setPosition(x, y);
        coordinates->setText(QString("x: %1   y: %2   z: %3").arg(x, 0, 'f', 1).arg(y, 0, 'f', 1).arg(z, 0, 'f', 1));
    });
    QObject::connect(&thread, &QThread::finished, engine, &QObject::deleteLater);
    thread.start();
    window.show();
    int result = app.exec();
    thread.quit();
    thread.wait();
    return result;
}

#include "main.moc"
