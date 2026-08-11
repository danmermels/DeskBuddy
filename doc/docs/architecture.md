# DeskBuddy Architecture

## Overview

DeskBuddy is an ESP32-C3-based smart desk companion with a round GC9A01 display, mmWave presence sensor, AI coaching, task management, and a full e-commerce web platform.

```
┌────────────────────┐     ┌─────────────────────┐     ┌──────────────────┐
│   ESP32-C3 Device  │────▶│   Cloudflare Worker  │────▶│   Stripe Checkout │
│   (firmware)       │     │   (web/)             │     │   (payments)      │
├────────────────────┤     ├─────────────────────┤     └──────────────────┘
│ • Presence radar   │     │ • Landing page       │              │
│ • AI coach (Groq)  │     │ • Store (cart/check) │              ▼
│ • Task manager     │     │ • Admin dashboard    │     ┌──────────────────┐
│ • Clock faces      │     │ • Support tickets    │     │   D1 Database     │
│ • UDP beacon       │     │ • Companion dl       │     │   (SQLite)        │
│ • mDNS discovery   │     │ • Telemetry ingest   │     ├──────────────────┤
│ • WiFi / OTA       │     │ • R2 static files    │     │ • telemetry       │
└────────────────────┘     └─────────────────────┘     │ • products        │
         │                          │                  │ • orders          │
         ▼                          ▼                  │ • support_tickets │
┌────────────────────┐     ┌─────────────────────┐     │ • firmware_ver    │
│  Companion App     │     │   R2 Object Store    │     └──────────────────┘
│  (Tauri)           │     ├─────────────────────┤
├────────────────────┤     │ • Companion install  │
│ • System tray      │     │ • admin.html         │
│ • mDNS discovery   │     │ • store.js           │
│ • UDP beacon       │     └─────────────────────┘
│ • TCP health check │
│ • Win/Mac/Linux    │
└────────────────────┘
```

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
│       ├── worker.js        # Main entry — route dispatch (~50 lines)
│       ├── seed.js          # Product seed data
│       ├── routes/
│       │   ├── telemetry.js # POST /telemetry, GET /api/stats, /api/device
│       │   ├── store.js     # /store, /api/checkout, /webhook/stripe
│       │   ├── companion.js # /companion, /companion/download/*
│       │   ├── admin.js     # /admin, /api/admin/*
│       │   └── support.js   # /support, /api/support
│       └── pages/
│           ├── landing.js   # LANDING_PAGE() — EN/PT with IP geo-detection
│           ├── companion.js # COMPANION_PAGE()
│           ├── store.js     # STORE_PAGE() — product grid
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
└── docs/                    # Documentation
    ├── architecture.md      # This file
    └── web.md               # Cloudflare + Stripe stack guide
```

## ESP32 Firmware (`src/`)

### Hardware
- **MCU:** ESP32-C3 (RISC-V, 160MHz, 320KB RAM, 4MB Flash)
- **Display:** GC9A01 240×240 round TFT (SPI)
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
- **UI:** Pure system tray — no main window
- **Discovery:** Three parallel threads:
  1. **mDNS** — listens for `deskbuddy._http._tcp.local`
  2. **UDP beacon** — listens on port 42042
  3. **TCP health check** — probes port 80 on saved IP every 5s
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

See [web.md](web.md) for full Cloudflare + Stripe documentation.

## Data Flow

### Purchase Flow
```
Customer → /store → Add to Cart → /api/checkout → Stripe Checkout
                                                       ↓
Customer pays on Stripe ← Stripe hosted page ← redirect
         ↓
Stripe webhook → /webhook/stripe → DB: order.status = 'paid'
         ↓                          DB: stock decremented
DB: order.status = 'abandoned' (if expired)
DB: stock restored (if expired/failed)
```

### Telemetry Flow
```
ESP32 → POST /telemetry → D1 telemetry table
                              ↓
Admin → GET /api/stats → aggregated stats (auth required)
```

### Companion Discovery Flow
```
ESP32 broadcasts UDP → port 42042 → Companion tray app
ESP32 mDNS advertises → deskbuddy.local → Companion tray app
Companion TCP probes → port 80 on saved IP → Companion tray app
```

## Secrets & Configuration

| Secret | Where | Purpose |
|---|---|---|
| `ADMIN_PASSWORD` | Cloudflare Dashboard | Admin panel + telemetry API auth |
| `STRIPE_SECRET_KEY` | Cloudflare Dashboard | Stripe Checkout API |
| `STRIPE_WEBHOOK_SECRET` | Cloudflare Dashboard | Stripe webhook verification |
| `CLOUDFLARE_API_TOKEN` | GitHub Secrets | CI → R2 upload |
| `CLOUDFLARE_ACCOUNT_ID` | GitHub Secrets | CI → R2 upload |
