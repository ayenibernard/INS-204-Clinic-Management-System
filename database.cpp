#include "database.h"
#include <QSqlError>
#include <QUuid>
#include <QDate>
#include <stdexcept>

static void failure(const QString& text) {
    throw std::runtime_error(text.toStdString());
}
Database::Database(const QString& path) {
    connection = QSqlDatabase::addDatabase("QSQLITE", QUuid::createUuid().toString());
    connection.setDatabaseName(path);
    if (!connection.open()) failure(connection.lastError().text());
    query("PRAGMA foreign_keys=ON");
    query("PRAGMA busy_timeout=5000");
    query("CREATE TABLE IF NOT EXISTS patient (id INTEGER PRIMARY KEY, name TEXT NOT NULL, dob TEXT NOT NULL, phone TEXT NOT NULL, address TEXT NOT NULL)");
    query("CREATE TABLE IF NOT EXISTS appointment (id INTEGER PRIMARY KEY, patient_id INTEGER NOT NULL REFERENCES patient(id), doctor TEXT NOT NULL, scheduled TEXT NOT NULL, status TEXT NOT NULL DEFAULT 'Booked' CHECK(status IN ('Booked','Completed','Cancelled')))");
    query("CREATE UNIQUE INDEX IF NOT EXISTS doctor_slot ON appointment(doctor,scheduled) WHERE status='Booked'");
    query("CREATE TABLE IF NOT EXISTS visit (id INTEGER PRIMARY KEY, appointment_id INTEGER NOT NULL UNIQUE REFERENCES appointment(id), visited TEXT NOT NULL, diagnosis TEXT NOT NULL, notes TEXT NOT NULL)");
    query("CREATE TABLE IF NOT EXISTS prescription (id INTEGER PRIMARY KEY, visit_id INTEGER NOT NULL REFERENCES visit(id), medicine TEXT NOT NULL, dosage TEXT NOT NULL, directions TEXT NOT NULL)");
    query("CREATE TABLE IF NOT EXISTS bill (id INTEGER PRIMARY KEY, visit_id INTEGER NOT NULL UNIQUE REFERENCES visit(id), issued TEXT NOT NULL)");
    query("CREATE TABLE IF NOT EXISTS bill_item (id INTEGER PRIMARY KEY, bill_id INTEGER NOT NULL REFERENCES bill(id), description TEXT NOT NULL, quantity INTEGER NOT NULL CHECK(quantity>0), unit_kobo INTEGER NOT NULL CHECK(unit_kobo>0))");
    query("CREATE TABLE IF NOT EXISTS payment (id INTEGER PRIMARY KEY, bill_id INTEGER NOT NULL REFERENCES bill(id), paid_at TEXT NOT NULL, kobo INTEGER NOT NULL CHECK(kobo>0), method TEXT NOT NULL)");
}
Database::~Database() {
    const auto name = connection.connectionName();
    connection.close();
    connection = QSqlDatabase();
    QSqlDatabase::removeDatabase(name);
}
QSqlQuery Database::query(const QString& sql, const QVariantList& values) const {
    QSqlQuery result(connection);
    if (!result.prepare(sql)) failure(result.lastError().text());
    for (const auto& value : values) result.addBindValue(value);
    if (!result.exec()) {
        const auto error = result.lastError().text();
        if (error.contains("appointment.doctor") && error.contains("appointment.scheduled"))
            failure("This doctor already has a booked appointment at that time.");
        failure(error);
    }
    return result;
}
qint64 Database::insert(const QString& sql, const QVariantList& values) const {
    return query(sql, values).lastInsertId().toLongLong();
}
QString Database::path() const { return connection.databaseName(); }
void Database::visit(qint64 appointment, const QString& diagnosis, const QString& notes) const {
    auto db = connection;
    if (!db.transaction()) failure(db.lastError().text());
    try {
        auto result = query("UPDATE appointment SET status='Completed' WHERE id=? AND status='Booked'", {appointment});
        if (result.numRowsAffected() != 1) failure("Select an open appointment.");
        insert("INSERT INTO visit(appointment_id,visited,diagnosis,notes) VALUES(?,?,?,?)", {appointment, QDate::currentDate().toString(Qt::ISODate), diagnosis, notes});
        if (!db.commit()) failure(db.lastError().text());
    } catch (...) { db.rollback(); throw; }
}
void Database::payment(qint64 bill, qint64 kobo, const QString& method) const {
    // Acquire SQLite's write lock before reading the balance.
    query("BEGIN IMMEDIATE");
    try {
        auto result = query("SELECT COALESCE((SELECT SUM(quantity*unit_kobo) FROM bill_item WHERE bill_id=b.id),0)-COALESCE((SELECT SUM(kobo) FROM payment WHERE bill_id=b.id),0) FROM bill b WHERE id=?", {bill});
        if (!result.next() || kobo <= 0 || kobo > result.value(0).toLongLong()) failure("Payment exceeds the outstanding balance.");
        insert("INSERT INTO payment(bill_id,paid_at,kobo,method) VALUES(?,?,?,?)", {bill,QDate::currentDate().toString(Qt::ISODate),kobo,method});
        query("COMMIT");
    } catch (...) { query("ROLLBACK"); throw; }
}
