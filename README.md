Smart Dose – Automated Pill Organizer with Bluetooth Scheduling

A smart, Arduino-based pill management system that automates medication reminders, tracks pill count, and syncs schedules through a mobile app built with MIT App Inventor.

⭐ Features

4-slot pill dispenser (Morning, Afternoon, Evening, Night)

Automatic servo-controlled opening of compartments

Real-time clock scheduling using DS3231 RTC

Bluetooth communication using HC-05

LCD display with live updates

LED indicators + buzzer alerts

Manual override for each slot

Mobile app for schedule configuration & pill tracking

🧰 Hardware Used

As listed in the project report (Page 10–11):

Arduino Uno R3

HC-05 Bluetooth Module

DS3231 RTC

SG90 Servo Motors ×4

16×2 LCD (I2C)

Push buttons, LEDs, buzzer, compartments

📱 Mobile App

Created using MIT App Inventor

Includes:

Dashboard (Page 16, Fig 2.1)

Settings screen (Page 16, Fig 2.2)

📂 Repository Structure
/code
   smart_dose.ino
/app
   SmartDose.aia
   screenshots/
/hardware
   block_diagram.png
   circuit_diagram.png
   hardware_setup.jpeg
/report
   SmartDose_Project_Report.pdf

🚀 How It Works

User sets medicine name, time, and count in the app.

App sends data via Bluetooth → Arduino stores it.

At scheduled time:

Servo opens slot

LED glows

Buzzer alerts

LCD shows medicine info

User presses main button to confirm intake.

Count decreases & updated via Bluetooth.

👨‍💻 Contributors

Devulapalli Datta Sai Srinivas

Vattikuti Sri Sasank

Baswareddy Sreesanth Reddy

Moka Siva Pujitha

Vemuri Karthikeya

Guide: Dr. Ramesh A
