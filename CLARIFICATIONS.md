# ScareCrow System - Clarification Guide

> Comprehensive answers to common questions about setup, networking, and deployment.

---

## Table of Contents
1. [SIM800L & Cellular Communication](#1-sim800l--cellular-communication)
2. [ESP32 Captive Portal Access](#2-esp32-captive-portal-access)
3. [Port Forwarding Setup](#3-port-forwarding-setup)
4. [Internet Communication Flow](#4-internet-communication-flow)
5. [Running the Backend](#5-running-the-backend)
6. [Auto-Start Backend on Boot](#6-auto-start-backend-on-boot)
7. [Custom Domain Setup](#7-custom-domain-setup)
8. [API Location & Documentation](#8-api-location--documentation)
9. [Common Problems & Confusions](#9-common-problems--confusions)
10. [What's Missing / Future Considerations](#10-whats-missing--future-considerations)

---

## 1. SIM800L & Cellular Communication

### Q: If I use SIM800L, how will ESP32 talk to my computer?

**Answer:** It will talk **via the internet**, just like any phone does.

```
┌─────────────────┐     Cellular      ┌─────────────────┐     Internet     ┌─────────────────┐
│     ESP32 +     │───────────────────│   Cell Tower    │─────────────────│  Your Computer  │
│     SIM800L     │   (SIM card data) │   (Globe/Smart) │    (router)     │   (public IP)   │
└─────────────────┘                   └─────────────────┘                 └─────────────────┘
```

**Requirements:**
- Your computer must have a **public IP address** or **Dynamic DNS** (DDNS)
- Your router must have **port forwarding** configured (see section 3)
- Your SIM card needs an **active data plan**

**Important:** SIM800L uses 2G network. Check if your carrier still supports 2G in your area. Alternative: Use SIM7000 or SIM7600 for 4G LTE.

---

## 2. ESP32 Captive Portal Access

### Q: How to access the ESP32 portal?

**Answer:** There are **two different portals** depending on the situation:

| Portal | When to Use | IP Address | How to Access |
|--------|-------------|------------|---------------|
| **Captive Portal** | Initial setup (no WiFi saved) | `192.168.4.1` | Connect to `ScareCrow-XXXX` WiFi, go to `http://192.168.4.1` |
| **Status Page** | After WiFi connected | ESP32's IP (e.g., `192.168.0.104`) | Open `http://ESP32_IP` in browser (same network) |

**Captive Portal Triggers:**
- First boot (no saved WiFi)
- WiFi connection fails
- Hold BOOT button for 10 seconds (manual reset)

---

## 3. Port Forwarding Setup

### Q: How to forward the ports of my PC?

**Answer:** Port forwarding tells your router to send incoming internet traffic to your computer.

### Steps (Generic - varies by router):

1. **Find your PC's local IP:**
   ```
   Open CMD → type: ipconfig
   Look for: IPv4 Address (e.g., 192.168.0.103)
   ```

2. **Access your router:**
   - Open browser → go to `192.168.0.1` or `192.168.1.1` (check router label)
   - Login with router credentials (often admin/admin or printed on router)

3. **Find Port Forwarding section:**
   - Usually under: Advanced → NAT → Port Forwarding
   - Or: Security → Apps and Gaming → Port Forwarding

4. **Add a rule:**
   | Field | Value |
   |-------|-------|
   | Service Name | ScareCrow |
   | External Port | 8000 |
   | Internal Port | 8000 |
   | Internal IP | 192.168.0.103 (your PC) |
   | Protocol | TCP |

5. **Save and restart router**

### Testing:
- Go to https://canyouseeme.org/
- Enter port 8000
- If it shows "Success", port forwarding works!

**⚠️ Security Warning:** Opening ports exposes your PC to the internet. Consider:
- Using a firewall
- Adding authentication to the API
- Using HTTPS

---

## 4. Internet Communication Flow

### Q: Will it talk to each other via internet?

**Answer:** Yes! Here's the complete flow:

### Local Network (Same WiFi):
```
ESP32 (192.168.0.104) ──► Router ──► PC (192.168.0.103:8000)
                         (LAN only, no internet needed)
```

### Via Internet (SIM800L or remote WiFi):
```
ESP32 ──► Internet ──► Your Public IP ──► Router ──► Port Forward ──► PC (192.168.0.103:8000)
```

**What you need for internet access:**
- [ ] Public IP address (from your ISP) OR Dynamic DNS
- [ ] Port forwarding configured
- [ ] Firewall allows port 8000
- [ ] Server URL in ESP32 set to your public IP/domain

---

## 5. Running the Backend

### Q: How to run the backend?

**Answer:** 

### Manual Start:
```bash
# Navigate to backend folder
cd C:\Users\Aleyah\Desktop\scarecrowcode\backend

# Activate virtual environment (if using one)
# OR run directly with Python

# Start the server
py -3.11 -m uvicorn main:app --reload --host 0.0.0.0 --port 8000
```

### What Each Part Means:
| Part | Meaning |
|------|---------|
| `py -3.11` | Use Python 3.11 |
| `-m uvicorn` | Run uvicorn module |
| `main:app` | Run `app` from `main.py` |
| `--reload` | Auto-restart on code changes (dev only) |
| `--host 0.0.0.0` | Accept connections from any IP |
| `--port 8000` | Listen on port 8000 |

### For Production (without --reload):
```bash
py -3.11 -m uvicorn main:app --host 0.0.0.0 --port 8000
```

---

## 6. Auto-Start Backend on Boot

### Q: If I turn on my computer, will the backend automatically run?

**Answer:** **No, not by default.** You need to configure it.

### Option A: Windows Startup Folder (Easiest)

1. Create `start_scarecrow.bat`:
   ```batch
   @echo off
   cd /d C:\Users\Aleyah\Desktop\scarecrowcode\backend
   py -3.11 -m uvicorn main:app --host 0.0.0.0 --port 8000
   ```

2. Press `Win + R`, type: `shell:startup`

3. Copy the `.bat` file to this folder

4. Restart computer to test

### Option B: Windows Task Scheduler (More Control)

1. Open Task Scheduler
2. Create Basic Task → Name: "ScareCrow Backend"
3. Trigger: "When the computer starts"
4. Action: "Start a program"
5. Program: `py`
6. Arguments: `-3.11 -m uvicorn main:app --host 0.0.0.0 --port 8000`
7. Start in: `C:\Users\Aleyah\Desktop\scarecrowcode\backend`

### Option C: Install as Windows Service (Advanced)
Use NSSM (Non-Sucking Service Manager) to run as background service.

---

## 7. Custom Domain Setup

### Q: I want to make a custom domain, will my computer support it or is that not free?

**Answer:** Your computer CAN support a custom domain. Here's the breakdown:

### Cost Breakdown:
| Item | Cost | Notes |
|------|------|-------|
| Domain Name | $10-15/year | From Namecheap, Porkbun, GoDaddy |
| Dynamic DNS | FREE | DuckDNS, No-IP free tier |
| SSL Certificate | FREE | Let's Encrypt, Cloudflare |
| Hosting on your PC | FREE | No extra cost |

### Option 1: Free Subdomain with DDNS (Recommended to Start)
Use DuckDNS (free):
1. Go to https://www.duckdns.org/
2. Sign in with Google/GitHub
3. Create subdomain: `myscarecrow.duckdns.org`
4. Install DuckDNS updater on your PC
5. Your ESP32 URL becomes: `http://myscarecrow.duckdns.org:8000`

### Option 2: Custom Domain ($10-15/year)
1. Buy domain at Namecheap/Porkbun
2. Point DNS to your public IP (use Cloudflare for free SSL)
3. Your ESP32 URL becomes: `https://api.mydomain.com`

### Using Cloudflare Tunnel (Recommended - No Port Forwarding!)
Cloudflare Tunnel is FREE and more secure:
- No need to open ports
- Free SSL/HTTPS
- Works even with dynamic IP
- Install `cloudflared` on your PC

---

## 8. API Location & Documentation

### Q: Where do I find the API? Is the API standalone?

**Answer:**

### API Location:
```
http://192.168.0.103:8000/docs    ← Interactive API Documentation (Swagger UI)
http://192.168.0.103:8000/redoc   ← Alternative API Documentation
```

### API Endpoints:

| Method | Endpoint | Purpose |
|--------|----------|---------|
| `POST` | `/upload` | ESP32 uploads images here |
| `GET` | `/devices/{id}/config` | ESP32 fetches its settings |
| `PUT` | `/devices/{id}/config` | Dashboard updates device settings |
| `GET` | `/detections` | Get list of all detections |
| `GET` | `/stats` | Dashboard statistics |

### Is the API Standalone?
**Yes and No:**

- **The API (FastAPI)** is a standalone Python application
- **BUT** it depends on:
  - Python 3.11+
  - Libraries in `requirements.txt`
  - YOLO model file (downloads automatically)
  
- **The Frontend** (HTML dashboard) is served BY the API at `http://localhost:8000`

### File Structure:
```
scarecrowcode/
├── backend/
│   ├── main.py         ← API is here
│   ├── detection.py    ← YOLO detection
│   ├── database.py     ← SQLite database
│   └── requirements.txt
├── frontend/           ← Dashboard (served by API)
└── esp32/              ← ESP32 firmware
```

---

## 9. Common Problems & Confusions

### 🔴 Problem: "Config fetch failed, HTTP code: -1"
**Cause:** ESP32 can't reach the server  
**Solutions:**
- Check server URL is correct (192.168.0.x vs 192.168.1.x)
- Ensure backend is running
- ESP32 and PC on same network?
- Firewall blocking port 8000?

### 🔴 Problem: "Connection refused" 
**Cause:** Server not running or wrong port  
**Solutions:**
- Start the backend server
- Check port 8000 is correct
- Check `--host 0.0.0.0` is set (not just localhost)

### 🔴 Problem: Works locally, but not from internet
**Cause:** Port forwarding not set up  
**Solutions:**
- Configure port forwarding (section 3)
- Check public IP: https://whatismyip.com
- Test with https://canyouseeme.org

### 🟡 Confusion: "Why do I need my PC's IP?"
**Explanation:** The ESP32 is just a camera. It needs to send photos somewhere for AI processing. Your PC runs the AI.

### 🟡 Confusion: "What's the difference between 192.168.4.1 and 192.168.0.104?"
| IP | What It Is |
|----|------------|
| `192.168.4.1` | ESP32's own hotspot (captive portal mode) |
| `192.168.0.104` | ESP32's IP when connected to your WiFi |
| `192.168.0.103` | Your PC (backend server) |

### 🟡 Confusion: "Can ESP32 run YOLO itself?"
**No.** ESP32 doesn't have enough processing power. YOLO needs a real computer (PC, server, or at minimum a Raspberry Pi 4).

---

## 10. What's Missing / Future Considerations

### Not Yet Implemented:
- [ ] **SIM800L full code** - Only placeholders exist, TinyGSM integration needed
- [ ] **HTTPS/SSL** - Currently HTTP only (not secure for internet)
- [ ] **User Authentication** - Anyone with URL can access dashboard
- [ ] **Email/SMS Alerts** - No notification system yet
- [ ] **ESP32 Status Webpage** - Could add local web interface on ESP32

### Recommended Before "Going Live":
1. ✅ Add authentication to API
2. ✅ Set up HTTPS (via Cloudflare or Let's Encrypt)
3. ✅ Test SIM800L if using cellular
4. ✅ Set up auto-start on PC boot
5. ✅ Create backup of database

### For Production Deployment:
Consider moving to cloud server (Render.com ~$7/mo) to avoid:
- PC running 24/7
- Port forwarding complexity
- Dynamic IP issues
- Security concerns

---

## Quick Reference Card

| What | Where |
|------|-------|
| Backend Server | `http://192.168.0.103:8000` |
| API Docs | `http://192.168.0.103:8000/docs` |
| Dashboard | `http://192.168.0.103:8000` |
| ESP32 Captive Portal | `http://192.168.4.1` |
| Backend Code | `scarecrowcode/backend/main.py` |
| ESP32 Code | `scarecrowcode/esp32/ScareCrowESP32_v2/` |

---

*Last Updated: December 20, 2024*
