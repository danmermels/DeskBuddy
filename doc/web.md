# Web Platform — Cloudflare + Stripe Stack

## Overview

The web platform runs on a single **Cloudflare Worker** at `deskbuddy.ca`. It handles the landing page, store, admin dashboard, support tickets, companion app downloads, telemetry ingestion, and Stripe payment processing. Everything fits in Cloudflare's free tier.

## Tech Stack

| Service | Purpose | Free Tier Limit |
|---|---|---|
| **Cloudflare Workers** | Serverless runtime | 100K requests/day |
| **Cloudflare D1** | SQLite database | 5GB / 5M reads/mo |
| **Cloudflare R2** | Object storage | 10GB / 1M ops/mo |
| **Stripe Checkout** | Payment processing | No monthly fee (2.9%+txn) |
| **Chart.js** | Admin telemetry charts | CDN |

## Directory Structure

```
web/
├── wrangler.toml          # Worker config (D1/R2 bindings, compatibility)
├── schema.sql             # D1 database tables
├── store.js               # Client-side cart JS → uploaded to R2
├── admin.html             # Admin dashboard → uploaded to R2
├── dashboard/
│   └── index.html         # Standalone telemetry dashboard (static SPA)
└── src/
    ├── worker.js           # Main entry — ES module, route dispatch
    ├── seed.js             # Seed product data for first-run
    ├── routes/             # Route handlers (returns Response or null)
    │   ├── telemetry.js    # Device telemetry + firmware updates
    │   ├── store.js        # Store catalog + cart + Stripe checkout + webhook
    │   ├── companion.js    # Companion app download page + file serving
    │   ├── admin.js        # Admin dashboard + CRUD API
    │   └── support.js      # Support ticket form + API
    └── pages/              # HTML template functions
        ├── landing.js      # Landing page (EN/PT with IP geo-detection)
        ├── companion.js    # Companion app download page
        ├── store.js        # Store product grid
        ├── support.js      # Support ticket form
        └── success.js      # Post-purchase confirmation
```

## Worker Architecture

The main `worker.js` is ~50 lines. It uses ES module imports and a dispatch loop:

```javascript
import { handleTelemetry } from './routes/telemetry.js';
import { handleStore } from './routes/store.js';
import { handleCompanion } from './routes/companion.js';
import { handleSupport } from './routes/support.js';
import { handleAdmin } from './routes/admin.js';
import { LANDING_PAGE } from './pages/landing.js';

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const path = url.pathname;

    // CORS
    if (request.method === 'OPTIONS') return new Response(null, { status: 204, headers: corsHeaders });

    // Dispatch to route handlers — each returns Response if matched, null if skipped
    const handlers = [handleTelemetry, handleStore, handleCompanion, handleSupport, handleAdmin];
    for (const handler of handlers) {
      const response = await handler(request, env, path, url, corsHeaders);
      if (response) return response;
    }

    // Catch-all: landing page with geo-detection
    const country = request.cf?.country || '';
    const lang = url.searchParams.get('lang') || (country === 'BR' ? 'pt' : 'en');
    return new Response(LANDING_PAGE('2.5 MB', '...', lang), {
      headers: { 'Content-Type': 'text/html; charset=utf-8', ...corsHeaders },
    });
  },
};
```

### Route Handler Pattern

Each handler exports an async function with this signature:

```javascript
export async function handleXxx(request, env, path, url, corsHeaders) {
  // If this handler matches the route, return a Response
  // Otherwise, return null (next handler runs)
}
```

## URL Routes

| Route | Method | Handler | Auth | Description |
|---|---|---|---|---|
| `/` | GET | landing.js | No | Landing page (EN/PT auto) |
| `/store` | GET | store.js | No | Product catalog with cart |
| `/store.js` | GET | store.js (R2) | No | Client-side cart JS |
| `/store/success` | GET | store.js | No | Post-purchase page |
| `/api/products` | GET | store.js | No | Product JSON |
| `/api/checkout` | POST | store.js | No | Create Stripe session |
| `/webhook/stripe` | POST | store.js | No | Stripe payment hooks |
| `/support` | GET | support.js | No | Support ticket form |
| `/api/support` | POST | support.js | No | Submit ticket |
| `/companion` | GET | companion.js | No | Download page |
| `/companion/download/:platform` | GET | companion.js | No | Binary downloads |
| `/admin` | GET | admin.js (R2) | No* | Admin login page |
| `/admin/auth` | POST | admin.js | No | Authenticate |
| `/api/admin/*` | * | admin.js | Yes | Admin CRUD |
| `/telemetry` | POST | telemetry.js | No | ESP32 data ingest |
| `/firmware/check` | GET | telemetry.js | No | ESP32 update check |
| `/api/stats` | GET | telemetry.js | Yes** | Aggregated stats |
| `/api/device` | GET | telemetry.js | Yes** | Per-device detail |

\* Admin HTML loads from R2, no auth required to view login page
\** Protected by `Bearer <ADMIN_PASSWORD>` — same token as admin panel

## D1 Database Schema

### tables

```sql
telemetry        — ESP32 device reports (chip_id, fw_ver, stats, timestamps)
firmware_versions — OTA firmware versions (version, url, sha256, mandatory)
products         — Store products (slug, name, price_cents, stock, active)
orders           — Stripe orders (session_id, email, total, status, items_json)
support_tickets  — Customer support requests (name, email, subject, message)
```

### Stock Tracking

- `stock` column: `NULL` = unlimited, integer = remaining count
- Stock decremented on checkout (pending payment)
- Stock restored on webhook events: `checkout.session.expired` or `async_payment_failed`
- Stock stays decremented on `checkout.session.completed` (paid)

## Stripe Integration

### Checkout Flow

1. Customer clicks "Checkout" → `POST /api/checkout` with cart items
2. Worker validates stock, creates Stripe Checkout Session via API
3. Returns `session.url` → browser redirects to Stripe hosted payment page
4. Customer completes payment (card, PIX, boleto, etc.)
5. Stripe POSTs to `/webhook/stripe` → worker updates order status in D1

### Webhook Events Handled

| Event | Action |
|---|---|
| `checkout.session.completed` | Order → `paid`, stock stays decremented |
| `checkout.session.expired` | Order → `abandoned`, stock restored |
| `checkout.session.async_payment_failed` | Order → `failed`, stock restored |

### Supported Countries

Stripe Checkout collects shipping address for: Canada (`CA`), United States (`US`), Brazil (`BR`).

### Test Mode vs Live

- `sk_test_` key → sandbox, no real charges
- `sk_live_` key → production, real money moves
- Switch keys in Cloudflare Dashboard → `deskbuddy-api` → Settings → Variables → Secrets → `STRIPE_SECRET_KEY`

## R2 Object Store

### Objects

| Key | Content | Served At |
|---|---|---|
| `companion/windows-setup.exe` | Windows installer | `/companion/download/windows` |
| `companion/mac-arm64.dmg` | macOS Apple Silicon | `/companion/download/mac-arm64` |
| `companion/mac-x64.dmg` | macOS Intel | `/companion/download/mac-x64` |
| `companion/linux-amd64.deb` | Linux .deb | `/companion/download/linux` |
| `companion/admin.html` | Admin dashboard | `/admin` |
| `companion/store.js` | Client-side cart JS | `/store.js` |

### Uploading Files

```bash
cd web
npx wrangler r2 object put deskbuddy-firmware/companion/admin.html --file admin.html --remote
```

## Secrets Management

Secrets are stored in Cloudflare and accessed via `env.SECRET_NAME` in the worker.

### Adding a Secret

**Via Dashboard** (recommended):
1. Cloudflare Dashboard → Workers & Pages → `deskbuddy-api`
2. Settings → Variables → Secrets → Add
3. Save and Deploy

**Via CLI** (from `web/`):
```bash
npx wrangler secret put SECRET_NAME
```
Then redeploy:
```bash
npx wrangler deploy
```

### Required Secrets

| Secret | Required For |
|---|---|
| `ADMIN_PASSWORD` | Admin panel login + telemetry API auth |
| `STRIPE_SECRET_KEY` | Payment processing (`sk_test_` or `sk_live_`) |
| `STRIPE_WEBHOOK_SECRET` | Stripe webhook signature verification |

## Deploying

### Deploy Worker

```bash
cd web
npx wrangler deploy
```

Wrangler bundles ES module imports automatically. No build step needed.

### Deploy Static Files to R2

```bash
# Update admin dashboard
npx wrangler r2 object put deskbuddy-firmware/companion/admin.html --file admin.html --remote

# Update store JS
npx wrangler r2 object put deskbuddy-firmware/companion/store.js --file store.js --remote
```

### Deploy D1 Schema

```bash
npx wrangler d1 execute deskbuddy-db --file=schema.sql --remote
```

### Run D1 Queries

```bash
# Query
npx wrangler d1 execute deskbuddy-db --command="SELECT * FROM products" --remote

# Update
npx wrangler d1 execute deskbuddy-db --command="UPDATE products SET stock=10 WHERE slug='deskbuddy-kit'" --remote
```

### List Secrets

```bash
npx wrangler secret list
```

## CI/CD

The companion app is built by GitHub Actions (`.github/workflows/build-companion.yml`). It builds Windows, macOS (ARM + x64), and Linux installers and auto-uploads to R2.

Required GitHub Secrets for CI:
- `CLOUDFLARE_API_TOKEN` — with R2 write access
- `CLOUDFLARE_ACCOUNT_ID` — from Cloudflare Dashboard

## Custom Domain

`deskbuddy.ca` is configured as a custom domain on the worker:
1. Domain added to Cloudflare (Cloudflare manages DNS)
2. Worker custom domain added via Dashboard → Workers & Pages → `deskbuddy-api` → Custom Domains
3. SSL certificate auto-provisioned by Cloudflare

## Language Support

The landing page serves English or Portuguese based on:
1. `?lang=en` or `?lang=pt` URL parameter (takes priority)
2. `request.cf.country === 'BR'` → Portuguese, otherwise English
3. Language switcher in nav saves preference to `localStorage`

## Inventory Management

Products have an optional `stock` column (NULL = unlimited). Stock is:
- Decremented when order is created (pending)
- Restored if order expires or payment fails
- Kept decremented on successful payment

Admin panel shows stock levels. Store page shows "Out of Stock" badges and disables Sold Out buttons.

## Admin Panel

Access at `/admin`. Password-protected via `ADMIN_PASSWORD` secret.

**Tabs:**
- **Products** — CRUD, toggle active/hidden, set stock levels, reorder
- **Orders** — view all orders with status (pending/paid/abandoned/failed)
- **Tickets** — customer support requests from `/support`
- **Telemetry** — KPI cards, Chart.js charts (version distribution, clock faces), device list with detail modal

## Troubleshooting

### Worker not picking up secrets

After setting secrets via Dashboard, redeploy with `npx wrangler deploy` from `web/`.

### Checkout returns "Store not configured"

Ensure `STRIPE_SECRET_KEY` is set in Cloudflare Secrets.

### Admin shows "Admin Not Configured"

Upload `admin.html` to R2 or set `ADMIN_PASSWORD` secret.

### D1 tables missing

```bash
cd web
npx wrangler d1 execute deskbuddy-db --file=schema.sql --remote
```

### Telemetry dashboard charts broken

Update `admin.html` in R2 and redeploy worker.

### Stripe webhook not receiving events

1. Verify webhook URL in Stripe Dashboard: `https://deskbuddy.ca/webhook/stripe`
2. Check events subscribed: `checkout.session.completed`, `checkout.session.expired`, `checkout.session.async_payment_failed`
3. Set `STRIPE_WEBHOOK_SECRET` in Cloudflare secrets

### Check wrangler version

```bash
npx wrangler --version
```
