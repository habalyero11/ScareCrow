# ScareCrowWeb Railway Deployment

## Prerequisites
- Railway account (sign up at https://railway.app)
- Railway CLI installed: `npm install -g @railway/cli`
- Git installed

## Quick Deploy

### 1. Login to Railway
```bash
railway login
```

### 2. Initialize Project
```bash
cd backend
railway init
```
Select "Empty Project" when prompted.

### 3. Add Environment Variables
In Railway Dashboard or via CLI:
```bash
railway variables set SUPABASE_URL=https://cxqwlacbhhianfqxqmjk.supabase.co
railway variables set SUPABASE_KEY=your-anon-key
railway variables set STORAGE_BUCKET=scarecrow-images
```

### 4. Deploy
```bash
railway up
```

Railway will automatically:
- Detect the Dockerfile
- Build the container
- Deploy your app
- Give you a public URL like `https://scarecrow-api.up.railway.app`

### 5. Get Your URL
```bash
railway open
```
Or check the Railway Dashboard for your deployment URL.

## After Deployment

1. **Update Frontend**: Edit `frontend/js/config.js`:
   ```javascript
   window.SCARECROW_API_URL = 'https://your-app.up.railway.app';
   ```

2. **Update ESP32**: Configure your ESP32 devices with the Railway URL

## Useful Commands

| Command | Description |
|---------|-------------|
| `railway logs` | View deployment logs |
| `railway status` | Check deployment status |
| `railway variables` | List environment variables |
| `railway open` | Open deployed app in browser |

## Free Tier

Railway offers $5 free credit monthly - more than enough for this project!
