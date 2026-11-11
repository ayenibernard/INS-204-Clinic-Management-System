#pragma once
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QVariant>
#include <QString>

class Database {
public:
    explicit Database(const QString& path);
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;
    QSqlQuery query(const QString& sql, const QVariantList& values = {}) const;
    qint64 insert(const QString& sql, const QVariantList& values) const;
    void visit(qint64 appointment, const QString& diagnosis, const QString& notes) const;
    void payment(qint64 bill, qint64 kobo, const QString& method) const;
    QString path() const;
private:
    QSqlDatabase connection;
};
