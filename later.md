# ScareCrowWeb - Continue Later Notes

## Current Status: ONNX Migration Complete 🔄

Migrated from PyTorch/Ultralytics to ONNX Runtime to fix Railway 4GB image limit.
Previous image: 8.4GB → Expected new image: ~800MB-1.2GB

---

## What's Done

1. **Railway Deployment** - Backend deployed to Railway.app
2. **Supabase Integration** - Created `database_supabase.py` and `storage.py`
3. **ESP32 Compatibility** - Added `/upload/esp32` endpoint for raw JPEG uploads
4. **Frontend Config** - Added `config.js` for switching between local/cloud API

---

## Next Steps To Complete

### 1. Get Railway URL
- Go to Railway Dashboard → Your service → Settings → Networking
- Click "Generate Domain" to get a public URL like `https://xxx.up.railway.app`

### 2. Add Environment Variables in Railway
Go to your Railway service → Variables tab and add:
```
SUPABASE_URL=https://cxqwlacbhhianfqxqmjk.supabase.co
SUPABASE_KEY=(your Supabase anon key from .env)
STORAGE_BUCKET=scarecrow-images
```

### 3. Set Up Supabase Database
Run `backend/supabase_setup.sql` in Supabase SQL Editor

### 4. Create Supabase Storage Bucket
Supabase Dashboard → Storage → New Bucket → Name: `scarecrow-images` → Public: ON

### 5. Update Frontend Config
Edit `frontend/js/config.js`:
```javascript
window.SCARECROW_API_URL = 'https://YOUR-RAILWAY-URL.up.railway.app';
```

### 6. Update ESP32 Devices
In ESP32 captive portal, set Server URL to your Railway URL

---

## Important Files Created

| File | Purpose |
|------|---------|
| `backend/main_cloud.py` | Cloud FastAPI with Supabase |
| `backend/database_supabase.py` | Supabase database operations |
| `backend/storage.py` | Supabase image storage |
| `Dockerfile` (root) | Railway build config |
| `frontend/js/config.js` | API URL configuration |

---

## Railway Settings That Worked

- **Root Directory**: `backend`
- **Builder**: Dockerfile
- Uses root `Dockerfile` with `COPY . .`

---

## GitHub Repo
https://github.com/habalyero11/ScareCrow

Good night! 🌙
