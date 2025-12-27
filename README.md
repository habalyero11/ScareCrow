# 🦅 ScareCrowWeb

A web application for receiving images from ESP32-CAM devices, performing animal detection using YOLOv8, and displaying results on a modern dashboard.

![Python](https://img.shields.io/badge/Python-3.9+-blue.svg)
![FastAPI](https://img.shields.io/badge/FastAPI-0.104+-green.svg)
![YOLOv8](https://img.shields.io/badge/YOLOv8-Ultralytics-orange.svg)

## 🌟 Features

- **Real-time Animal Detection**: Uses YOLOv8 for accurate animal detection
- **Multi-Device Support**: Handle multiple ESP32-CAM devices simultaneously  
- **Modern Dashboard**: Beautiful dark-themed UI with glassmorphism design
- **Alert System**: Automatic alerts when animals are detected
- **Detection History**: Browse all past detections with images
- **Device Monitoring**: Track device status and connectivity

## 📁 Project Structure

```
scarecrowcode/
├── backend/
│   ├── main.py              # FastAPI application
│   ├── database.py          # SQLite database setup
│   ├── detection.py         # YOLO detection logic
│   ├── models.py            # Pydantic schemas
│   ├── requirements.txt     # Python dependencies
│   └── uploads/             # Uploaded images storage
├── frontend/
│   ├── index.html           # Main dashboard
│   ├── css/
│   │   └── styles.css       # Modern styling
│   ├── js/
│   │   └── app.js           # Frontend logic
│   └── pages/
│       ├── history.html     # Detection history
│       ├── alerts.html      # Alerts page
│       └── devices.html     # Device status
└── README.md
```

## 🚀 Quick Start

### Prerequisites

- Python 3.9 or higher
- pip (Python package manager)
- Modern web browser

### Backend Setup

1. **Navigate to backend directory**:
   ```bash
   cd backend
   ```

2. **Create virtual environment** (recommended):
   ```bash
   python -m venv venv
   
   # Windows
   venv\Scripts\activate
   
   # Linux/Mac
   source venv/bin/activate
   ```

3. **Install dependencies**:
   ```bash
   pip install -r requirements.txt
   ```

4. **Run the server**:
   ```bash
   uvicorn main:app --reload --host 0.0.0.0 --port 8000
   ```

   The API will be available at `http://localhost:8000`

### Frontend Setup

1. **Open the frontend**:
   - Simply open `frontend/index.html` in your browser
   - Or serve with a local server:
     ```bash
     cd frontend
     python -m http.server 3000
     ```
   - Then visit `http://localhost:3000`

## 📡 API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| `POST` | `/upload` | Receive image from ESP32-CAM |
| `GET` | `/detections` | Retrieve detection logs |
| `GET` | `/devices` | Retrieve device statuses |
| `GET` | `/alerts` | Retrieve alerts |
| `DELETE` | `/alerts/{id}` | Delete an alert |
| `GET` | `/stats` | Get dashboard statistics |
| `GET` | `/images/{filename}` | Serve uploaded images |

### ESP32-CAM Upload Example

```python
import requests

url = "http://YOUR_SERVER_IP:8000/upload"

with open("image.jpg", "rb") as f:
    files = {"image": f}
    data = {
        "device_id": "ESP32-CAM-001",
        "timestamp": "2024-01-15T10:30:00"
    }
    response = requests.post(url, files=files, data=data)
    print(response.json())
```

### ESP32-CAM Arduino Code Snippet

```cpp
#include <WiFi.h>
#include <HTTPClient.h>
#include "esp_camera.h"

const char* serverUrl = "http://YOUR_SERVER_IP:8000/upload";

void sendImage() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return;
    
    HTTPClient http;
    http.begin(serverUrl);
    http.addHeader("Content-Type", "multipart/form-data");
    
    // Send image with device_id
    String boundary = "----WebKitFormBoundary";
    String body = "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"device_id\"\r\n\r\n";
    body += "ESP32-CAM-001\r\n";
    body += "--" + boundary + "\r\n";
    body += "Content-Disposition: form-data; name=\"image\"; filename=\"capture.jpg\"\r\n";
    body += "Content-Type: image/jpeg\r\n\r\n";
    
    // ... add image data and send
    
    esp_camera_fb_return(fb);
}
```

## 🔧 Configuration

### Backend Configuration

Edit `backend/main.py` to change:
- CORS origins
- Upload directory path
- Server host/port

### Detection Configuration

Edit `backend/detection.py` to:
- Change YOLO model (default: yolov8n.pt)
- Add/remove animal classes
- Adjust confidence thresholds

## ☁️ Cloud Migration Guide

This application is designed for easy migration to cloud services.

### Database Migration (SQLite → PostgreSQL)

1. **Install PostgreSQL driver**:
   ```bash
   pip install asyncpg sqlalchemy[asyncio]
   ```

2. **Update `database.py`**:
   ```python
   # Replace SQLite connection
   DATABASE_URL = os.environ.get('DATABASE_URL')
   # Example: postgresql://user:pass@host:5432/dbname
   
   # Use SQLAlchemy with async support
   from sqlalchemy.ext.asyncio import create_async_engine
   engine = create_async_engine(DATABASE_URL)
   ```

3. **For Supabase**:
   - Create a new Supabase project
   - Get the PostgreSQL connection string from Settings > Database
   - Replace `DATABASE_URL` with Supabase connection string

### Image Storage Migration

1. **AWS S3**:
   ```python
   import boto3
   
   s3 = boto3.client('s3')
   s3.upload_fileobj(image_file, 'bucket-name', filename)
   ```

2. **Supabase Storage**:
   ```python
   from supabase import create_client
   
   supabase = create_client(url, key)
   supabase.storage.from_('images').upload(filename, image_data)
   ```

### Environment Variables

For production, use environment variables:

```bash
# .env file
DATABASE_URL=postgresql://user:pass@host:5432/db
S3_BUCKET=my-bucket
AWS_ACCESS_KEY_ID=xxx
AWS_SECRET_ACCESS_KEY=xxx
```

## 🧪 Testing

### Test Upload Endpoint

```bash
curl -X POST "http://localhost:8000/upload" \
  -F "image=@test_image.jpg" \
  -F "device_id=TEST-001" \
  -F "timestamp=2024-01-15T10:00:00"
```

### Test Detection Response

```json
{
  "success": true,
  "message": "Image processed successfully",
  "detection": {
    "id": 1,
    "device_id": "TEST-001",
    "animal": "cat",
    "confidence": 0.92,
    "image_path": "TEST-001_20240115_100000_abc123.jpg",
    "timestamp": "2024-01-15T10:00:00"
  }
}
```

## 🤝 Contributing

1. Fork the repository
2. Create your feature branch (`git checkout -b feature/AmazingFeature`)
3. Commit your changes (`git commit -m 'Add some AmazingFeature'`)
4. Push to the branch (`git push origin feature/AmazingFeature`)
5. Open a Pull Request

## 📝 License

This project is licensed under the MIT License.

## 🙋 Support

For questions or issues, please open a GitHub issue.

---

Built with ❤️ for smart agriculture and wildlife monitoring
