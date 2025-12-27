1. Project Overview

Build a web application called ScareCrowWeb that receives images from multiple ESP32-CAM devices triggered by PIR motion sensors, performs animal detection using OpenCV and YOLO on the backend, stores detection data in a local database, and displays results on a web dashboard.

This setup is for testing only and must be designed so it can later be migrated to a cloud database without changing system behavior.

2. System Flow (MUST FOLLOW EXACTLY)

PIR sensor detects motion

ESP32-CAM captures a still image (not video stream)

ESP32-CAM sends the image via HTTP POST to the backend

Backend saves the image locally

Backend runs YOLO (via OpenCV, Python) for animal detection

Detection results are stored in a local database

Web dashboard displays detections and logs

Alerts are generated based on detection results

3. Hardware Assumptions

ESP32-CAM with PIR sensor

Multiple ESP32-CAM devices supported

Each device sends a unique device_id

ESP32 performs no ML and no database operations

4. Backend Requirements

Technology Stack:

Python backend using FastAPI (preferred) or Flask

OpenCV

YOLOv8 (Ultralytics)

Local SQLite database (testing only)

Backend Responsibilities:

Provide POST /upload endpoint to receive images from ESP32-CAM

Accept metadata: device_id, timestamp

Save uploaded images to a local folder

Run YOLO animal detection on the saved image

Return detected animal type and confidence score

Store detection data in SQLite

Handle multiple ESP32-CAM uploads concurrently

Constraints:

YOLO runs only in the backend

No video streaming

Backend communicates with the database, not ESP32

5. Local Database Design (SQLite)

Create the following tables:

devices

device_id

last_seen

status

detections

id

device_id

animal

confidence

image_path

timestamp

alerts

id

device_id

message

timestamp

The database must be initialized automatically when the app starts.

6. Frontend Requirements

Features:

Dashboard displaying:

Detected animal

Confidence score

Device ID

Timestamp

Captured image

Detection history / logs page

Alert list

Device list and status

Constraints:

Frontend performs no ML

Frontend only consumes backend APIs

7. Animal Detection (ML Requirements)

Use YOLOv8 pre-trained model

Use OpenCV for image loading and preprocessing

Detection runs once per uploaded image

Bounding boxes may be optionally drawn for visualization

8. API Endpoints (Minimum)

POST /upload – receive image from ESP32-CAM

GET /detections – retrieve detection logs

GET /devices – retrieve device status

GET /alerts – retrieve alerts

9. Non-Functional Requirements

Support multiple ESP32-CAM devices

Clear separation of backend, ML, and frontend

Easy migration from SQLite to Supabase/PostgreSQL later

Web-based access only

10. Output Expected from AI Coder

The AI coder should:

Generate backend project structure

Provide FastAPI/Flask code

Integrate YOLO and OpenCV

Implement SQLite database models

Create basic frontend dashboard UI

Clearly document where cloud migration would occur later

Do NOT:

Add ML to ESP32-CAM

Use video streaming

Introduce cloud services

Change the system flow

Final Instruction to AI Coder

Implement the system exactly as described.
This is a local testing version, not a simplified version.
The architecture must remain compatible with future cloud deployment.