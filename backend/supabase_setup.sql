-- ==============================================
-- ScareCrowWeb Supabase Database Setup
-- ==============================================
-- Run this in your Supabase SQL Editor:
-- https://supabase.com/dashboard/project/YOUR_PROJECT/sql
-- ==============================================

-- Enable UUID extension (usually already enabled)
CREATE EXTENSION IF NOT EXISTS "uuid-ossp";

-- ==============================================
-- TABLES
-- ==============================================

-- Devices table
CREATE TABLE IF NOT EXISTS devices (
    device_id TEXT PRIMARY KEY,
    last_seen TIMESTAMPTZ DEFAULT NOW(),
    status TEXT DEFAULT 'offline'
);

-- Detections table
CREATE TABLE IF NOT EXISTS detections (
    id SERIAL PRIMARY KEY,
    device_id TEXT REFERENCES devices(device_id) ON DELETE CASCADE,
    animal TEXT,
    confidence REAL,
    image_path TEXT,
    timestamp TIMESTAMPTZ DEFAULT NOW()
);

-- Alerts table
CREATE TABLE IF NOT EXISTS alerts (
    id SERIAL PRIMARY KEY,
    device_id TEXT REFERENCES devices(device_id) ON DELETE CASCADE,
    message TEXT,
    timestamp TIMESTAMPTZ DEFAULT NOW()
);

-- Device configuration table
CREATE TABLE IF NOT EXISTS device_config (
    device_id TEXT PRIMARY KEY REFERENCES devices(device_id) ON DELETE CASCADE,
    servo_speed INTEGER DEFAULT 90,
    servo_angle INTEGER DEFAULT 180,
    servo_duration INTEGER DEFAULT 3,
    pir_delay_ms INTEGER DEFAULT 500,
    pir_cooldown_sec INTEGER DEFAULT 10,
    led_pattern TEXT DEFAULT 'blink',
    led_speed_ms INTEGER DEFAULT 200,
    led_duration_sec INTEGER DEFAULT 5,
    sound_type TEXT DEFAULT 'beep',
    sound_volume INTEGER DEFAULT 80,
    sound_duration_sec INTEGER DEFAULT 3,
    auto_deterrent BOOLEAN DEFAULT TRUE,
    detection_threshold REAL DEFAULT 0.5,
    updated_at TIMESTAMPTZ DEFAULT NOW()
);

-- ==============================================
-- INDEXES (for better query performance)
-- ==============================================

CREATE INDEX IF NOT EXISTS idx_detections_timestamp ON detections(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_detections_device ON detections(device_id);
CREATE INDEX IF NOT EXISTS idx_alerts_timestamp ON alerts(timestamp DESC);
CREATE INDEX IF NOT EXISTS idx_alerts_device ON alerts(device_id);

-- ==============================================
-- ROW LEVEL SECURITY (RLS)
-- For production, enable RLS and create policies
-- ==============================================

-- Enable RLS (uncomment for production)
-- ALTER TABLE devices ENABLE ROW LEVEL SECURITY;
-- ALTER TABLE detections ENABLE ROW LEVEL SECURITY;
-- ALTER TABLE alerts ENABLE ROW LEVEL SECURITY;
-- ALTER TABLE device_config ENABLE ROW LEVEL SECURITY;

-- Allow all operations for authenticated users (adjust for your needs)
-- CREATE POLICY "Allow all for authenticated" ON devices FOR ALL USING (true);
-- CREATE POLICY "Allow all for authenticated" ON detections FOR ALL USING (true);
-- CREATE POLICY "Allow all for authenticated" ON alerts FOR ALL USING (true);
-- CREATE POLICY "Allow all for authenticated" ON device_config FOR ALL USING (true);

-- ==============================================
-- STORAGE BUCKET
-- ==============================================
-- Create this manually in Supabase Dashboard:
-- 1. Go to Storage
-- 2. Create bucket named: scarecrow-images
-- 3. Set as PUBLIC bucket
-- 4. Or run this (may need elevated permissions):
-- ==============================================

-- Note: Storage bucket creation via SQL may not work on all Supabase plans
-- INSERT INTO storage.buckets (id, name, public)
-- VALUES ('scarecrow-images', 'scarecrow-images', true)
-- ON CONFLICT DO NOTHING;

-- ==============================================
-- VERIFICATION QUERIES
-- Run these to verify setup
-- ==============================================

-- Check tables exist:
-- SELECT table_name FROM information_schema.tables WHERE table_schema = 'public';

-- Check storage buckets:
-- SELECT * FROM storage.buckets;

COMMENT ON TABLE devices IS 'ESP32-CAM devices that send images';
COMMENT ON TABLE detections IS 'Animal detection results from YOLO';
COMMENT ON TABLE alerts IS 'Alert notifications for detected animals';
COMMENT ON TABLE device_config IS 'Remote configuration for ESP32 devices';

-- Done!
SELECT 'ScareCrowWeb database setup complete!' as status;
