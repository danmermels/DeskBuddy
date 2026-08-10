CREATE TABLE IF NOT EXISTS telemetry (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    chip_id TEXT NOT NULL,
    fw_ver TEXT NOT NULL,
    hw_rev TEXT DEFAULT 'esp32c3',
    ts INTEGER NOT NULL,
    uptime_h REAL,
    boot_count INTEGER DEFAULT 0,
    clock_face INTEGER,
    ai_mode INTEGER,
    ai_persona INTEGER,
    temp_unit INTEGER,
    time_24h INTEGER,
    font_idx INTEGER,
    daily_desk_h REAL,
    daily_focus_h REAL,
    daily_breaks INTEGER,
    prod_score INTEGER,
    daily_ai_requests INTEGER,
    heap_free_kb INTEGER,
    created_at TEXT DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_telemetry_chip ON telemetry(chip_id);
CREATE INDEX IF NOT EXISTS idx_telemetry_ts ON telemetry(ts);
CREATE INDEX IF NOT EXISTS idx_telemetry_fw ON telemetry(fw_ver);

CREATE TABLE IF NOT EXISTS firmware_versions (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    version TEXT NOT NULL UNIQUE,
    url TEXT NOT NULL,
    size INTEGER NOT NULL,
    sha256 TEXT NOT NULL,
    mandatory INTEGER DEFAULT 0,
    release_notes TEXT DEFAULT '',
    min_fw_ver TEXT DEFAULT '',
    created_at TEXT DEFAULT (datetime('now'))
);

-- Store
CREATE TABLE IF NOT EXISTS products (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    slug TEXT NOT NULL UNIQUE,
    name TEXT NOT NULL,
    description TEXT DEFAULT '',
    price_cents INTEGER NOT NULL,
    image_key TEXT DEFAULT '',
    active INTEGER DEFAULT 1,
    sort_order INTEGER DEFAULT 0,
    created_at TEXT DEFAULT (datetime('now'))
);

CREATE TABLE IF NOT EXISTS orders (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    stripe_session_id TEXT NOT NULL UNIQUE,
    customer_email TEXT DEFAULT '',
    customer_name TEXT DEFAULT '',
    total_cents INTEGER NOT NULL,
    status TEXT DEFAULT 'pending',
    items_json TEXT DEFAULT '[]',
    shipping_address_json TEXT DEFAULT '{}',
    created_at TEXT DEFAULT (datetime('now'))
);

CREATE INDEX IF NOT EXISTS idx_orders_stripe ON orders(stripe_session_id);

-- Support
CREATE TABLE IF NOT EXISTS support_tickets (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    name TEXT NOT NULL,
    email TEXT NOT NULL,
    subject TEXT NOT NULL,
    message TEXT NOT NULL,
    status TEXT DEFAULT 'open',
    created_at TEXT DEFAULT (datetime('now'))
);
