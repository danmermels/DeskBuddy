# DeskBuddy Architecture

## Overview

DeskBuddy is an ESP32-C3-based smart desk companion with a round GC9A01 display, mmWave presence sensor, AI coaching, task management, and a full e-commerce web platform.

## Repository Structure

```
DeskBuddy/
├── src/                     # ESP32-C3 firmware (C++)
│   ├── main.cpp             # Entry point, setup(), loop()
│   ├── Constants.h          # Timing thresholds, network config
│   ├── Display.h            # TFT display rendering, RLE decoder
│   ├── Faceplates.h         # Clock face presets
│   ├── Behaviour.h          # AI prompt templates, personas
│   ├── AI.h                 # FreeRTOS background AI query task
│   ├── Radar.h              # LD2410 mmWave sensor driver
│   ├── PresenceAnalysis.h   # Occupancy history, day rollover
│   ├── Web.h / Web_EN.h     # Inline web server (English)
│   ├── Web_PTBR.h           # Inline web server (Portuguese)
│   └── ...                  # MQTT, logging, stats, timer modules
│
├── web/                     # Cloudflare Worker (JavaScript)
│   ├── wrangler.toml        # Worker config, D1/R2 bindings
│   ├── schema.sql           # D1 database schema
│   ├── store.js             # Client-side cart JS (served from R2)
│   ├── admin.html           # Admin dashboard (served from R2)
│   ├── dashboard/
│   │   └── index.html       # Legacy telemetry dashboard (static)
│   └── src/
│       ├── worker.js        # Main entry - route dispatch (~50 lines)
│       ├── seed.js          # Product seed data
│       ├── routes/
│       │   ├── telemetry.js # POST /telemetry, GET /api/stats, /api/device
│       │   ├── store.js     # /store, /api/checkout, /webhook/stripe
│       │   ├── companion.js # /companion, /companion/download/*
│       │   ├── admin.js     # /admin, /api/admin/*
│       │   └── support.js   # /support, /api/support
│       └── pages/
│           ├── landing.js   # LANDING_PAGE() - EN/PT with IP geo-detection
│           ├── companion.js # COMPANION_PAGE()
│           ├── store.js     # STORE_PAGE() - product grid
│           ├── support.js   # SUPPORT_PAGE()
│           └── success.js   # STORE_SUCCESS_PAGE()
│
├── tools/
│   └── deskbuddy_companion/ # Desktop tray app (Tauri v2 + Rust)
│       ├── src-tauri/src/tray.rs  # Tray icon, mDNS, UDP beacon, TCP health
│       ├── package.json
│       └── ...
│
├── .github/workflows/
│   └── build-companion.yml  # CI: builds Win/Mac/Linux installers
│
└── doc/                     # Documentation
    ├── architecture.md      # This file
    └── web.md               # Cloudflare + Stripe stack guide
```

## ESP32 Firmware (`src/`)

### Hardware
- **MCU:** ESP32-C3 (RISC-V, 160MHz, 320KB RAM, 4MB Flash)
- **Display:** GC9A01 240x240 round TFT (SPI)
- **Sensor:** HLK-LD2410 mmWave radar (Serial, 256000 baud)

### Key Features
- **Presence detection:** mmWave radar tracks desk presence, focus sessions, breaks
- **AI coaching:** Groq/Gemini/DeepSeek API calls for motivational messages
- **Task manager:** Daily/monthly tasks with points, due dates, overdue tracking
- **Clock faces:** 6+ presets (Default, Minimalist, HiTech, Dev, Aviator, DeskBuddy character variants)
- **WiFi:** Station mode with static IP option, captive portal fallback (AP mode)
- **mDNS:** Registers as `deskbuddy._http._tcp.local` on port 80
- **UDP beacon:** Broadcasts `"deskbuddy"` to subnet broadcast on port 42042 every 30s
- **Web server:** Arduino WebServer on port 80, all routes defined inline in `Web_EN.h`/`Web_PTBR.h`
- **MQTT:** Publishes presence state to configurable broker
- **OTA:** Over-the-air firmware updates via ArduinoOTA
- **Telemetry:** Periodic POST to `/telemetry` with device stats

### Build
```bash
pio run            # Compile
pio run -t upload  # Compile + flash
```

## Desktop Companion App (`tools/deskbuddy_companion/`)

### Architecture
- **Framework:** Tauri v2 (Rust backend + minimal webview frontend)
- **UI:** Pure system tray - no main window
- **Discovery:** Three parallel threads:
  1. **mDNS** - listens for `deskbuddy._http._tcp.local`
  2. **UDP beacon** - listens on port 42042
  3. **TCP health check** - probes port 80 on saved IP every 5s
- **Config:** `%LOCALAPPDATA%\DeskBuddy Companion\settings.json`
- **Logs:** `%LOCALAPPDATA%\DeskBuddy Companion\companion.log`

### Build & Deploy
```bash
cd tools/deskbuddy_companion
npm install
npx tauri build           # Windows
# Cross-platform via GitHub Actions
```

CI builds all platforms (Windows `.exe`, macOS `.dmg` ARM + x64, Linux `.deb`) and auto-uploads to R2.

## Web Platform (`web/`)

### Architecture
- **Runtime:** Cloudflare Workers (serverless edge compute)
- **Database:** Cloudflare D1 (SQLite at the edge)
- **Storage:** Cloudflare R2 (object storage for downloads, admin.html, store.js)
- **Payments:** Stripe Checkout (hosted payment page)
- **Domain:** `deskbuddy.ca` (custom domain on worker)
- **Secrets:** Stripe keys, admin password bound via Cloudflare Secrets

See `doc/web.md` for full Cloudflare + Stripe documentation.

## Data Flows

### Purchase Flow
1. Customer visits `/store`, adds products to cart (localStorage)
2. Customer clicks Checkout -> `POST /api/checkout` with cart items
3. Worker validates stock, creates Stripe Checkout Session, saves order as `pending` in D1
4. Customer pays on Stripe's hosted page
5. Stripe sends webhook to `/webhook/stripe` -> order status updated to `paid`
6. If payment expires: webhook -> order status `abandoned`, stock restored
7. If payment fails: webhook -> order status `failed`, stock restored

### Telemetry Flow
1. ESP32 devices POST to `/telemetry` with device stats (open, no auth)
2. D1 stores each report in `telemetry` table
3. Admin panel or dashboard calls `/api/stats` (requires Bearer auth)
4. Returns aggregated stats: device counts, version distribution, clock face usage, averages

### Companion Discovery Flow
1. ESP32 broadcasts UDP beacon on port 42042 every 30 seconds
2. ESP32 advertises via mDNS as `deskbuddy._http._tcp.local`
3. Companion tray app listens for both signals
4. Companion also periodically TCP-probes port 80 on saved IP
5. Any match -> tray icon turns green -> left-click opens browser to dashboard

## URL Routes

| Route | Auth | Description |
|---|---|---|
| `/` | No | Landing page (EN/PT with geo-detection) |
| `/store` | No | Product catalog + cart |
| `/store.js` | No | Client-side cart JS (served from R2) |
| `/api/checkout` | No | Create Stripe Checkout Session |
| `/webhook/stripe` | No | Stripe payment event receiver |
| `/support` | No | Support ticket form |
| `/companion` | No | Companion app download page |
| `/companion/download/*` | No | Binary downloads (Win/Mac/Linux) |
| `/admin` | Yes* | Admin dashboard (login page open) |
| `/api/admin/*` | Yes | Product/order/ticket CRUD |
| `/telemetry` | No | ESP32 data ingest |
| `/firmware/check` | No | ESP32 OTA update check |
| `/api/stats` | Yes | Aggregated telemetry stats |
| `/api/device` | Yes | Per-device telemetry detail |

*Admin HTML loads from R2; login requires ADMIN_PASSWORD

## D1 Database Tables

| Table | Purpose |
|---|---|
| `telemetry` | ESP32 device reports (chip_id, fw_ver, stats, timestamps) |
| `firmware_versions` | OTA firmware versions (version, url, sha256, mandatory) |
| `products` | Store catalog (slug, name, price_cents, stock, active) |
| `orders` | Stripe orders (session_id, email, total, status, items_json) |
| `support_tickets` | Customer requests (name, email, subject, message) |

## Secrets

| Secret | Where | Used By |
|---|---|---|
| `ADMIN_PASSWORD` | Cloudflare | Admin panel + telemetry API auth |
| `STRIPE_SECRET_KEY` | Cloudflare | Stripe Checkout API calls |
| `STRIPE_WEBHOOK_SECRET` | Cloudflare | Webhook signature verification |
| `CLOUDFLARE_API_TOKEN` | GitHub | CI auto-upload to R2 |
| `CLOUDFLARE_ACCOUNT_ID` | GitHub | CI auto-upload to R2 |
