#include "window.h"
#include <QtWidgets>
#include <QPrinter>
#include <QSettings>
#include <QPainter>
#include <algorithm>
#include <QPrintDialog>
#include <cmath>
#include <stdexcept>

namespace {
QString money(qint64 kobo) { return QString("NGN %1").arg(kobo / 100.0, 0, 'f', 2); }
QString required(QLineEdit* field) {
    const auto text = field->text().trimmed();
    if (text.isEmpty()) throw std::runtime_error("Please complete every field.");
    return text;
}
QLineEdit* input(QFormLayout* layout, const QString& label, const QString& value = {}) {
    auto* field = new QLineEdit(value);
    field->setMaxLength(250);
    layout->addRow(label, field);
    return field;
}
void buttons(QDialog& dialog, QFormLayout* layout) {
    auto* box = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
    layout->addRow(box);
    QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
}
QComboBox* choices(QFormLayout* layout, const QString& label, QSqlQuery query) {
    auto* combo = new QComboBox;
    combo->addItem("Select…", QVariant());
    while (query.next()) combo->addItem(query.value(1).toString(), query.value(0));
    layout->addRow(label, combo);
    return combo;
}
qint64 choice(QComboBox* combo) {
    if (!combo->currentData().isValid()) throw std::runtime_error("Please select a record.");
    return combo->currentData().toLongLong();
}
}
class AppointmentChart : public QWidget {
public:
    AppointmentChart() { setMinimumSize(235, 110); }
    void setCounts(const std::array<int, 3>& counts) {
        values = counts;
        setAccessibleName(QString("Appointments: %1 booked, %2 completed, %3 cancelled")
            .arg(values[0]).arg(values[1]).arg(values[2]));
        update();
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QRect ring(14, (height() - 76) / 2, 76, 76);
        const int total = values[0] + values[1] + values[2];
        const QStringList labels{"Booked", "Completed", "Cancelled"};
        const std::array<QColor, 3> colors{QColor("#67a8ff"), QColor("#60d6bc"), QColor("#eaaa65")};
        painter.setPen(QPen(QColor("#354e69"), 12));
        painter.drawEllipse(ring);
        int start = 90 * 16;
        int seen = 0;
        for (int i = 0; i < 3; ++i) {
            seen += values[i];
            const int end = total ? 90 * 16 - qRound(5760.0 * seen / total) : start;
            painter.setPen(QPen(colors[i], 12));
            if (end != start) painter.drawArc(ring, start, end - start);
            start = end;
            const int y = height() / 2 - 24 + 24 * i;
            painter.fillRect(112, y - 5, 9, 9, colors[i]);
            painter.setPen(QColor("#e6effb"));
            painter.drawText(QRect(130, y - 12, width() - 136, 24),
                             Qt::AlignVCenter, labels[i] + "  " + QString::number(values[i]));
        }
        painter.drawText(ring, Qt::AlignCenter, QString::number(total));
    }
private:
    std::array<int, 3> values{};
};

QTableWidget* Window::table(QWidget* page) {
    auto* view = new QTableWidget;
    view->setSelectionBehavior(QAbstractItemView::SelectRows);
    view->setSelectionMode(QAbstractItemView::SingleSelection);
    view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    view->horizontalHeader()->setStretchLastSection(true);
    view->verticalHeader()->hide();
    view->setAlternatingRowColors(true);
    static_cast<QVBoxLayout*>(page->layout())->addWidget(view);
    return view;
}
void Window::fill(QTableWidget* target, const QStringList& headers, const QString& sql, const QVariantList& values) {
    auto result = db.query(sql, values);
    target->setSortingEnabled(false);
    target->setRowCount(0);
    target->setColumnCount(headers.size());
    target->setHorizontalHeaderLabels(headers);
    while (result.next()) {
        const int row = target->rowCount();
        target->insertRow(row);
        for (int col = 0; col < headers.size(); ++col)
            target->setItem(row, col, new QTableWidgetItem(result.value(col).toString()));
    }
    target->resizeColumnsToContents();
}
void Window::guarded(const std::function<void()>& action) {
    try { action(); }
    catch (const std::exception& error) { QMessageBox::warning(this, "Clinic", QString::fromUtf8(error.what())); }
}
qint64 Window::selected(QTableWidget* target) {
    if (target->currentRow() < 0) throw std::runtime_error("Select a row in the table first.");
    return target->item(target->currentRow(), 0)->text().toLongLong();
}
Window::Window(Database& database) : db(database) {
    resize(1050, 720);
    auto* central = new QWidget;
    setCentralWidget(central);
    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(24, 16, 24, 16);
    root->setSpacing(14);

    auto* header = new QHBoxLayout;
    header->setSpacing(14);
    auto* logo = new QLabel;
    logo->setPixmap(windowIcon().pixmap(QSize(58, 58), devicePixelRatioF()));
    logo->setAccessibleName("Harmony General Hospital Heartbeat H logo");
    header->addWidget(logo, 0, Qt::AlignVCenter);
    auto* branding = new QVBoxLayout;
    branding->setSpacing(3);
    hospitalTitle = new QLabel;
    hospitalTitle->setObjectName("title");
    hospitalTitle->setTextFormat(Qt::PlainText);
    hospitalTitle->setWordWrap(true);
    branding->addWidget(hospitalTitle);
    auto* subtitle = new QLabel("Clinic Management System");
    subtitle->setObjectName("subtitle");
    branding->addWidget(subtitle);
    header->addLayout(branding, 1);
    auto* settings = new QPushButton("Hospital settings");
    settings->setObjectName("secondaryAction");
    header->addWidget(settings, 0, Qt::AlignTop);
    connect(settings, &QPushButton::clicked, this, [this] { guarded([this] { editHospitalName(); }); });
    root->addLayout(header);
    setHospitalName(QSettings().value("branding/hospitalName", "Harmony General Hospital").toString());
    auto* workspace = new QHBoxLayout;
    workspace->setSpacing(16);
    root->addLayout(workspace, 1);
    auto* navigation = new QListWidget;
    navigation->setObjectName("navigation");
    navigation->setFixedWidth(166);
    navigation->setSpacing(3);
    navigation->setAccessibleName("Clinic sections");
    workspace->addWidget(navigation);
    auto* pages = new QStackedWidget;
    workspace->addWidget(pages, 1);
    connect(navigation, &QListWidget::currentRowChanged, pages, &QStackedWidget::setCurrentIndex);
    auto page = [&](const QString& name) {
        auto* widget = new QWidget;
        new QVBoxLayout(widget);
        pages->addWidget(widget);
        navigation->addItem(name);
        return widget;
    };
    auto action = [&](QWidget* parent, const QString& name, std::function<void()> callback) {
        auto* button = new QPushButton(name);
        static_cast<QVBoxLayout*>(parent->layout())->addWidget(button);
        connect(button, &QPushButton::clicked, this, [this, callback] { guarded(callback); });
    };
    auto* dashboard = page("Dashboard");
    auto* overview = static_cast<QVBoxLayout*>(dashboard->layout());
    overview->setContentsMargins(0, 0, 0, 0);
    overview->setSpacing(12);
    auto* metrics = new QGridLayout;
    metrics->setSpacing(12);
    const QStringList labels{"Patients", "Booked appointments", "Completed visits", "Payments received"};
    for (int index = 0; index < labels.size(); ++index) {
        auto* card = new QFrame;
        card->setObjectName("metricCard");
        auto* layout = new QVBoxLayout(card);
        layout->setContentsMargins(16, 11, 16, 11);
        layout->setSpacing(4);
        auto* label = new QLabel(labels[index]);
        label->setObjectName("metricLabel");
        layout->addWidget(label);
        metricValues[index] = new QLabel;
        metricValues[index]->setObjectName("metricValue");
        layout->addWidget(metricValues[index]);
        metrics->addWidget(card, 0, index);
        metrics->setColumnStretch(index, 1);
    }
    overview->addLayout(metrics);
    auto* analytics = new QHBoxLayout;
    analytics->setSpacing(12);
    overview->addLayout(analytics);
    auto panel = [&](const QString& title) {
        auto* frame = new QFrame;
        frame->setObjectName("metricCard");
        auto* layout = new QVBoxLayout(frame);
        layout->setContentsMargins(14, 10, 14, 10);
        auto* heading = new QLabel(title);
        heading->setObjectName("sectionTitle");
        layout->addWidget(heading);
        analytics->addWidget(frame, 1);
        return layout;
    };
    auto* ages = panel("Patient age distribution");
    const QStringList ranges{"0–17", "18–35", "36–55", "56+"};
    for (int i = 0; i < ranges.size(); ++i) {
        auto* row = new QHBoxLayout;
        auto* label = new QLabel(ranges[i]);
        label->setObjectName("metricLabel");
        label->setMinimumWidth(52);
        row->addWidget(label);
        ageBars[i] = new QProgressBar;
        ageBars[i]->setAccessibleName(ranges[i] + " years");
        row->addWidget(ageBars[i], 1);
        ages->addLayout(row);
    }
    auto* statuses = panel("Appointment status");
    appointmentChart = new AppointmentChart;
    statuses->addWidget(appointmentChart, 1);
    dailyActivity = new QLabel;
    dailyActivity->setObjectName("subtitle");
    dailyActivity->setWordWrap(true);
    overview->addWidget(dailyActivity);
    auto* agenda = new QLabel("Upcoming appointments");
    agenda->setObjectName("sectionTitle");
    overview->addWidget(agenda);
    upcoming = table(dashboard);
    auto* dashboardActions = new QHBoxLayout;
    for (const auto& name : {QString("Refresh dashboard"), QString("Show database location")}) {
        auto* button = new QPushButton(name);
        button->setObjectName("secondaryAction");
        dashboardActions->addWidget(button);
        if (name == "Refresh dashboard")
            connect(button, &QPushButton::clicked, this, [this] { guarded([this] { refresh(); }); });
        else
            connect(button, &QPushButton::clicked, this, [this] {
                QMessageBox::information(this, "Data location", db.path());
            });
    }
    dashboardActions->addStretch();
    overview->addLayout(dashboardActions);

    auto* patientPage = page("Patients");
    auto* search = new QLineEdit;
    search->setPlaceholderText("Search by patient name or phone");
    patientPage->layout()->addWidget(search);
    patients = table(patientPage);
    connect(search, &QLineEdit::textChanged, this, [this](const QString& value) {
        guarded([&] { fill(patients, {"ID","Name","Date of birth","Phone","Address"},
            "SELECT * FROM patient WHERE name LIKE ? OR phone LIKE ? ORDER BY name",
            {"%" + value + "%", "%" + value + "%"}); });
    });
    action(patientPage, "Register patient", [this] { patientDialog(false); });
    action(patientPage, "Edit selected patient", [this] { patientDialog(true); });

    auto* appointmentPage = page("Appointments");
    appointments = table(appointmentPage);
    action(appointmentPage, "Book appointment", [this] { appointmentDialog(false); });
    action(appointmentPage, "Reschedule selected appointment", [this] { appointmentDialog(true); });
    action(appointmentPage, "Cancel selected appointment", [this] {
        const auto id = selected(appointments);
        if (QMessageBox::question(this,"Cancel appointment","Cancel the selected appointment?") != QMessageBox::Yes) return;
        const auto result = db.query("UPDATE appointment SET status='Cancelled' WHERE id=? AND status='Booked'", {id});
        if (result.numRowsAffected() != 1) throw std::runtime_error("Only booked appointments can be cancelled.");
        refresh();
    });

    auto* clinicalPage = page("Clinical records");
    clinicalPage->layout()->addWidget(new QLabel("Visits"));
    visits = table(clinicalPage);
    action(clinicalPage, "Record visit from appointment", [this] { visitDialog(); });
    clinicalPage->layout()->addWidget(new QLabel("Prescriptions"));
    prescriptions = table(clinicalPage);
    action(clinicalPage, "Issue prescription", [this] { prescriptionDialog(); });

    auto* billingPage = page("Billing");
    billingPage->layout()->addWidget(new QLabel("Bills and outstanding balances"));
    bills = table(billingPage);
    billingPage->layout()->addWidget(new QLabel("Service items for the selected bill"));
    auto* billItems = table(billingPage);
    connect(bills, &QTableWidget::currentCellChanged, this,
        [this, billItems](int row, int, int, int) {
            guarded([&] {
                if (row < 0 || !bills->item(row, 0)) { billItems->setRowCount(0); return; }
                fill(billItems, {"Service", "Quantity", "Unit NGN", "Total NGN"},
                    "SELECT description,quantity,printf('%.2f',unit_kobo/100.0),printf('%.2f',quantity*unit_kobo/100.0) FROM bill_item WHERE bill_id=?",
                    {bills->item(row, 0)->text().toLongLong()});
            });
        });
    action(billingPage, "Create bill or add service to selected bill", [this] { billDialog(); });
    action(billingPage, "Record payment for selected bill", [this] { paymentDialog(); });
    billingPage->layout()->addWidget(new QLabel("Payment receipts"));
    payments = table(billingPage);
    action(billingPage, "Print selected payment receipt", [this] { receipt(); });

    auto* reportPage = page("Reports");
    auto* dates = new QHBoxLayout;
    from = new QDateEdit(QDate::currentDate().addMonths(-1));
    to = new QDateEdit(QDate::currentDate());
    for (auto* edit : {from, to}) { edit->setCalendarPopup(true); edit->setDisplayFormat("yyyy-MM-dd"); }
    dates->addWidget(new QLabel("From")); dates->addWidget(from);
    dates->addWidget(new QLabel("To")); dates->addWidget(to);
    static_cast<QVBoxLayout*>(reportPage->layout())->addLayout(dates);
    action(reportPage, "Generate reports", [this] { reports(); });
    reportSummary = new QLabel;
    reportPage->layout()->addWidget(reportSummary);
    reportPage->layout()->addWidget(new QLabel("Appointments"));
    appointmentReport = table(reportPage);
    reportPage->layout()->addWidget(new QLabel("Patient visits"));
    visitReport = table(reportPage);
    reportPage->layout()->addWidget(new QLabel("Revenue received"));
    paymentReport = table(reportPage);
    action(reportPage, "Export displayed reports as CSV", [this] {
        const auto path = QFileDialog::getSaveFileName(this,"Export reports","clinic_reports.csv","CSV (*.csv)");
        if (path.isEmpty()) return;
        QSaveFile file(path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) throw std::runtime_error("Cannot open export file.");
        QTextStream stream(&file);
        auto cell = [](QString text) {
            if (text.startsWith('=') || text.startsWith('+') || text.startsWith('-') || text.startsWith('@')) text.prepend(QChar(39));
            text.replace(QChar(34), QString(2, QChar(34)));
            return QString(QChar(34)) + text + QChar(34);
        };
        for (auto* view : {appointmentReport, visitReport, paymentReport}) {
            for (int col=0; col<view->columnCount(); ++col) stream << (col ? "," : "") << cell(view->horizontalHeaderItem(col)->text());
            stream << "\n";
            for (int row=0; row<view->rowCount(); ++row) {
                for (int col=0; col<view->columnCount(); ++col) stream << (col ? "," : "") << cell(view->item(row,col)->text());
                stream << "\n";
            }
            stream << "\n";
        }
        stream.flush();
        if (stream.status() != QTextStream::Ok || !file.commit()) throw std::runtime_error("Cannot save export.");
        statusBar()->showMessage("Reports exported",5000);
    });
    navigation->setCurrentRow(0);
    refresh();
}
void Window::refresh() {
    fill(patients, {"ID","Name","Date of birth","Phone","Address"}, "SELECT * FROM patient ORDER BY name");
    fill(appointments, {"ID","Patient","Doctor","Scheduled","Status"}, "SELECT a.id,p.name,a.doctor,a.scheduled,a.status FROM appointment a JOIN patient p ON p.id=a.patient_id ORDER BY a.scheduled DESC");
    fill(visits, {"ID","Patient","Doctor","Visit date","Diagnosis","Notes"}, "SELECT v.id,p.name,a.doctor,v.visited,v.diagnosis,v.notes FROM visit v JOIN appointment a ON a.id=v.appointment_id JOIN patient p ON p.id=a.patient_id ORDER BY v.id DESC");
    fill(prescriptions, {"ID","Patient","Visit","Medicine","Dosage","Directions"}, "SELECT r.id,p.name,v.id,r.medicine,r.dosage,r.directions FROM prescription r JOIN visit v ON v.id=r.visit_id JOIN appointment a ON a.id=v.appointment_id JOIN patient p ON p.id=a.patient_id ORDER BY r.id DESC");
    fill(bills, {"ID","Patient","Issued","Charge NGN","Paid NGN","Balance NGN"}, R"(
        SELECT b.id,p.name,b.issued,
        printf('%.2f',COALESCE((SELECT SUM(quantity*unit_kobo) FROM bill_item WHERE bill_id=b.id),0)/100.0),
        printf('%.2f',COALESCE((SELECT SUM(kobo) FROM payment WHERE bill_id=b.id),0)/100.0),
        printf('%.2f',(COALESCE((SELECT SUM(quantity*unit_kobo) FROM bill_item WHERE bill_id=b.id),0)-COALESCE((SELECT SUM(kobo) FROM payment WHERE bill_id=b.id),0))/100.0)
        FROM bill b JOIN visit v ON v.id=b.visit_id JOIN appointment a ON a.id=v.appointment_id JOIN patient p ON p.id=a.patient_id ORDER BY b.id DESC)");
    fill(payments, {"Receipt","Bill","Date","Amount NGN","Method"}, "SELECT id,bill_id,paid_at,printf('%.2f',kobo/100.0),method FROM payment ORDER BY id DESC");
    auto count = db.query("SELECT (SELECT COUNT(*) FROM patient),(SELECT COUNT(*) FROM appointment WHERE status='Booked'),(SELECT COUNT(*) FROM visit),(SELECT COALESCE(SUM(kobo),0) FROM payment)");
    count.next();
    for (int index = 0; index < 3; ++index)
        metricValues[index]->setText(count.value(index).toString());
    metricValues[3]->setText(money(count.value(3).toLongLong()));
    fill(upcoming, {"Scheduled", "Patient", "Doctor"},
         "SELECT a.scheduled,p.name,a.doctor FROM appointment a JOIN patient p ON p.id=a.patient_id "
         "WHERE a.status='Booked' AND a.scheduled>=? ORDER BY a.scheduled LIMIT 8",
         {QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm")});
    refreshDashboardAnalytics();
    if (from->date() <= to->date()) reports();
}
void Window::refreshDashboardAnalytics() {
    const auto today = QDate::currentDate();
    std::array<int, 4> ages{};
    auto patients = db.query("SELECT dob FROM patient");
    while (patients.next()) {
        const auto birth = QDate::fromString(patients.value(0).toString(), Qt::ISODate);
        if (!birth.isValid() || birth > today) continue;
        int age = today.year() - birth.year();
        if (today < birth.addYears(age)) --age;
        if (age <= 17) ++ages[0];
        else if (age <= 35) ++ages[1];
        else if (age <= 55) ++ages[2];
        else ++ages[3];
    }
    const int ageTotal = ages[0] + ages[1] + ages[2] + ages[3];
    for (std::size_t i = 0; i < ages.size(); ++i) {
        ageBars[i]->setRange(0, std::max(1, ageTotal));
        ageBars[i]->setValue(ages[i]);
        ageBars[i]->setFormat(QString::number(ages[i]));
    }
    auto appointments = db.query(
        "SELECT COUNT(CASE WHEN status='Booked' THEN 1 END), "
        "COUNT(CASE WHEN status='Completed' THEN 1 END), "
        "COUNT(CASE WHEN status='Cancelled' THEN 1 END), "
        "COUNT(CASE WHEN substr(scheduled,1,10)=? THEN 1 END) FROM appointment",
        {today.toString(Qt::ISODate)});
    appointments.next();
    appointmentChart->setCounts({appointments.value(0).toInt(), appointments.value(1).toInt(),
                                 appointments.value(2).toInt()});
    auto activity = db.query(
        "SELECT (SELECT COUNT(*) FROM visit WHERE visited=?), "
        "(SELECT COALESCE(SUM(kobo),0) FROM payment WHERE paid_at=?)",
        {today.toString(Qt::ISODate), today.toString(Qt::ISODate)});
    activity.next();
    dailyActivity->setText(QString("Today  ·  Scheduled: %1  ·  Visits: %2  ·  Payments: %3")
        .arg(appointments.value(3).toInt()).arg(activity.value(0).toInt())
        .arg(money(activity.value(1).toLongLong())));
}
void Window::setHospitalName(const QString& name) {
    hospitalTitle->setText(name);
    setWindowTitle(name + " · Clinic Management System");
}
void Window::editHospitalName() {
    QDialog dialog(this);
    dialog.setWindowTitle("Hospital settings");
    auto* layout = new QFormLayout(&dialog);
    auto* name = input(layout, "Hospital name", hospitalTitle->text());
    name->setMaxLength(100);
    buttons(dialog, layout);
    if (dialog.exec() != QDialog::Accepted) return;
    const auto hospitalName = required(name);
    QSettings settings;
    settings.setValue("branding/hospitalName", hospitalName);
    settings.sync();
    if (settings.status() != QSettings::NoError)
        throw std::runtime_error("Unable to save the hospital name.");
    setHospitalName(hospitalName);
}
void Window::patientDialog(bool edit) {
    qint64 id=0;
    QStringList values{"","","",""};
    if (edit) {
        id=selected(patients);
        auto result=db.query("SELECT name,dob,phone,address FROM patient WHERE id=?",{id});
        if (!result.next()) throw std::runtime_error("Patient not found.");
        for (int i=0;i<4;++i) values[i]=result.value(i).toString();
    }
    QDialog dialog(this); dialog.setWindowTitle(edit ? "Edit patient" : "Register patient");
    auto* layout=new QFormLayout(&dialog);
    auto* name=input(layout,"Full name",values[0]);
    auto* dob=new QDateEdit(edit ? QDate::fromString(values[1],Qt::ISODate) : QDate(2000,1,1));
    dob->setCalendarPopup(true); dob->setMaximumDate(QDate::currentDate()); dob->setDisplayFormat("yyyy-MM-dd"); layout->addRow("Date of birth",dob);
    auto* phone=input(layout,"Phone",values[2]);
    auto* address=input(layout,"Address",values[3]);
    buttons(dialog,layout);
    if (dialog.exec()!=QDialog::Accepted) return;
    QVariantList fields{required(name),dob->date().toString(Qt::ISODate),required(phone),required(address)};
    if (edit) { fields<<id; db.query("UPDATE patient SET name=?,dob=?,phone=?,address=? WHERE id=?",fields); }
    else db.insert("INSERT INTO patient(name,dob,phone,address) VALUES(?,?,?,?)",fields);
    refresh();
}
void Window::appointmentDialog(bool edit) {
    qint64 id=0;
    QString doctor,scheduled;
    if (edit) {
        id=selected(appointments);
        auto result=db.query("SELECT doctor,scheduled FROM appointment WHERE id=? AND status='Booked'",{id});
        if (!result.next()) throw std::runtime_error("Only booked appointments can be rescheduled.");
        doctor=result.value(0).toString(); scheduled=result.value(1).toString();
    }
    QDialog dialog(this); dialog.setWindowTitle(edit ? "Reschedule appointment" : "Book appointment");
    auto* layout=new QFormLayout(&dialog);
    QComboBox* patient=nullptr;
    if (!edit) patient=choices(layout,"Patient",db.query("SELECT id,name FROM patient ORDER BY name"));
    auto* doctorField=input(layout,"Doctor",doctor);
    doctorField->setPlaceholderText("Enter doctor's name");
    auto* date=new QDateTimeEdit(edit ? QDateTime::fromString(scheduled,"yyyy-MM-dd HH:mm") : QDateTime::currentDateTime().addSecs(3600));
    date->setCalendarPopup(true); date->setDisplayFormat("yyyy-MM-dd HH:mm"); layout->addRow("Date and time",date);
    buttons(dialog,layout);
    if (dialog.exec()!=QDialog::Accepted) return;
    const auto doctorName=required(doctorField).simplified().toCaseFolded();
    const auto time=date->dateTime().toString("yyyy-MM-dd HH:mm");
    if (edit) db.query("UPDATE appointment SET doctor=?,scheduled=? WHERE id=? AND status='Booked'",{doctorName,time,id});
    else db.insert("INSERT INTO appointment(patient_id,doctor,scheduled) VALUES(?,?,?)",{choice(patient),doctorName,time});
    refresh();
}
void Window::visitDialog() {
    QDialog dialog(this); dialog.setWindowTitle("Record visit");
    auto* layout=new QFormLayout(&dialog);
    auto* appointment=choices(layout,"Booked appointment",db.query("SELECT a.id,p.name||' · '||a.scheduled||' · '||a.doctor FROM appointment a JOIN patient p ON p.id=a.patient_id WHERE a.status='Booked' ORDER BY a.scheduled"));
    auto* diagnosis=input(layout,"Diagnosis");
    auto* notes=input(layout,"Clinical notes");
    buttons(dialog,layout);
    if (dialog.exec()!=QDialog::Accepted) return;
    db.visit(choice(appointment),required(diagnosis),required(notes)); refresh();
}
void Window::prescriptionDialog() {
    QDialog dialog(this); dialog.setWindowTitle("Issue prescription");
    auto* layout=new QFormLayout(&dialog);
    auto* visit=choices(layout,"Visit",db.query("SELECT v.id,p.name||' · Visit '||v.id FROM visit v JOIN appointment a ON a.id=v.appointment_id JOIN patient p ON p.id=a.patient_id ORDER BY v.id DESC"));
    auto* medicine=input(layout,"Medicine");
    auto* dosage=input(layout,"Dosage");
    auto* directions=input(layout,"Directions and duration");
    buttons(dialog,layout);
    if (dialog.exec()!=QDialog::Accepted) return;
    db.insert("INSERT INTO prescription(visit_id,medicine,dosage,directions) VALUES(?,?,?,?)",{choice(visit),required(medicine),required(dosage),required(directions)}); refresh();
}
void Window::billDialog() {
    QDialog dialog(this); dialog.setWindowTitle("Create bill / add service");
    auto* layout=new QFormLayout(&dialog);
    auto* visit=choices(layout,"Visit",db.query("SELECT v.id,p.name||' · Visit '||v.id FROM visit v JOIN appointment a ON a.id=v.appointment_id JOIN patient p ON p.id=a.patient_id ORDER BY v.id DESC"));
    if (bills->currentRow()>=0) {
        const auto billId=selected(bills);
        auto result=db.query("SELECT visit_id FROM bill WHERE id=?",{billId}); result.next();
        visit->setCurrentIndex(visit->findData(result.value(0)));
    }
    auto* description=input(layout,"Service description");
    auto* quantity=new QSpinBox; quantity->setRange(1,10000); layout->addRow("Quantity",quantity);
    auto* price=new QDoubleSpinBox; price->setDecimals(2); price->setRange(0.01,1000000); price->setPrefix("NGN "); layout->addRow("Unit charge",price);
    buttons(dialog,layout);
    if (dialog.exec()!=QDialog::Accepted) return;
    const auto visitId=choice(visit);
    const auto text=required(description);
    db.query("BEGIN IMMEDIATE");
    try {
        db.query("INSERT INTO bill(visit_id,issued) VALUES(?,?) ON CONFLICT(visit_id) DO NOTHING",{visitId,QDate::currentDate().toString(Qt::ISODate)});
        auto result=db.query("SELECT id FROM bill WHERE visit_id=?",{visitId}); result.next();
        db.insert("INSERT INTO bill_item(bill_id,description,quantity,unit_kobo) VALUES(?,?,?,?)",{result.value(0),text,quantity->value(),qRound64(price->value()*100)});
        db.query("COMMIT");
    } catch (...) { db.query("ROLLBACK"); throw; }
    refresh();
}
void Window::paymentDialog() {
    const auto id=selected(bills);
    QDialog dialog(this); dialog.setWindowTitle("Payment for bill "+QString::number(id));
    auto* layout=new QFormLayout(&dialog);
    auto* amount=new QDoubleSpinBox; amount->setDecimals(2); amount->setRange(0.01,100000000); amount->setPrefix("NGN "); layout->addRow("Amount",amount);
    auto* method=new QComboBox; method->addItems({"Cash","Transfer","Card"}); layout->addRow("Method",method);
    buttons(dialog,layout);
    if (dialog.exec()!=QDialog::Accepted) return;
    db.payment(id,qRound64(amount->value()*100),method->currentText()); refresh();
}
void Window::receipt() {
    const auto id=selected(payments);
    auto result=db.query("SELECT p.name,r.bill_id,r.paid_at,r.kobo,r.method FROM payment r JOIN bill b ON b.id=r.bill_id JOIN visit v ON v.id=b.visit_id JOIN appointment a ON a.id=v.appointment_id JOIN patient p ON p.id=a.patient_id WHERE r.id=?",{id});
    if (!result.next()) throw std::runtime_error("Receipt not found.");
    QString html = "<h1>" + hospitalTitle->text().toHtmlEscaped()
        + "</h1><h2>Payment receipt</h2><p>Receipt " + QString::number(id) + "</p>";
    const QStringList labels{"Patient","Bill","Date","Amount paid","Method"};
    for (int i=0;i<5;++i) html+="<p>"+labels[i]+": "+(i==3 ? money(result.value(i).toLongLong()) : result.value(i).toString()).toHtmlEscaped()+"</p>";
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer,this);
    if (dialog.exec()==QDialog::Accepted) { QTextDocument document; document.setHtml(html); document.print(&printer); }
}
void Window::reports() {
    if (from->date()>to->date()) throw std::runtime_error("The start date must come before the end date.");
    const QVariantList range{from->date().toString(Qt::ISODate),to->date().toString(Qt::ISODate)};
    fill(appointmentReport, {"ID","Patient","Doctor","Time","Status"}, "SELECT a.id,p.name,a.doctor,a.scheduled,a.status FROM appointment a JOIN patient p ON p.id=a.patient_id WHERE substr(a.scheduled,1,10) BETWEEN ? AND ? ORDER BY a.scheduled",range);
    fill(visitReport, {"Visit","Patient","Date","Diagnosis"}, "SELECT v.id,p.name,v.visited,v.diagnosis FROM visit v JOIN appointment a ON a.id=v.appointment_id JOIN patient p ON p.id=a.patient_id WHERE v.visited BETWEEN ? AND ? ORDER BY v.visited",range);
    fill(paymentReport, {"Receipt","Bill","Date","NGN received","Method"}, "SELECT id,bill_id,paid_at,printf('%.2f',kobo/100.0),method FROM payment WHERE paid_at BETWEEN ? AND ? ORDER BY paid_at",range);
    auto result=db.query("SELECT COALESCE(SUM(kobo),0) FROM payment WHERE paid_at BETWEEN ? AND ?",range); result.next();
    reportSummary->setText(QString("%1 appointments · %2 visits · Revenue received %3").arg(appointmentReport->rowCount()).arg(visitReport->rowCount()).arg(money(result.value(0).toLongLong())));
}
