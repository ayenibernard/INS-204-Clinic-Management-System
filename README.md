# Clinic Qt — INS 204

Ayeni Oluwamuyiwa Bernard  
Matriculation number 2025/A/SENG/0250  
Department of Software Engineering, MIVA Open University  
o.ayeni4109@miva.edu.ng

## Open and run on Windows

1. Extract the ZIP completely into a folder such as `C:\Users\USER\Documents\INS204_Clinic_Qt`.
2. Open **MSYS2 UCRT64** and run the command below if any of these packages are missing.
3. Double-click **Build_and_Run.bat** in the extracted folder. It builds the source and starts the desktop application.

```sh
pacman -S --needed mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-cmake mingw-w64-ucrt-x86_64-ninja mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-gdb
```

The build expects MSYS2 under `C:\msys64`. For another installation path, open Command Prompt in the project folder and run:

```bat
set "CLINIC_QT_ROOT=D:\msys64\ucrt64"
Build_and_Run.bat
```

Use one toolchain consistently. The UCRT64 Qt libraries must be built with the UCRT64 compiler. A failed build prints its error in the command window.

## Use VS Code

Open the extracted **ClinicQt** folder itself using **File → Open Folder**. Install Microsoft's C/C++ extension if you want debugging.

- **Ctrl+Shift+B** configures and builds.
- **Terminal → Run Task → Clinic - Run** builds and launches.
- **F5** builds a debug version and starts GDB.

The included VS Code tasks use the standard `C:\msys64\ucrt64` path. Edit the paths in `.vscode/tasks.json` and `.vscode/launch.json` if MSYS2 is installed elsewhere.

## Hospital branding

The app starts with the sample name **Harmony General Hospital** above **Clinic Management System**. Use **Hospital settings** to enter the name you want. The app remembers the name on that Windows account and uses it on payment receipts.

## Dashboard analytics

The sidebar opens all six clinic sections. The dashboard calculates patient age bands from dates of birth and groups appointments by their stored status. The daily summary uses the computer’s local date.

## Demonstration

1. In **Patients**, register a fictional patient, then search and edit the record.
2. In **Appointments**, book a visit with a doctor's name and time. The system normalizes doctor names for conflict checking. Try booking the same doctor at the same time.
3. Select a booked appointment to reschedule or cancel it.
4. In **Clinical records**, save a diagnosis and notes against a booked appointment. This completes the appointment. Add one or more sample prescriptions to the visit.
5. In **Billing**, create a bill for the visit. Add further services using the same visit. Each service has quantity and unit price. Select the bill to view its service items.
6. Select the bill and record a partial payment. Record the remainder separately. The application rejects payments above the outstanding balance.
7. Select a payment receipt and click **Print selected payment receipt**. Choose a printer or Microsoft Print to PDF.
8. In **Reports**, choose the date range and click **Generate reports**. Appointments use their scheduled date. Visits and payments use the date they were recorded. Export the displayed results as CSV if required.
9. Close and reopen the app to verify saved records.

## Database and design

All application logic and interface code is C++. Qt Widgets provides the desktop interface. Qt SQL uses its QSQLITE driver to access SQLite. CMake builds the program.

The seven database tables are patient, appointment, visit, prescription, bill, bill_item and payment. Foreign keys connect the records. A unique index prevents two booked appointments for the same doctor at the same minute. Billing uses integer kobo. Saving a visit and changing the appointment status happens in one transaction. Payment validation and insertion use a transaction to keep balances consistent.

The database is created in the Windows application data folder returned by Qt, outside the source folder. Use **Dashboard → Show database location** to see its exact path. Keep a copy of that database if you want to preserve your demonstration records. The earlier browser prototype uses a different data format and has no automatic import into this version.

## Verification

After building, run **Verify.bat**. It uses a temporary database and checks the main workflow, validation, totals, persistence and Qt window startup.

This academic prototype uses a shared local interface. Staff accounts, role permissions, encryption and scheduled backups are still design requirements. Use fictional records during demonstrations. Doctor details are entered when booking appointments.

## Source files

- `main.cpp` — application startup, theme, data folder, instance lock and verification mode.
- `database.h / database.cpp` — schema, SQL access and transaction rules.
- `window.h / window.cpp` — dashboard, patient forms, appointments, clinical records, billing and reports.
- `CMakeLists.txt` — Qt Widgets, SQL and PrintSupport build configuration.
- `.vscode` — build, run and debug configuration.

The source package is intended for use on the computer with the MSYS2 Qt tools installed. The EXE produced in `build` relies on those Qt libraries.

## Package validation

The source was compiled with GCC and Qt 6.8.2 on Linux. The temporary-database verification passed, including the Qt window startup check using the offscreen platform. Windows launching, interactive layout and printing still need verification on the target laptop.
