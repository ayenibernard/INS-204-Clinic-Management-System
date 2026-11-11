#include "database.h"
#include "window.h"
#include <QtWidgets>
#include <QSqlError>
#include <iostream>
#include <stdexcept>

static void verify(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
static int selfTest() {
    QTemporaryDir directory;
    verify(directory.isValid(), "Cannot create temporary test folder.");
    const auto path=directory.filePath("test.sqlite");
    {
        Database db(path);
        const auto patient=db.insert("INSERT INTO patient(name,dob,phone,address) VALUES(?,?,?,?)",{"Ada Demo","1990-01-02","08000000000","Ilorin"});
        db.query("UPDATE patient SET phone=? WHERE id=?",{"08000000001",patient});
        const auto appointment=db.insert("INSERT INTO appointment(patient_id,doctor,scheduled) VALUES(?,?,?)",{patient,"dr demo","2026-11-10 10:00"});
        bool rejected=false;
        try { db.insert("INSERT INTO appointment(patient_id,doctor,scheduled) VALUES(?,?,?)",{patient,"dr demo","2026-11-10 10:00"}); }
        catch (...) { rejected=true; }
        verify(rejected,"Double booking was accepted.");
        db.query("UPDATE appointment SET scheduled=? WHERE id=?",{"2026-11-10 11:00",appointment});
        const auto cancelled=db.insert("INSERT INTO appointment(patient_id,doctor,scheduled) VALUES(?,?,?)",{patient,"dr demo","2026-11-10 12:00"});
        db.query("UPDATE appointment SET status='Cancelled' WHERE id=?",{cancelled});
        db.visit(appointment,"Sample diagnosis","Sample notes");
        rejected=false;
        try { db.visit(appointment,"Duplicate diagnosis","Notes"); } catch (...) { rejected=true; }
        verify(rejected,"A second visit was accepted for one appointment.");
        auto result=db.query("SELECT id FROM visit WHERE appointment_id=?",{appointment});
        verify(result.next(),"Visit was not saved.");
        const auto visit=result.value(0);
        db.insert("INSERT INTO prescription(visit_id,medicine,dosage,directions) VALUES(?,?,?,?)",{visit,"Demo medicine","Demo dosage","Demo directions"});
        const auto bill=db.insert("INSERT INTO bill(visit_id,issued) VALUES(?,?)",{visit,QDate::currentDate().toString(Qt::ISODate)});
        db.insert("INSERT INTO bill_item(bill_id,description,quantity,unit_kobo) VALUES(?,?,?,?)",{bill,"Consultation",1,100000});
        db.insert("INSERT INTO bill_item(bill_id,description,quantity,unit_kobo) VALUES(?,?,?,?)",{bill,"Sample service",2,25000});
        db.payment(bill,50000,"Cash");
        rejected=false;
        try { db.payment(bill,100001,"Transfer"); } catch (...) { rejected=true; }
        verify(rejected,"Overpayment was accepted.");
        db.payment(bill,100000,"Transfer");
        result=db.query("SELECT SUM(kobo) FROM payment WHERE paid_at BETWEEN ? AND ?",{QDate::currentDate().toString(Qt::ISODate),QDate::currentDate().toString(Qt::ISODate)});
        verify(result.next() && result.value(0).toLongLong()==150000,"Revenue report is incorrect.");
        result=db.query("PRAGMA foreign_key_check");
        verify(!result.next(),"Foreign key integrity failed.");
        Window window(db);
        window.refresh();
        window.show();
        QApplication::processEvents();
    }
    {
        Database reopened(path);
        auto result=reopened.query("SELECT COUNT(*),SUM(kobo) FROM payment");
        verify(result.next() && result.value(0).toInt()==2 && result.value(1).toLongLong()==150000,"Persistence after reopening failed.");
        result=reopened.query("SELECT phone FROM patient");
        verify(result.next() && result.value(0).toString()=="08000000001","Patient update was lost.");
    }
    std::cout<<"PASS: patient update, scheduling, double-booking rejection, rescheduling, cancellation, visits, prescriptions, multiple bill items, split payments, overpayment rejection, revenue, foreign keys, Qt window startup and persistence.\n";
    return 0;
}
int main(int argc,char* argv[]) {
    QApplication app(argc,argv);
    app.setWindowIcon(QIcon(":/branding/heartbeat-h.png"));
    app.setOrganizationName("MIVAStudentProjects");
    app.setApplicationName("INS204ClinicQt");
    app.setStyle("Fusion");
    app.setStyleSheet(R"(
        QWidget { background: #101b2d; color: #e6effb; font-family: "Segoe UI"; font-size: 10pt; }
        QLabel#title { color: #f4f8ff; font-size: 24pt; font-weight: 700; }
        QLabel#subtitle { color: #8bddcf; font-size: 12pt; }
        QLabel#sectionTitle { color: #edf4fc; font-size: 13pt; font-weight: 700; }
        QFrame#metricCard { background: #192d46; border: 1px solid #354e69; border-radius: 10px; }
        QLabel#metricLabel { background: transparent; color: #b4c8dc; }
        QLabel#metricValue { background: transparent; color: #f4f8ff; font-size: 22pt; font-weight: 700; }
        QPushButton#secondaryAction { background: #192d46; border-color: #536e8c; }
        QPushButton#secondaryAction:hover { background: #294460; }
        QListWidget#navigation { background: #132137; border: 1px solid #354e69; border-radius: 8px; padding: 8px; }
        QListWidget#navigation::item { border-radius: 5px; padding: 11px 8px; }
        QListWidget#navigation::item:selected { background: #17685e; color: #ffffff; }
        QLabel#sectionTitle { background: transparent; }
        QProgressBar { border: 1px solid #354e69; border-radius: 4px; background: #132137; color: white; text-align: center; }
        QProgressBar::chunk { background: #17685e; border-radius: 3px; }
        QPushButton { background: #1c554f; border: 1px solid #41867e; border-radius: 5px; padding: 8px 12px; }
        QPushButton:hover { background: #257267; }
        QLineEdit, QComboBox, QDateEdit, QDateTimeEdit, QSpinBox, QDoubleSpinBox { background: #1a2b41; padding: 7px; border: 1px solid #536e8c; border-radius: 4px; }
        QTableWidget { background: #132137; alternate-background-color: #1a2d45; gridline-color: #354e69; selection-background-color: #206b64; }
        QHeaderView::section { background: #203a55; padding: 8px; border: none; }
        QDialog { min-width: 480px; }
    )");
    try {
        if (app.arguments().contains("--self-test")) return selfTest();
        const auto folder=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        if (!QDir().mkpath(folder)) throw std::runtime_error("Cannot create the clinic data folder.");
        QLockFile lock(folder+"/clinic.lock");
        lock.setStaleLockTime(0);
        if (!lock.tryLock(100)) throw std::runtime_error("The clinic application is already running, or its data folder is locked.");
        Database db(folder+"/clinic.sqlite");
        Window window(db);
        window.show();
        return app.exec();
    } catch (const std::exception& error) {
        if (app.arguments().contains("--self-test")) {
            std::cerr<<error.what()<<"\n";
            return 1;
        }
        QMessageBox::critical(nullptr,"Clinic startup",QString::fromUtf8(error.what()));
        return 1;
    }
}
