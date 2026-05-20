#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>


#include <iostream>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    auto db = QSqlDatabase::addDatabase("QPSQL");
    db.setHostName("127.0.0.1");
    db.setDatabaseName("cad_test_db");
    db.setUserName("louis301");
    db.setPassword("123");

    if (!db.open()) {
        qCritical() << "DB error:" << db.lastError().text();
        return -1;
    }

    qDebug() << "✓ Connected to PostgreSQL";
    qDebug() << "✓ Qt version:" << QT_VERSION_STR;
    std::cout << "0101010\n";

    return 0;
}
