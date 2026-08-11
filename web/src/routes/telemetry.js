export async function handleTelemetry(request, env, path, url, corsHeaders) {
  // POST /telemetry — always open (ESP32 devices push data here)
  if (request.method === 'POST' && path === '/telemetry') {
    const data = await request.json();
    const required = ['chip_id', 'fw_ver', 'ts'];
    for (const field of required) {
      if (!data[field]) return Response.json({ ok: false, error: `missing field: ${field}` }, { status: 400 });
    }
    await env.DB.prepare(`
      INSERT INTO telemetry (chip_id, fw_ver, hw_rev, ts, uptime_h, boot_count, clock_face, ai_mode, ai_persona, temp_unit, time_24h, font_idx, daily_desk_h, daily_focus_h, daily_breaks, prod_score, daily_ai_requests, heap_free_kb, daily_task_active, daily_task_done, daily_task_overdue, monthly_task_active, monthly_task_done, monthly_task_overdue)
      VALUES (?1,?2,?3,?4,?5,?6,?7,?8,?9,?10,?11,?12,?13,?14,?15,?16,?17,?18,?19,?20,?21,?22,?23,?24)
    `).bind(data.chip_id, data.fw_ver, data.hw_rev||'esp32c3', data.ts, data.uptime_h||0, data.boot_count||0,
      data.clock_face, data.ai_mode, data.ai_persona, data.temp_unit, data.time_24h, data.font_idx,
      data.daily_desk_h||0, data.daily_focus_h||0, data.daily_breaks||0, data.prod_score||0, data.daily_ai_requests||0, data.heap_free_kb||0,
      data.daily_task_active||0, data.daily_task_done||0, data.daily_task_overdue||0,
      data.monthly_task_active||0, data.monthly_task_done||0, data.monthly_task_overdue||0).run();

    let updateInfo = { ok: true, update_available: false };
    const latest = await env.DB.prepare(`SELECT version, url, size, sha256, mandatory, release_notes FROM firmware_versions ORDER BY created_at DESC LIMIT 1`).first();
    if (latest && latest.version !== data.fw_ver) {
      updateInfo.update_available = true;
      updateInfo.firmware = { version:latest.version, url:latest.url, size:latest.size, sha256:latest.sha256, mandatory:!!latest.mandatory, notes:latest.release_notes||'' };
    }
    return Response.json(updateInfo, { headers: corsHeaders });
  }

  if (request.method === 'GET' && path === '/firmware/check') {
    const deviceVer = url.searchParams.get('ver') || '0.0.0';
    const latest = await env.DB.prepare(`SELECT version, url, size, sha256, mandatory, release_notes, min_fw_ver FROM firmware_versions WHERE version > ?1 ORDER BY created_at DESC LIMIT 1`).bind(deviceVer).first();
    if (!latest) return Response.json({ update_available: false }, { headers: corsHeaders });
    if (latest.min_fw_ver && deviceVer < latest.min_fw_ver) return Response.json({ update_available: false, reason: 'below_minimum' }, { headers: corsHeaders });
    return Response.json({ update_available: true, version:latest.version, url:latest.url, size:latest.size, sha256:latest.sha256, mandatory:!!latest.mandatory, notes:latest.release_notes||'' }, { headers: corsHeaders });
  }

  if (request.method === 'GET' && path === '/api/device') {
    if (!authorize(request, env)) return Response.json({ error: 'Unauthorized' }, { status: 401, headers: corsHeaders });
    const chipId = url.searchParams.get('chip_id');
    if (!chipId) return Response.json({ error:'missing chip_id' }, { status:400, headers:corsHeaders });
    const latest = await env.DB.prepare(`SELECT * FROM telemetry WHERE chip_id = ?1 ORDER BY ts DESC LIMIT 1`).bind(chipId).first();
    if (!latest) return Response.json({ error:'device not found' }, { status:404, headers:corsHeaders });
    const history24h = await env.DB.prepare(`SELECT ts, daily_desk_h, daily_focus_h, daily_breaks, prod_score, daily_task_active, daily_task_done, daily_task_overdue, monthly_task_active, monthly_task_done, monthly_task_overdue, uptime_h, boot_count FROM telemetry WHERE chip_id = ?1 AND ts > ?2 ORDER BY ts DESC LIMIT 24`).bind(chipId, Math.floor(Date.now()/1000)-86400).all();
    return Response.json({ device:{ chip_id:latest.chip_id, fw_ver:latest.fw_ver, hw_rev:latest.hw_rev, clock_face:latest.clock_face, ai_mode:latest.ai_mode, ai_persona:latest.ai_persona, temp_unit:latest.temp_unit, time_24h:latest.time_24h, font_idx:latest.font_idx, uptime_h:latest.uptime_h, boot_count:latest.boot_count, heap_free_kb:latest.heap_free_kb, last_seen:latest.ts }, latest_stats:{ daily_desk_h:latest.daily_desk_h, daily_focus_h:latest.daily_focus_h, daily_breaks:latest.daily_breaks, prod_score:latest.prod_score, daily_task_active:latest.daily_task_active, daily_task_done:latest.daily_task_done, daily_task_overdue:latest.daily_task_overdue, monthly_task_active:latest.monthly_task_active, monthly_task_done:latest.monthly_task_done, monthly_task_overdue:latest.monthly_task_overdue }, history_24h:history24h?.results||[] }, { headers:corsHeaders });
  }

  if (request.method === 'GET' && path === '/api/stats') {
    if (!authorize(request, env)) return Response.json({ error: 'Unauthorized' }, { status: 401, headers: corsHeaders });
    const days = parseInt(url.searchParams.get('days')||'30');
    const cutoff = Math.floor(Date.now()/1000) - days*86400;
    const [total, activeToday, versionDist, clockFaces, aiModes, avgMetrics, latestFw, deviceList, recent] = await Promise.all([
      env.DB.prepare('SELECT COUNT(DISTINCT chip_id) as devices FROM telemetry WHERE ts > ?1').bind(cutoff).first(),
      env.DB.prepare('SELECT COUNT(DISTINCT chip_id) as devices FROM telemetry WHERE ts > ?1').bind(Math.floor(Date.now()/1000)-86400).first(),
      env.DB.prepare('SELECT fw_ver, COUNT(DISTINCT chip_id) as count FROM telemetry WHERE ts > ?1 GROUP BY fw_ver ORDER BY count DESC').bind(cutoff).all(),
      env.DB.prepare('SELECT clock_face, COUNT(*) as count FROM telemetry WHERE ts > ?1 GROUP BY clock_face ORDER BY count DESC').bind(cutoff).all(),
      env.DB.prepare('SELECT ai_mode, COUNT(*) as count FROM telemetry WHERE ts > ?1 GROUP BY ai_mode ORDER BY ai_mode').bind(cutoff).all(),
      env.DB.prepare(`SELECT ROUND(AVG(daily_desk_h),1) as avg_desk_h, ROUND(AVG(daily_focus_h),1) as avg_focus_h, ROUND(AVG(daily_breaks),1) as avg_breaks, ROUND(AVG(prod_score),1) as avg_score, ROUND(AVG(uptime_h),1) as avg_uptime, ROUND(AVG(heap_free_kb),0) as avg_heap_kb, ROUND(AVG(daily_task_active),1) as avg_daily_active, ROUND(AVG(daily_task_done),1) as avg_daily_done, ROUND(AVG(daily_task_overdue),1) as avg_daily_overdue, ROUND(AVG(monthly_task_active),1) as avg_monthly_active, ROUND(AVG(monthly_task_done),1) as avg_monthly_done, ROUND(AVG(monthly_task_overdue),1) as avg_monthly_overdue FROM telemetry WHERE ts > ?1`).bind(cutoff).first(),
      env.DB.prepare('SELECT version, created_at FROM firmware_versions ORDER BY created_at DESC LIMIT 1').first(),
      env.DB.prepare('SELECT chip_id, fw_ver, MAX(ts) as last_seen FROM telemetry WHERE ts > ?1 GROUP BY chip_id ORDER BY last_seen DESC').bind(cutoff).all(),
      env.DB.prepare('SELECT chip_id, fw_ver, uptime_h, daily_desk_h, daily_breaks, prod_score, daily_task_done, daily_task_overdue, ts FROM telemetry ORDER BY ts DESC LIMIT 50').all()
    ]);
    return Response.json({ total_devices:total?.devices||0, active_today:activeToday?.devices||0, version_distribution:versionDist?.results||[], clock_faces:clockFaces?.results||[], ai_modes:aiModes?.results||[], avg_metrics:avgMetrics||{}, latest_firmware:latestFw||null, device_list:deviceList?.results||[], recent_entries:recent?.results||[] }, { headers:corsHeaders });
  }

  return null;
}

function authorize(request, env) {
  const auth = request.headers.get('Authorization');
  return auth && env.ADMIN_PASSWORD && auth === `Bearer ${env.ADMIN_PASSWORD}`;
}
