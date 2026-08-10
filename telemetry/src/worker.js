export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const path = url.pathname;

    const corsHeaders = {
      'Access-Control-Allow-Origin': '*',
      'Access-Control-Allow-Methods': 'GET, POST, OPTIONS',
      'Access-Control-Allow-Headers': 'Content-Type, Authorization',
    };

    if (request.method === 'OPTIONS') {
      return new Response(null, { status: 204, headers: corsHeaders });
    }

    // Telemetry ingest from ESP32 devices
    if (request.method === 'POST' && path === '/telemetry') {
      try {
        const data = await request.json();
        const required = ['chip_id', 'fw_ver', 'ts'];
        for (const field of required) {
          if (!data[field]) {
            return Response.json({ ok: false, error: `missing field: ${field}` }, { status: 400 });
          }
        }

        await env.DB.prepare(`
          INSERT INTO telemetry (chip_id, fw_ver, hw_rev, ts, uptime_h, boot_count,
            clock_face, ai_mode, ai_persona, temp_unit, time_24h, font_idx,
            daily_desk_h, daily_focus_h, daily_breaks, prod_score, daily_ai_requests, heap_free_kb,
            daily_task_active, daily_task_done, daily_task_overdue,
            monthly_task_active, monthly_task_done, monthly_task_overdue)
          VALUES (?1, ?2, ?3, ?4, ?5, ?6, ?7, ?8, ?9, ?10, ?11, ?12, ?13, ?14, ?15, ?16, ?17, ?18, ?19, ?20, ?21, ?22, ?23, ?24)
        `).bind(
          data.chip_id, data.fw_ver, data.hw_rev || 'esp32c3', data.ts,
          data.uptime_h || 0, data.boot_count || 0,
          data.clock_face, data.ai_mode, data.ai_persona, data.temp_unit, data.time_24h, data.font_idx,
          data.daily_desk_h || 0, data.daily_focus_h || 0, data.daily_breaks || 0,
          data.prod_score || 0, data.daily_ai_requests || 0, data.heap_free_kb || 0,
          data.daily_task_active || 0, data.daily_task_done || 0, data.daily_task_overdue || 0,
          data.monthly_task_active || 0, data.monthly_task_done || 0, data.monthly_task_overdue || 0
        ).run();

        let updateInfo = { ok: true, update_available: false };
        const latest = await env.DB.prepare(
          `SELECT version, url, size, sha256, mandatory, release_notes
           FROM firmware_versions ORDER BY created_at DESC LIMIT 1`
        ).first();

        if (latest && latest.version !== data.fw_ver) {
          updateInfo.update_available = true;
          updateInfo.firmware = {
            version: latest.version,
            url: latest.url,
            size: latest.size,
            sha256: latest.sha256,
            mandatory: !!latest.mandatory,
            notes: latest.release_notes || '',
          };
        }

        return Response.json(updateInfo, { headers: corsHeaders });
      } catch (e) {
        return Response.json({ ok: false, error: e.message }, { status: 500, headers: corsHeaders });
      }
    }

    // Firmware update check
    if (request.method === 'GET' && path === '/firmware/check') {
      try {
        const deviceVer = url.searchParams.get('ver') || '0.0.0';
        const latest = await env.DB.prepare(
          `SELECT version, url, size, sha256, mandatory, release_notes, min_fw_ver
           FROM firmware_versions WHERE version > ?1
           ORDER BY created_at DESC LIMIT 1`
        ).bind(deviceVer).first();

        if (!latest) {
          return Response.json({ update_available: false }, { headers: corsHeaders });
        }
        if (latest.min_fw_ver && deviceVer < latest.min_fw_ver) {
          return Response.json({ update_available: false, reason: 'below_minimum' }, { headers: corsHeaders });
        }
        return Response.json({
          update_available: true,
          version: latest.version,
          url: latest.url,
          size: latest.size,
          sha256: latest.sha256,
          mandatory: !!latest.mandatory,
          notes: latest.release_notes || '',
        }, { headers: corsHeaders });
      } catch (e) {
        return Response.json({ update_available: false, error: e.message }, { status: 500, headers: corsHeaders });
      }
    }

    // Per-device detail: latest entry + settings for one device
    if (request.method === 'GET' && path === '/api/device') {
      try {
        const chipId = url.searchParams.get('chip_id');
        if (!chipId) {
          return Response.json({ error: 'missing chip_id' }, { status: 400, headers: corsHeaders });
        }

        const latest = await env.DB.prepare(
          `SELECT * FROM telemetry WHERE chip_id = ?1 ORDER BY ts DESC LIMIT 1`
        ).bind(chipId).first();

        if (!latest) {
          return Response.json({ error: 'device not found' }, { status: 404, headers: corsHeaders });
        }

        const history24h = await env.DB.prepare(
          `SELECT ts, daily_desk_h, daily_focus_h, daily_breaks, prod_score,
                  daily_task_active, daily_task_done, daily_task_overdue,
                  monthly_task_active, monthly_task_done, monthly_task_overdue,
                  uptime_h, boot_count
           FROM telemetry WHERE chip_id = ?1 AND ts > ?2
           ORDER BY ts DESC LIMIT 24`
        ).bind(chipId, Math.floor(Date.now() / 1000) - 86400).all();

        return Response.json({
          device: {
            chip_id: latest.chip_id,
            fw_ver: latest.fw_ver,
            hw_rev: latest.hw_rev,
            clock_face: latest.clock_face,
            ai_mode: latest.ai_mode,
            ai_persona: latest.ai_persona,
            temp_unit: latest.temp_unit,
            time_24h: latest.time_24h,
            font_idx: latest.font_idx,
            uptime_h: latest.uptime_h,
            boot_count: latest.boot_count,
            heap_free_kb: latest.heap_free_kb,
            last_seen: latest.ts,
          },
          latest_stats: {
            daily_desk_h: latest.daily_desk_h,
            daily_focus_h: latest.daily_focus_h,
            daily_breaks: latest.daily_breaks,
            prod_score: latest.prod_score,
            daily_task_active: latest.daily_task_active,
            daily_task_done: latest.daily_task_done,
            daily_task_overdue: latest.daily_task_overdue,
            monthly_task_active: latest.monthly_task_active,
            monthly_task_done: latest.monthly_task_done,
            monthly_task_overdue: latest.monthly_task_overdue,
          },
          history_24h: history24h?.results || [],
        }, { headers: corsHeaders });
      } catch (e) {
        return Response.json({ error: e.message }, { status: 500, headers: corsHeaders });
      }
    }

    // Dashboard API: aggregated stats
    if (request.method === 'GET' && path === '/api/stats') {
      try {
        const days = parseInt(url.searchParams.get('days') || '30');
        const cutoff = Math.floor(Date.now() / 1000) - days * 86400;

        const total = await env.DB.prepare(
          `SELECT COUNT(DISTINCT chip_id) as devices FROM telemetry WHERE ts > ?1`
        ).bind(cutoff).first();

        const activeToday = await env.DB.prepare(
          `SELECT COUNT(DISTINCT chip_id) as devices FROM telemetry WHERE ts > ?1`
        ).bind(Math.floor(Date.now() / 1000) - 86400).first();

        const versionDist = await env.DB.prepare(
          `SELECT fw_ver, COUNT(DISTINCT chip_id) as count
           FROM telemetry WHERE ts > ?1 GROUP BY fw_ver ORDER BY count DESC`
        ).bind(cutoff).all();

        const clockFaces = await env.DB.prepare(
          `SELECT clock_face, COUNT(*) as count
           FROM telemetry WHERE ts > ?1 GROUP BY clock_face ORDER BY count DESC`
        ).bind(cutoff).all();

        const aiModes = await env.DB.prepare(
          `SELECT ai_mode, COUNT(*) as count
           FROM telemetry WHERE ts > ?1 GROUP BY ai_mode ORDER BY ai_mode`
        ).bind(cutoff).all();

        const avgMetrics = await env.DB.prepare(
          `SELECT
             ROUND(AVG(daily_desk_h), 1) as avg_desk_h,
             ROUND(AVG(daily_focus_h), 1) as avg_focus_h,
             ROUND(AVG(daily_breaks), 1) as avg_breaks,
             ROUND(AVG(prod_score), 1) as avg_score,
             ROUND(AVG(uptime_h), 1) as avg_uptime,
             ROUND(AVG(heap_free_kb), 0) as avg_heap_kb,
             ROUND(AVG(daily_task_active), 1) as avg_daily_active,
             ROUND(AVG(daily_task_done), 1) as avg_daily_done,
             ROUND(AVG(daily_task_overdue), 1) as avg_daily_overdue,
             ROUND(AVG(monthly_task_active), 1) as avg_monthly_active,
             ROUND(AVG(monthly_task_done), 1) as avg_monthly_done,
             ROUND(AVG(monthly_task_overdue), 1) as avg_monthly_overdue
           FROM telemetry WHERE ts > ?1`
        ).bind(cutoff).first();

        const latestFw = await env.DB.prepare(
          `SELECT version, created_at FROM firmware_versions ORDER BY created_at DESC LIMIT 1`
        ).first();

        const deviceList = await env.DB.prepare(
          `SELECT chip_id, fw_ver, MAX(ts) as last_seen
           FROM telemetry WHERE ts > ?1
           GROUP BY chip_id ORDER BY last_seen DESC`
        ).bind(cutoff).all();

        const recent = await env.DB.prepare(
          `SELECT chip_id, fw_ver, uptime_h, daily_desk_h, daily_breaks, prod_score,
                  daily_task_done, daily_task_overdue, ts
           FROM telemetry ORDER BY ts DESC LIMIT 50`
        ).all();

        return Response.json({
          total_devices: total?.devices || 0,
          active_today: activeToday?.devices || 0,
          version_distribution: versionDist?.results || [],
          clock_faces: clockFaces?.results || [],
          ai_modes: aiModes?.results || [],
          avg_metrics: avgMetrics || {},
          latest_firmware: latestFw || null,
          device_list: deviceList?.results || [],
          recent_entries: recent?.results || [],
        }, { headers: corsHeaders });
      } catch (e) {
        return Response.json({ error: e.message }, { status: 500, headers: corsHeaders });
      }
    }

    // Companion app download page
    if (request.method === 'GET' && path === '/companion') {
      const hasFile = await env.FIRMWARE.head('companion/deskbuddy-companion-setup.exe');
      return new Response(COMPANION_PAGE(hasFile ? '2.5 MB' : null, 'B303AFFC9D5DFBC2053237B483BA12FB34ECC8D84487612C45BAAD2F8105E9CB'), {
        headers: { 'Content-Type': 'text/html; charset=utf-8', ...corsHeaders },
      });
    }

    // Platform-specific downloads
    if (request.method === 'GET' && path.startsWith('/companion/download')) {
      const downloads = {
        '/companion/download/windows':    { key: 'companion/windows-setup.exe',    name: 'DeskBuddy_Companion_Setup.exe',    type: 'application/vnd.microsoft.portable-executable' },
        '/companion/download':            { key: 'companion/windows-setup.exe',    name: 'DeskBuddy_Companion_Setup.exe',    type: 'application/vnd.microsoft.portable-executable' },
        '/companion/download/mac-arm64':  { key: 'companion/mac-arm64.dmg',       name: 'DeskBuddy_Companion_arm64.dmg',     type: 'application/x-apple-diskimage' },
        '/companion/download/mac-x64':    { key: 'companion/mac-x64.dmg',         name: 'DeskBuddy_Companion_x64.dmg',       type: 'application/x-apple-diskimage' },
        '/companion/download/linux':      { key: 'companion/linux-amd64.deb',     name: 'DeskBuddy_Companion_amd64.deb',     type: 'application/vnd.debian.binary-package' },
      };
      const dl = downloads[path];
      if (dl) {
        const obj = await env.FIRMWARE.get(dl.key);
        if (obj) {
          return new Response(obj.body, {
            headers: { 'Content-Type': dl.type, 'Content-Disposition': `attachment; filename="${dl.name}"`, 'Content-Length': obj.size, ...corsHeaders },
          });
        }
      }
      return new Response('Not Found', { status: 404, headers: corsHeaders });
    }

    return new Response(LANDING_PAGE('2.5 MB', 'B303AFFC9D5DFBC2053237B483BA12FB34ECC8D84487612C45BAAD2F8105E9CB'), {
      headers: { 'Content-Type': 'text/html; charset=utf-8', ...corsHeaders },
    });
  },
};

function COMPANION_PAGE(fileSize, checksum) {
  const dl = (href, platform, size) =>
    `<a href="${href}" class="dl-btn">
      ${platform === 'Windows' ? '<svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor"><path d="M3 12V6.5l8-1.1v6.6H3zm0 1h8v6.6l-8-1.1V13zm9-7.3L21 3v9h-9V5.7zm0 13.6V13h9v9l-9-2.7z"/></svg>' : platform === 'macOS' ? '<svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor"><path d="M18.71 19.5c-.83 1.24-1.71 2.45-3.05 2.47-1.34.03-1.77-.79-3.29-.79-1.53 0-2 .77-3.27.82-1.31.05-2.3-1.32-3.14-2.53C4.25 17 2.94 12.45 4.7 9.39c.87-1.52 2.43-2.48 4.12-2.51 1.28-.02 2.5.87 3.29.87.78 0 2.26-1.07 3.8-.91.65.03 2.47.26 3.64 1.98-.09.06-2.17 1.28-2.15 3.81.03 3.02 2.65 4.03 2.68 4.04-.03.07-.42 1.44-1.38 2.83M13 3.5c.73-.83 1.94-1.46 2.94-1.5.13 1.17-.34 2.35-1.04 3.19-.69.85-1.83 1.51-2.95 1.42-.15-1.15.41-2.35 1.05-3.11z"/></svg>' : '<svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor"><path d="M20.6 17.7c-.4.9-.9 1.7-1.4 2.4-.8 1-1.5 1.7-2.3 1.7s-1.1-.3-2-.3c-.9 0-1.6.3-2 .3-.8 0-1.6-.7-2.5-1.9C9.5 18.5 8.7 16 9.1 14c.3-1.4 1.2-2.3 2.1-2.3.9 0 1.5.3 2 .3s1.3-.3 2.1-.3c.7 0 1.5.4 2.1 1 .1.1-1.3.8-1.3 2.5 0 2 1.7 2.7 1.8 2.7l-.1.3c-.3.7-.8 1.5-1.2 2.2zM16 3.2c-.7.8-1.5 1.5-2.6 1.5-.2-1.4.4-2.7 1.1-3.5.7-.9 2-1.5 3-1.5.1 1.3-.3 2.7-1.5 3.5z"/></svg>'}
      ${platform}${size ? ` <span class="btn-meta">${size}</span>` : ''}
    </a>`;

  const downloadBlock = fileSize
    ? `<div class="dl-grid">
        ${dl('/companion/download/windows', 'Windows', fileSize)}
        ${dl('/companion/download/mac-arm64', 'macOS Apple Silicon', '3.6 MB')}
        ${dl('/companion/download/mac-x64', 'macOS Intel', '3.7 MB')}
        ${dl('/companion/download/linux', 'Linux (.deb)', '3.9 MB')}
       </div>`
    : `<p class="soon">Installers coming soon. Build from <code>tools/deskbuddy_companion</code>.</p>`;
  const checksumBlock = checksum
    ? `<div class="checksum"><span>SHA-256:</span><code>${checksum}</code></div>`
    : '';
  return `<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>DeskBuddy Companion</title>
  <style>
    * { margin:0; padding:0; box-sizing:border-box; }
    body { font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif; background:#0f172a; color:#f8fafc; min-height:100vh; display:flex; align-items:center; justify-content:center; padding:20px; }
    .card { background:#1e293b; border:1px solid #334155; border-radius:16px; padding:40px; max-width:580px; width:100%; text-align:center; }
    .card .icon { font-size:3rem; margin-bottom:16px; }
    h1 { font-size:1.6rem; color:#38bdf8; margin-bottom:8px; }
    .subtitle { color:#94a3b8; font-size:0.92rem; line-height:1.5; margin-bottom:24px; }
    .steps { text-align:left; background:#0f172a; border-radius:10px; padding:20px 24px; margin-bottom:24px; }
    .steps h3 { color:#e2e8f0; font-size:0.85rem; margin-bottom:10px; text-transform:uppercase; letter-spacing:0.05em; }
    .steps ol { color:#94a3b8; font-size:0.88rem; padding-left:20px; }
    .steps ol li { margin-bottom:6px; }
    .dl-grid { display:grid; grid-template-columns:1fr 1fr; gap:10px; margin-bottom:16px; }
    @media(max-width:500px){.dl-grid{grid-template-columns:1fr}}
    .dl-btn { display:flex; align-items:center; gap:8px; background:#1e293b; border:1px solid #334155; color:#e2e8f0; padding:12px 14px; border-radius:10px; text-decoration:none; font-weight:600; font-size:0.85rem; transition:all .2s; text-align:left; line-height:1.3; }
    .dl-btn:hover { border-color:#38bdf8; background:#38bdf810; }
    .dl-btn svg { flex-shrink:0; opacity:0.6; }
    .btn-meta { display:block; font-size:0.68rem; font-weight:400; color:#64748b; }
    .soon { color:#64748b; font-style:italic; }
    code { background:#334155; padding:2px 6px; border-radius:4px; font-size:0.78rem; word-break:break-all; }
    .checksum { margin-bottom:16px; }
    .checksum span { color:#64748b; font-size:0.72rem; display:block; margin-bottom:4px; text-transform:uppercase; letter-spacing:0.05em; }
    .checksum code { display:block; padding:6px 10px; }
    .warn { text-align:left; background:#1a1a0a; border:1px solid #4a4a0a; border-radius:10px; padding:16px 20px; margin-bottom:20px; }
    .warn h4 { color:#facc15; font-size:0.8rem; margin-bottom:6px; text-transform:uppercase; letter-spacing:0.04em; }
    .warn p { color:#a3a35a; font-size:0.78rem; line-height:1.4; margin:0; }
    .footer { margin-top:20px; color:#475569; font-size:0.75rem; }
    .footer a { color:#64748b; }
  </style>
</head>
<body>
  <div class="card">
    <div class="icon">&#x1F4A1;</div>
    <h1>DeskBuddy Companion</h1>
    <p class="subtitle">One-click access to your DeskBuddy dashboard — right from your system tray. Discovers your device on the network automatically.</p>
    <div class="steps">
      <h3>Setup</h3>
      <ol>
        <li>Download and run the installer</li>
        <li>The DeskBuddy icon appears in your system tray</li>
        <li><strong>Green</strong> = device online. Click to open the dashboard.</li>
        <li><strong>Gray</strong> = searching. Make sure DeskBuddy is on the same Wi‑Fi.</li>
      </ol>
    </div>
    ${downloadBlock}
    ${checksumBlock}
    <div class="warn">
      <h4>&#x26A0; Security Note</h4>
      <p>The app is not code-signed. <strong>Windows</strong>: click "More info" then "Run anyway". <strong>macOS</strong>: right-click the app in Finder and select "Open". The app only connects to your local network and does not send data externally.</p>
    </div>
    <div class="footer">
      v1.0.0 &middot; <a href="/companion/download/windows">Windows</a> &middot; <a href="/companion/download/mac-arm64">Mac ARM</a> &middot; <a href="/companion/download/mac-x64">Mac Intel</a> &middot; <a href="/companion/download/linux">Linux</a>
    </div>
  </div>
</body>
</html>`;
}

function LANDING_PAGE(fileSize, checksum) {
  const svg = (platform) => {
    if (platform === 'win') return '<svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor"><path d="M3 12V6.5l8-1.1v6.6H3zm0 1h8v6.6l-8-1.1V13zm9-7.3L21 3v9h-9V5.7zm0 13.6V13h9v9l-9-2.7z"/></svg>';
    if (platform === 'mac') return '<svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor"><path d="M18.71 19.5c-.83 1.24-1.71 2.45-3.05 2.47-1.34.03-1.77-.79-3.29-.79-1.53 0-2 .77-3.27.82-1.31.05-2.3-1.32-3.14-2.53C4.25 17 2.94 12.45 4.7 9.39c.87-1.52 2.43-2.48 4.12-2.51 1.28-.02 2.5.87 3.29.87.78 0 2.26-1.07 3.8-.91.65.03 2.47.26 3.64 1.98-.09.06-2.17 1.28-2.15 3.81.03 3.02 2.65 4.03 2.68 4.04-.03.07-.42 1.44-1.38 2.83M13 3.5c.73-.83 1.94-1.46 2.94-1.5.13 1.17-.34 2.35-1.04 3.19-.69.85-1.83 1.51-2.95 1.42-.15-1.15.41-2.35 1.05-3.11z"/></svg>';
    return '<svg width="18" height="18" viewBox="0 0 24 24" fill="currentColor"><path d="M20.6 17.7c-.4.9-.9 1.7-1.4 2.4-.8 1-1.5 1.7-2.3 1.7s-1.1-.3-2-.3c-.9 0-1.6.3-2 .3-.8 0-1.6-.7-2.5-1.9C9.5 18.5 8.7 16 9.1 14c.3-1.4 1.2-2.3 2.1-2.3.9 0 1.5.3 2 .3s1.3-.3 2.1-.3c.7 0 1.5.4 2.1 1 .1.1-1.3.8-1.3 2.5 0 2 1.7 2.7 1.8 2.7l-.1.3c-.3.7-.8 1.5-1.2 2.2zM16 3.2c-.7.8-1.5 1.5-2.6 1.5-.2-1.4.4-2.7 1.1-3.5.7-.9 2-1.5 3-1.5.1 1.3-.3 2.7-1.5 3.5z"/></svg>';
  };
  const btn = (href, platform, label, size) =>
    `<a href="${href}" class="btn btn-dl">${svg(platform)}${label}<span class="btn-meta">${size}</span></a>`;

  const downloadSection = checksum
    ? `<div class="download-grid">${btn('/companion/download/windows','win','Windows',fileSize)}${btn('/companion/download/mac-arm64','mac','macOS ARM','3.6 MB')}${btn('/companion/download/mac-x64','mac','macOS Intel','3.7 MB')}${btn('/companion/download/linux','linux','Linux .deb','3.9 MB')}</div>
       <div class="checksum"><span>SHA-256 (Windows):</span><code>${checksum}</code></div>
       <p class="checksum-note">Verify checksums after downloading to confirm integrity.</p>`
    : `<div class="download-grid">${btn('#','mac','macOS','Coming soon')}${btn('#','linux','Linux','Coming soon')}</div>`;

  return `<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>DeskBuddy — Your Smart Desk Companion</title>
<style>
  *,*::before,*::after{margin:0;padding:0;box-sizing:border-box}
  body{font-family:-apple-system,BlinkMacSystemFont,'Segoe UI',Roboto,sans-serif;background:#0a0e17;color:#e2e8f0;overflow-x:hidden}
  .nav{display:flex;align-items:center;justify-content:space-between;padding:18px 40px;max-width:1200px;margin:0 auto}
  .nav-logo{font-size:1.3rem;font-weight:800;color:#38bdf8;text-decoration:none}
  .nav-links{display:flex;gap:28px}
  .nav-links a{color:#94a3b8;text-decoration:none;font-size:0.88rem;transition:color .15s}
  .nav-links a:hover{color:#e2e8f0}
  .hero{text-align:center;padding:80px 20px 60px;max-width:720px;margin:0 auto}
  .hero-badge{display:inline-block;background:#38bdf820;color:#38bdf8;padding:6px 16px;border-radius:20px;font-size:0.78rem;font-weight:600;margin-bottom:24px;border:1px solid #38bdf830}
  .hero h1{font-size:3.2rem;font-weight:800;letter-spacing:-0.02em;margin-bottom:16px;background:linear-gradient(135deg,#38bdf8,#818cf8);-webkit-background-clip:text;-webkit-text-fill-color:transparent;background-clip:text}
  .hero p{color:#64748b;font-size:1.15rem;line-height:1.7;max-width:560px;margin:0 auto 36px}
  .hero .actions{display:flex;gap:14px;justify-content:center;flex-wrap:wrap}
  .btn{display:inline-flex;align-items:center;gap:10px;padding:14px 28px;border-radius:10px;font-weight:700;font-size:0.95rem;text-decoration:none;transition:all .2s;cursor:pointer;border:none;text-align:left}
  .btn-primary{background:#38bdf8;color:#0a0e17;}
  .btn-primary:hover{background:#7dd3fc;transform:translateY(-1px);box-shadow:0 8px 25px #38bdf830}
  .btn-secondary{background:#1e293b;color:#e2e8f0;border:1px solid #334155}
  .btn-secondary:hover{background:#334155;transform:translateY(-1px)}
  .btn-disabled{background:#0f172a;color:#475569;border:1px solid #1e293b;cursor:not-allowed}
  .btn-dl{background:#111827;color:#e2e8f0;border:1px solid #1e293b;line-height:1.3}
  .btn-dl:hover{border-color:#38bdf8;background:#38bdf810;transform:translateY(-1px)}
  .btn-meta{display:block;font-size:0.7rem;font-weight:400;opacity:0.7;margin-top:2px}
  section{padding:80px 20px}
  .section-label{text-align:center;color:#38bdf8;font-size:0.78rem;font-weight:700;text-transform:uppercase;letter-spacing:0.1em;margin-bottom:12px}
  .section-title{text-align:center;font-size:2rem;font-weight:800;margin-bottom:16px}
  .section-sub{text-align:center;color:#64748b;font-size:0.95rem;max-width:500px;margin:0 auto 48px;line-height:1.6}
  .container{max-width:1100px;margin:0 auto}
  .features{display:grid;grid-template-columns:repeat(auto-fit,minmax(280px,1fr));gap:20px}
  .feature-card{background:#111827;border:1px solid #1e293b;border-radius:14px;padding:28px;transition:border-color .2s}
  .feature-card:hover{border-color:#38bdf840}
  .feature-icon{font-size:2rem;margin-bottom:16px}
  .feature-card h3{font-size:1.05rem;margin-bottom:8px;color:#f1f5f9}
  .feature-card p{color:#64748b;font-size:0.88rem;line-height:1.6}
  .how{background:#0d1117;border-top:1px solid #1e293b;border-bottom:1px solid #1e293b}
  .steps-row{display:grid;grid-template-columns:repeat(auto-fit,minmax(220px,1fr));gap:24px}
  .step{text-align:center;position:relative}
  .step-num{display:inline-flex;align-items:center;justify-content:center;width:44px;height:44px;border-radius:50%;background:#38bdf8;color:#0a0e17;font-weight:800;font-size:1.1rem;margin-bottom:14px}
  .step h4{font-size:0.95rem;margin-bottom:6px}
  .step p{color:#64748b;font-size:0.82rem;line-height:1.5}
  .download-section{text-align:center}
  .download-grid{display:flex;gap:14px;justify-content:center;flex-wrap:wrap;margin-bottom:16px}
  .checksum{display:inline-block;background:#111827;border:1px solid #1e293b;border-radius:8px;padding:10px 16px;margin-bottom:8px}
  .checksum span{color:#64748b;font-size:0.68rem;display:block;text-transform:uppercase;letter-spacing:0.08em;margin-bottom:4px}
  .checksum code{color:#38bdf8;font-size:0.72rem;word-break:break-all}
  .checksum-note{color:#475569;font-size:0.72rem}
  .pricing-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px}
  .price-card{background:#111827;border:1px solid #1e293b;border-radius:14px;padding:32px;text-align:center}
  .price-card.popular{border-color:#38bdf850;position:relative}
  .price-card.popular::before{content:'Most Popular';position:absolute;top:-11px;left:50%;transform:translateX(-50%);background:#38bdf8;color:#0a0e17;font-size:0.68rem;font-weight:700;padding:4px 14px;border-radius:10px;text-transform:uppercase;letter-spacing:0.04em}
  .price-name{font-size:0.85rem;color:#94a3b8;text-transform:uppercase;letter-spacing:0.06em;margin-bottom:12px}
  .price-amount{font-size:2.5rem;font-weight:800;margin-bottom:4px}
  .price-amount span{font-size:1rem;color:#64748b;font-weight:400}
  .price-desc{color:#64748b;font-size:0.82rem;margin-bottom:24px}
  .price-features{list-style:none;text-align:left;margin-bottom:24px}
  .price-features li{color:#94a3b8;font-size:0.84rem;padding:6px 0;border-bottom:1px solid #1e293b}
  .price-features li:last-child{border:none}
  .price-features li::before{content:'\\2713';color:#38bdf8;margin-right:8px;font-weight:700}
  .btn-buy{display:inline-block;width:100%;padding:12px;border-radius:8px;font-weight:700;font-size:0.9rem;text-decoration:none;transition:all .2s;cursor:not-allowed;opacity:0.5}
  .btn-buy.purple{background:#7c3aed;color:#fff}
  .btn-buy.frost{background:#38bdf8;color:#0a0e17}
  .coming-soon-badge{display:inline-block;background:#7c3aed20;color:#a78bfa;font-size:0.65rem;padding:2px 8px;border-radius:8px;text-transform:uppercase;letter-spacing:0.05em}
  .footer{text-align:center;padding:40px 20px;color:#475569;font-size:0.78rem;border-top:1px solid #1e293b}
  .footer a{color:#64748b}
  @media(max-width:640px){
    .nav{padding:16px 20px}
    .hero h1{font-size:2.2rem}
    .hero{padding:50px 20px 40px}
    section{padding:50px 20px}
    .section-title{font-size:1.5rem}
  }
</style>
</head>
<body>
<nav class="nav">
  <a href="/" class="nav-logo">DeskBuddy</a>
  <div class="nav-links">
    <a href="#features">Features</a>
    <a href="#download">Download</a>
    <a href="#pricing">Pricing</a>
    <a href="/companion">Companion App</a>
  </div>
</nav>

<section class="hero">
  <div class="hero-badge">&#x2022; Now Shipping v1.0</div>
  <h1>Your Desk. Smarter.</h1>
  <p>DeskBuddy tracks your presence, manages your tasks, and delivers AI-powered coaching — all from a beautiful circular display on your desk. Built for focus.</p>
  <div class="actions">
    <a href="#download" class="btn btn-primary">Get the Companion App</a>
    <a href="#features" class="btn btn-secondary">See How It Works</a>
  </div>
</section>

<section id="features">
  <div class="container">
    <div class="section-label">Features</div>
    <h2 class="section-title">Everything you need to stay focused</h2>
    <p class="section-sub">DeskBuddy combines hardware sensing, smart software, and AI to build better work habits.</p>
    <div class="features">
      <div class="feature-card">
        <div class="feature-icon">&#x1F4CD;</div>
        <h3>Presence Detection</h3>
        <p>mmWave radar detects when you're at your desk. Tracks desk time, focus sessions, and breaks automatically — no manual input needed.</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">&#x1F4CB;</div>
        <h3>Task & Agenda Manager</h3>
        <p>Built-in daily and monthly task lists with due dates, points, and overdue tracking. Manage everything from the web dashboard.</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">&#x1F9E0;</div>
        <h3>AI Productivity Coach</h3>
        <p>Connect Groq, Gemini, or DeepSeek for real-time motivational messages tailored to your work patterns. Choose from 4 coaching personas.</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">&#x23F0;</div>
        <h3>Customizable Clock Faces</h3>
        <p>Switch between analog, digital, minimalist, hi-tech, developer, and aviator faceplates. Your desk, your style.</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">&#x1F310;</div>
        <h3>Web Dashboard</h3>
        <p>Full control from any browser on your network. View stats, configure settings, manage files, and run timers — all over WiFi.</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">&#x1F4E1;</div>
        <h3>MQTT & Telemetry</h3>
        <p>Publish presence data to your MQTT broker. Integrate with Home Assistant, Node-RED, or your own automation stack.</p>
      </div>
    </div>
  </div>
</section>

<section class="how">
  <div class="container">
    <div class="section-label">Setup</div>
    <h2 class="section-title">Three minutes to smarter work</h2>
    <div class="steps-row">
      <div class="step">
        <div class="step-num">1</div>
        <h4>Place on Desk</h4>
        <p>Position the DeskBuddy facing you. The mmWave sensor calibrates automatically.</p>
      </div>
      <div class="step">
        <div class="step-num">2</div>
        <h4>Connect to WiFi</h4>
        <p>Join your network via captive portal or configure static IP. mDNS makes discovery automatic.</p>
      </div>
      <div class="step">
        <div class="step-num">3</div>
        <h4>Open Dashboard</h4>
        <p>Install the companion app or visit deskbuddy.local. Your stats, tasks, and AI coach are ready.</p>
      </div>
      <div class="step">
        <div class="step-num">4</div>
        <h4>Stay Focused</h4>
        <p>Let DeskBuddy track your time, surface overdue tasks, and keep you motivated throughout the day.</p>
      </div>
    </div>
  </div>
</section>

<section id="download" class="download-section">
  <div class="container">
    <div class="section-label">Desktop App</div>
    <h2 class="section-title">One click to your dashboard</h2>
    <p class="section-sub">The DeskBuddy Companion lives in your system tray, auto-discovers your device, and opens the dashboard instantly.</p>
    ${downloadSection}
    <p style="color:#64748b;font-size:0.78rem;margin-top:12px">No registration required. Companion app connects locally only.</p>
  </div>
</section>

<section id="pricing">
  <div class="container">
    <div class="section-label">Pricing</div>
    <h2 class="section-title">Choose your DeskBuddy</h2>
    <p class="section-sub" style="margin-bottom:32px"><span class="coming-soon-badge">Coming Soon</span>&ensp;Online ordering is being built. Join the waitlist below.</p>
    <div class="pricing-grid">
      <div class="price-card">
        <div class="price-name">DeskBuddy Kit</div>
        <div class="price-amount">$XX<span>.00</span></div>
        <div class="price-desc">ESP32-C3 + GC9A01 display + mmWave sensor</div>
        <ul class="price-features">
          <li>GC9A01 240×240 circular IPS display</li>
          <li>ESP32-C3 RISC-V microcontroller</li>
          <li>HLK-LD2410 mmWave presence sensor</li>
          <li>All clock faces included</li>
          <li>Web dashboard & WiFi provisioning</li>
          <li>DeskBuddy Companion tray app</li>
        </ul>
        <span class="btn btn-buy frost">Available Soon</span>
      </div>
      <div class="price-card popular">
        <div class="price-name">DeskBuddy + AI Bundle</div>
        <div class="price-amount">$XX<span>.00</span></div>
        <div class="price-desc">Everything in Kit, plus AI coaching & cloud telemetry</div>
        <ul class="price-features">
          <li>Everything in DeskBuddy Kit</li>
          <li>AI productivity coach (Groq / Gemini / DeepSeek)</li>
          <li>4 coaching personas</li>
          <li>Cloud telemetry dashboard</li>
          <li>Over-the-air firmware updates</li>
          <li>Priority email support</li>
        </ul>
        <span class="btn btn-buy purple">Available Soon</span>
      </div>
      <div class="price-card">
        <div class="price-name">DeskBuddy Pro</div>
        <div class="price-amount">$XX<span>.00</span></div>
        <div class="price-desc">For teams and power users</div>
        <ul class="price-features">
          <li>Everything in AI Bundle</li>
          <li>MQTT integration (Home Assistant ready)</li>
          <li>REST API for custom integrations</li>
          <li>Multi-device dashboard</li>
          <li>Custom faceplate designer</li>
          <li>Dedicated support channel</li>
        </ul>
        <span class="btn btn-buy purple">Available Soon</span>
      </div>
    </div>
    <p style="text-align:center;color:#475569;font-size:0.78rem;margin-top:28px">Free shipping worldwide. 30-day return policy.</p>
  </div>
</section>

<footer class="footer">
  DeskBuddy &copy; 2026 &middot; Built with ESP32-C3, GC9A01, and LD2410 &middot; 
  <a href="/companion">Companion App</a> &middot; 
  <a href="/api/stats">Telemetry</a>
</footer>
</body>
</html>`;
}
