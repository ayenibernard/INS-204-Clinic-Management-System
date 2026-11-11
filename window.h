#pragma once
#include "database.h"
#include <QMainWindow>
#include <QTableWidget>
#include <functional>
#include <array>
class QLabel;
class QDateEdit;
class QProgressBar;
class AppointmentChart;

class Window : public QMainWindow {
public:
    explicit Window(Database& database);
    void refresh();
private:
    Database& db;
    QTableWidget *patients, *appointments, *visits, *prescriptions, *bills, *payments;
    QLabel* hospitalTitle;
    std::array<QLabel*, 4> metricValues;
    QTableWidget* upcoming;
    std::array<QProgressBar*, 4> ageBars;
    AppointmentChart* appointmentChart;
    QLabel* dailyActivity;
    void refreshDashboardAnalytics();
    void setHospitalName(const QString& name);
    void editHospitalName();
    QLabel* reportSummary;
    QDateEdit *from, *to;
    QTableWidget *appointmentReport, *visitReport, *paymentReport;
    QTableWidget* table(QWidget* page);
    void fill(QTableWidget* target, const QStringList& headers, const QString& sql, const QVariantList& values = {});
    void guarded(const std::function<void()>& action);
    qint64 selected(QTableWidget* target);
    void patientDialog(bool edit);
    void appointmentDialog(bool edit);
    void visitDialog();
    void prescriptionDialog();
    void billDialog();
    void paymentDialog();
    void receipt();
    void reports();
};
