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

    // ─── Store ────────────────────────────────────
    if (request.method === 'GET' && path === '/store') {
      let products = await env.DB.prepare('SELECT * FROM products WHERE active=1 ORDER BY sort_order').all();
      if (!products.results || products.results.length === 0) {
        await seedProducts(env.DB);
        products = await env.DB.prepare('SELECT * FROM products WHERE active=1 ORDER BY sort_order').all();
      }
      return new Response(STORE_PAGE(products.results || []), { headers: { 'Content-Type': 'text/html; charset=utf-8', ...corsHeaders } });
    }

    if (request.method === 'GET' && path === '/api/products') {
      const products = await env.DB.prepare('SELECT * FROM products WHERE active=1 ORDER BY sort_order').all();
      return Response.json(products.results || [], { headers: corsHeaders });
    }

    if (request.method === 'POST' && path === '/api/checkout') {
      try {
        const stripeKey = env.STRIPE_SECRET_KEY;
        if (!stripeKey) return Response.json({ error: 'Store not configured yet' }, { status: 503, headers: corsHeaders });
        const body = await request.json();
        if (!body.items || !Array.isArray(body.items) || body.items.length === 0) {
          return Response.json({ error: 'No items in cart' }, { status: 400, headers: corsHeaders });
        }
        const line_items = [];
        for (const item of body.items) {
          const product = await env.DB.prepare('SELECT * FROM products WHERE slug=?1 AND active=1').bind(item.slug).first();
          if (!product) continue;
          line_items.push({ price_data: { currency:'cad', product_data:{name:product.name}, unit_amount:product.price_cents }, quantity:Math.min(item.qty||1,99) });
        }
        if (line_items.length === 0) return Response.json({ error:'No valid items found' }, { status:400, headers:corsHeaders });
        const params = new URLSearchParams();
        params.set('mode', 'payment');
        params.set('success_url', `${url.origin}/store/success?session_id={CHECKOUT_SESSION_ID}`);
        params.set('cancel_url', `${url.origin}/store`);
        params.set('shipping_address_collection[allowed_countries][0]', 'CA');
        params.set('shipping_address_collection[allowed_countries][1]', 'US');
        params.set('shipping_address_collection[allowed_countries][2]', 'BR');
        line_items.forEach((item, i) => {
          params.set(`line_items[${i}][price_data][currency]`, 'cad');
          params.set(`line_items[${i}][price_data][product_data][name]`, item.price_data.product_data.name);
          params.set(`line_items[${i}][price_data][unit_amount]`, item.price_data.unit_amount);
          params.set(`line_items[${i}][quantity]`, item.quantity);
        });
        if (body.email) params.set('customer_email', body.email);
        const stripeResp = await fetch('https://api.stripe.com/v1/checkout/sessions', {
          method:'POST',
          headers:{ Authorization:`Bearer ${stripeKey}`, 'Content-Type':'application/x-www-form-urlencoded' },
          body: params.toString()
        });
        const session = await stripeResp.json();
        if (session.url) {
          await env.DB.prepare('INSERT INTO orders (stripe_session_id, customer_email, total_cents, items_json, status) VALUES (?1,?2,?3,?4,?)')
            .bind(session.id, body.email||'', line_items.reduce((s,i)=>s+i.price_data.unit_amount*i.quantity,0), JSON.stringify(line_items), 'pending').run();
          return Response.json({ url:session.url }, { headers:corsHeaders });
        }
        return Response.json({ error:session.error?.message||'Stripe error' }, { status:400, headers:corsHeaders });
      } catch(e) { return Response.json({ error:e.message }, { status:500, headers:corsHeaders }); }
    }

    if (request.method === 'GET' && path === '/store/success') {
      return new Response(STORE_SUCCESS_PAGE(), { headers:{ 'Content-Type':'text/html;charset=utf-8', ...corsHeaders }});
    }

    if (request.method === 'POST' && path === '/webhook/stripe') {
      try {
        if (!env.STRIPE_SECRET_KEY) return new Response(null, { status:200 });
        const event = await request.json();
        const s = event.data.object;
        if (event.type === 'checkout.session.completed') {
          await env.DB.prepare("UPDATE orders SET status='paid', customer_email=?2 WHERE stripe_session_id=?1")
            .bind(s.id, s.customer_details?.email||'').run();
        } else if (event.type === 'checkout.session.expired') {
          await env.DB.prepare("UPDATE orders SET status='abandoned' WHERE stripe_session_id=?1").bind(s.id).run();
        } else if (event.type === 'checkout.session.async_payment_failed') {
          await env.DB.prepare("UPDATE orders SET status='failed' WHERE stripe_session_id=?1").bind(s.id).run();
        }
        return Response.json({ received:true }, { headers:corsHeaders });
      } catch(e) { return Response.json({ error:e.message }, { status:400, headers:corsHeaders }); }
    }

    // ─── Support ─────────────────────────────────
    if (request.method === 'GET' && path === '/support') {
      return new Response(SUPPORT_PAGE(), { headers:{ 'Content-Type':'text/html;charset=utf-8', ...corsHeaders }});
    }

    if (request.method === 'POST' && path === '/api/support') {
      try {
        const { name, email, subject, message } = await request.json();
        if (!name || !email || !subject || !message) {
          return Response.json({ ok:false, error:'All fields required' }, { status:400, headers:corsHeaders });
        }
        await env.DB.prepare('INSERT INTO support_tickets (name, email, subject, message) VALUES (?1,?2,?3,?4)')
          .bind(name, email, subject, message).run();
        return Response.json({ ok:true }, { headers:corsHeaders });
      } catch(e) { return Response.json({ ok:false, error:e.message }, { status:500, headers:corsHeaders }); }
    }

    // ─── Admin ───────────────────────────────────
    if (request.method === 'GET' && path === '/admin') {
      const obj = await env.FIRMWARE.get('companion/admin.html');
      if (!obj) return new Response(ADMIN_SETUP_HTML(), { headers:{'Content-Type':'text/html;charset=utf-8',...corsHeaders}});
      return new Response(obj.body, { headers:{ 'Content-Type':'text/html; charset=utf-8',...corsHeaders }});
    }

    if (request.method === 'POST' && path === '/admin/auth') {
      const { password } = await request.json();
      return Response.json({ ok: password && password === env.ADMIN_PASSWORD }, { headers:corsHeaders });
    }

    // Admin API (all auth-protected)
    if (path.startsWith('/api/admin/')) {
      const auth = request.headers.get('Authorization');
      if (!auth || auth !== `Bearer ${env.ADMIN_PASSWORD}`) return Response.json({error:'Unauthorized'},{status:401,headers:corsHeaders});

      if (path === '/api/admin/products' && request.method === 'GET') {
        const p = await env.DB.prepare('SELECT * FROM products ORDER BY sort_order').all();
        return Response.json({ products: p.results || [] }, { headers:corsHeaders });
      }
      if (path === '/api/admin/products' && request.method === 'POST') {
        const { name, description, price_cents, slug } = await request.json();
        const s = slug || name.toLowerCase().replace(/[^a-z0-9]+/g,'-').replace(/^-|-$/g,'');
        await env.DB.prepare('INSERT OR REPLACE INTO products (slug,name,description,price_cents,active,sort_order) VALUES (?1,?2,?3,?4,1,COALESCE((SELECT MAX(sort_order)+1 FROM products),0))')
          .bind(s, name, description||'', parseInt(price_cents)||0).run();
        return Response.json({ ok:true }, { headers:corsHeaders });
      }
      if (path === '/api/admin/products' && request.method === 'PUT') {
        const body = await request.json();
        if (body.id) {
          const sets = []; const vals = [];
          if (body.name) { sets.push('name=?'); vals.push(body.name); }
          if (body.description !== undefined) { sets.push('description=?'); vals.push(body.description); }
          if (body.price_cents !== undefined) { sets.push('price_cents=?'); vals.push(parseInt(body.price_cents)); }
          if (body.active !== undefined) { sets.push('active=?'); vals.push(body.active ? 1 : 0); }
          if (body.sort_order !== undefined) { sets.push('sort_order=?'); vals.push(parseInt(body.sort_order)); }
          if (sets.length > 0) { vals.push(body.id); await env.DB.prepare(`UPDATE products SET ${sets.join(',')} WHERE id=?`).bind(...vals).run(); }
        }
        return Response.json({ ok:true }, { headers:corsHeaders });
      }
      if (path === '/api/admin/products' && request.method === 'DELETE') {
        const id = url.searchParams.get('id');
        if (id) await env.DB.prepare('DELETE FROM products WHERE id=?1').bind(parseInt(id)).run();
        return Response.json({ ok:true }, { headers:corsHeaders });
      }
      if (path === '/api/admin/orders') {
        const o = await env.DB.prepare('SELECT * FROM orders ORDER BY created_at DESC LIMIT 50').all();
        return Response.json({ orders: o.results || [] }, { headers:corsHeaders });
      }
      if (path === '/api/admin/tickets') {
        const t = await env.DB.prepare('SELECT * FROM support_tickets ORDER BY created_at DESC LIMIT 50').all();
        return Response.json({ tickets: t.results || [] }, { headers:corsHeaders });
      }
      return Response.json({ error:'Not found' }, { status:404, headers:corsHeaders });
    }

    const country = (request.cf && request.cf.country) || '';
    const landingLang = country === 'BR' ? 'pt' : 'en';
    return new Response(LANDING_PAGE('2.5 MB', 'B303AFFC9D5DFBC2053237B483BA12FB34ECC8D84487612C45BAAD2F8105E9CB', landingLang), {
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

function LANDING_PAGE(fileSize, checksum, lang) {
  const t = lang === 'pt'
    ? { hero: 'Sua Mesa. Mais Inteligente.', sub: 'O DeskBuddy monitora sua presen\u00e7a, gerencia suas tarefas e oferece coaching com IA \u2014 tudo em uma tela circular na sua mesa.', cta1: 'Baixar App', cta2: 'Como Funciona',
        featLabel: 'Recursos', featTitle: 'Tudo para manter o foco', featSub: 'O DeskBuddy combina sensores, software inteligente e IA para criar melhores h\u00e1bitos.',
        f1: 'Detec\u00e7\u00e3o de Presen\u00e7a', f1d: 'Radar mmWave detecta quando voc\u00ea est\u00e1 na mesa. Monitora tempo, sess\u00f5es de foco e pausas automaticamente.',
        f2: 'Gerenciador de Tarefas', f2d: 'Listas de tarefas di\u00e1rias e mensais com prazos, pontos e controle de atrasos.',
        f3: 'Coach de Produtividade IA', f3d: 'Conecte Groq, Gemini ou DeepSeek para mensagens motivacionais em tempo real. 4 personas.',
        f4: 'Mostradores de Rel\u00f3gio', f4d: 'Alternar entre anal\u00f3gico, digital, minimalista, hi-tech e aviador.',
        f5: 'Painel Web', f5d: 'Controle total pelo navegador. Estat\u00edsticas, configura\u00e7\u00f5es, arquivos e cron\u00f4metros.',
        f6: 'MQTT & Telemetria', f6d: 'Publique dados de presen\u00e7a no seu broker MQTT. Integre com Home Assistant e Node-RED.',
        setupLabel: 'Configura\u00e7\u00e3o', setupTitle: 'Tr\u00eas minutos para trabalhar melhor',
        s1t: 'Coloque na Mesa', s1d: 'Posicione o DeskBuddy de frente para voc\u00ea. O sensor mmWave calibra automaticamente.',
        s2t: 'Conecte ao WiFi', s2d: 'Conecte-se via portal cativo ou IP est\u00e1tico. mDNS torna a descoberta autom\u00e1tica.',
        s3t: 'Abra o Painel', s3d: 'Instale o app companion ou acesse deskbuddy.local. Estat\u00edsticas e IA prontos.',
        s4t: 'Mantenha o Foco', s4d: 'Deixe o DeskBuddy monitorar seu tempo e mant\u00ea-lo motivado durante o dia.',
        dlLabel: 'App Desktop', dlTitle: 'Um clique para seu painel', dlSub: 'O DeskBuddy Companion fica na bandeja do sistema, descobre seu dispositivo automaticamente.',
        priceLabel: 'Pre\u00e7os', priceTitle: 'Escolha seu DeskBuddy', priceSub: 'Frete gr\u00e1tis para todo o Brasil. 30 dias de garantia.',
        buy: 'Comprar Agora', noship: 'Sem necessidade de cadastro. O app se conecta apenas localmente.',
        badge: 'Agora Dispon\u00edvel'
      }
    : { hero: 'Your Desk. Smarter.', sub: 'DeskBuddy tracks your presence, manages your tasks, and delivers AI-powered coaching \u2014 all from a beautiful circular display on your desk. Built for focus.', cta1: 'Get the Companion App', cta2: 'See How It Works',
        featLabel: 'Features', featTitle: 'Everything you need to stay focused', featSub: 'DeskBuddy combines hardware sensing, smart software, and AI to build better work habits.',
        f1: 'Presence Detection', f1d: 'mmWave radar detects when you\'re at your desk. Tracks desk time, focus sessions, and breaks automatically.',
        f2: 'Task & Agenda Manager', f2d: 'Built-in daily and monthly task lists with due dates, points, and overdue tracking.',
        f3: 'AI Productivity Coach', f3d: 'Connect Groq, Gemini, or DeepSeek for real-time motivational messages. Choose from 4 coaching personas.',
        f4: 'Customizable Clock Faces', f4d: 'Switch between analog, digital, minimalist, hi-tech, developer, and aviator faceplates.',
        f5: 'Web Dashboard', f5d: 'Full control from any browser on your network. Stats, settings, files, and timers.',
        f6: 'MQTT & Telemetry', f6d: 'Publish presence data to your MQTT broker. Integrate with Home Assistant, Node-RED.',
        setupLabel: 'Setup', setupTitle: 'Three minutes to smarter work',
        s1t: 'Place on Desk', s1d: 'Position the DeskBuddy facing you. The mmWave sensor calibrates automatically.',
        s2t: 'Connect to WiFi', s2d: 'Join your network via captive portal or configure static IP. mDNS makes discovery automatic.',
        s3t: 'Open Dashboard', s3d: 'Install the companion app or visit deskbuddy.local. Your stats, tasks, and AI coach are ready.',
        s4t: 'Stay Focused', s4d: 'Let DeskBuddy track your time, surface overdue tasks, and keep you motivated throughout the day.',
        dlLabel: 'Desktop App', dlTitle: 'One click to your dashboard', dlSub: 'The DeskBuddy Companion lives in your system tray, auto-discovers your device, and opens the dashboard instantly.',
        priceLabel: 'Pricing', priceTitle: 'Choose your DeskBuddy', priceSub: 'Free shipping across Canada and the US. 30-day return policy.',
        buy: 'Buy Now', noship: 'No registration required. Companion app connects locally only.',
        badge: 'Now Shipping v1.0'
      };
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
  .btn-buy{display:inline-block;width:100%;padding:12px;border-radius:8px;font-weight:700;font-size:0.9rem;text-decoration:none;transition:all .2s;text-align:center}
  .btn-buy:hover{transform:translateY(-1px)}
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
    <a href="/store">Store</a>
    <a href="/support">Support</a>
    <a href="#download">Download</a>
    <a href="#pricing">Pricing</a>
    <a href="/companion">Companion App</a>
  </div>
</nav>

<section class="hero">
  <div class="hero-badge">&#x2022; ${t.badge}</div>
  <h1>${t.hero}</h1>
  <p>${t.sub}</p>
  <div class="actions">
    <a href="#download" class="btn btn-primary">${t.cta1}</a>
    <a href="#features" class="btn btn-secondary">${t.cta2}</a>
  </div>
</section>

<section id="features">
  <div class="container">
    <div class="section-label">${t.featLabel}</div>
    <h2 class="section-title">${t.featTitle}</h2>
    <p class="section-sub">${t.featSub}</p>
    <div class="features">
      <div class="feature-card">
        <div class="feature-icon">&#x1F4CD;</div>
        <h3>${t.f1}</h3>
        <p>${t.f1d}</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">&#x1F4CB;</div>
        <h3>${t.f2}</h3>
        <p>${t.f2d}</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">&#x1F9E0;</div>
        <h3>${t.f3}</h3>
        <p>${t.f3d}</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">&#x23F0;</div>
        <h3>${t.f4}</h3>
        <p>${t.f4d}</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">&#x1F310;</div>
        <h3>${t.f5}</h3>
        <p>${t.f5d}</p>
      </div>
      <div class="feature-card">
        <div class="feature-icon">&#x1F4E1;</div>
        <h3>${t.f6}</h3>
        <p>${t.f6d}</p>
      </div>
    </div>
  </div>
</section>

<section class="how">
  <div class="container">
    <div class="section-label">${t.setupLabel}</div>
    <h2 class="section-title">${t.setupTitle}</h2>
    <div class="steps-row">
      <div class="step">
        <div class="step-num">1</div>
        <h4>${t.s1t}</h4>
        <p>${t.s1d}</p>
      </div>
      <div class="step">
        <div class="step-num">2</div>
        <h4>${t.s2t}</h4>
        <p>${t.s2d}</p>
      </div>
      <div class="step">
        <div class="step-num">3</div>
        <h4>${t.s3t}</h4>
        <p>${t.s3d}</p>
      </div>
      <div class="step">
        <div class="step-num">4</div>
        <h4>${t.s4t}</h4>
        <p>${t.s4d}</p>
      </div>
    </div>
  </div>
</section>

<section id="download" class="download-section">
  <div class="container">
    <div class="section-label">${t.dlLabel}</div>
    <h2 class="section-title">${t.dlTitle}</h2>
    <p class="section-sub">${t.dlSub}</p>
    ${downloadSection}
    <p style="color:#64748b;font-size:0.78rem;margin-top:12px">${t.noship}</p>
  </div>
</section>

<section id="pricing">
  <div class="container">
    <div class="section-label">${t.priceLabel}</div>
    <h2 class="section-title">${t.priceTitle}</h2>
    <p class="section-sub" style="margin-bottom:32px">${t.priceSub}</p>
    <div class="pricing-grid">
      <div class="price-card">
        <div class="price-name">DeskBuddy Kit</div>
        <div class="price-amount">$99<span>.00</span></div>
        <div class="price-desc">ESP32-C3 + GC9A01 display + mmWave sensor</div>
        <ul class="price-features">
          <li>GC9A01 240×240 circular IPS display</li>
          <li>ESP32-C3 RISC-V microcontroller</li>
          <li>HLK-LD2410 mmWave presence sensor</li>
          <li>All clock faces included</li>
          <li>Web dashboard & WiFi provisioning</li>
          <li>DeskBuddy Companion tray app</li>
        </ul>
        <a href="/store" class="btn btn-buy frost" style="cursor:pointer;opacity:1">${t.buy}</a>
      </div>
      <div class="price-card popular">
        <div class="price-name">DeskBuddy + AI Bundle</div>
        <div class="price-amount">$149<span>.00</span></div>
        <div class="price-desc">Everything in Kit, plus AI coaching & cloud telemetry</div>
        <ul class="price-features">
          <li>Everything in DeskBuddy Kit</li>
          <li>AI productivity coach (Groq / Gemini / DeepSeek)</li>
          <li>4 coaching personas</li>
          <li>Cloud telemetry dashboard</li>
          <li>Over-the-air firmware updates</li>
          <li>Priority email support</li>
        </ul>
        <a href="/store" class="btn btn-buy purple" style="cursor:pointer;opacity:1">${t.buy}</a>
      </div>
      <div class="price-card">
        <div class="price-name">DeskBuddy Pro</div>
        <div class="price-amount">$199<span>.00</span></div>
        <div class="price-desc">For teams and power users</div>
        <ul class="price-features">
          <li>Everything in AI Bundle</li>
          <li>MQTT integration (Home Assistant ready)</li>
          <li>REST API for custom integrations</li>
          <li>Multi-device dashboard</li>
          <li>Custom faceplate designer</li>
          <li>Dedicated support channel</li>
        </ul>
        <a href="/store" class="btn btn-buy purple" style="cursor:pointer;opacity:1">${t.buy}</a>
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

async function seedProducts(db) {
  const products = [
    { slug:'deskbuddy-kit', name:'DeskBuddy Kit', price_cents:9900, description:'ESP32-C3 + GC9A01 display + mmWave sensor', sort_order:0 },
    { slug:'deskbuddy-ai', name:'DeskBuddy + AI Bundle', price_cents:14900, description:'Kit + AI coaching (Groq/Gemini/DeepSeek) + cloud telemetry', sort_order:1 },
    { slug:'deskbuddy-pro', name:'DeskBuddy Pro', price_cents:19900, description:'AI Bundle + MQTT, REST API, multi-device dashboard', sort_order:2 },
  ];
  for (const p of products) {
    await db.prepare('INSERT OR IGNORE INTO products (slug,name,description,price_cents,sort_order) VALUES (?1,?2,?3,?4,?5)')
      .bind(p.slug, p.name, p.description, p.price_cents, p.sort_order).run();
  }
}

function STORE_PAGE(products) {
  const items = products.map(p => {
    const slug = JSON.stringify(p.slug);
    const name = JSON.stringify(p.name);
    return '<div class="store-card"><h3>'+p.name+'</h3><p>'+p.description+'</p><div class="store-price">$'+(p.price_cents/100).toFixed(2)+' CAD</div><button class="btn btn-primary" onclick="addToCart('+slug+','+name+','+p.price_cents+')">Add to Cart</button></div>';
  }).join('');
  return '<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>DeskBuddy Store</title><style>*,*::before,*::after{margin:0;padding:0;box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#0a0e17;color:#e2e8f0;min-height:100vh}.nav{display:flex;align-items:center;justify-content:space-between;padding:18px 40px;max-width:1200px;margin:0 auto}.nav-logo{font-size:1.3rem;font-weight:800;color:#38bdf8;text-decoration:none}.nav-links{display:flex;gap:28px;align-items:center}.nav-links a{color:#94a3b8;text-decoration:none;font-size:0.88rem}.nav-links a:hover{color:#e2e8f0}.cart-badge{background:#38bdf8;color:#0a0e17;border-radius:10px;padding:2px 8px;font-size:0.72rem;font-weight:700;display:none}.container{max-width:1000px;margin:0 auto;padding:40px 20px}.store-grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(300px,1fr));gap:20px;margin-top:30px}.store-card{background:#111827;border:1px solid #1e293b;border-radius:14px;padding:28px;transition:border-color .2s}.store-card:hover{border-color:#38bdf840}.store-card h3{font-size:1.1rem;margin-bottom:8px;color:#f1f5f9}.store-card p{color:#64748b;font-size:0.85rem;line-height:1.5;margin-bottom:16px}.store-price{font-size:1.6rem;font-weight:800;color:#38bdf8;margin-bottom:16px}.btn{display:inline-flex;align-items:center;gap:8px;padding:12px 24px;border-radius:8px;font-weight:700;font-size:0.9rem;text-decoration:none;transition:all .2s;cursor:pointer;border:none}.btn-primary{background:#38bdf8;color:#0a0e17}.btn-primary:hover{background:#7dd3fc;transform:translateY(-1px)}.btn-secondary{background:#1e293b;color:#e2e8f0;border:1px solid #334155}.btn-secondary:hover{background:#334155}.cart-panel{position:fixed;right:0;top:0;width:380px;height:100vh;background:#111827;border-left:1px solid #1e293b;padding:24px;transform:translateX(100%);transition:transform .2s;z-index:100;overflow-y:auto}.cart-panel.open{transform:translateX(0)}.cart-overlay{display:none;position:fixed;inset:0;background:#00000060;z-index:99}.cart-overlay.open{display:block}.cart-item{display:flex;justify-content:space-between;align-items:center;padding:12px 0;border-bottom:1px solid #1e293b}.cart-item button{background:none;border:none;color:#ef4444;cursor:pointer;font-size:1.1rem}.cart-total{font-size:1.3rem;font-weight:800;margin:16px 0;text-align:right}#checkout-email{width:100%;padding:10px;border-radius:8px;border:1px solid #334155;background:#0f172a;color:#f8fafc;margin-bottom:12px;font-size:0.88rem}.toast{position:fixed;bottom:24px;left:50%;transform:translateX(-50%);background:#38bdf8;color:#0a0e17;padding:12px 24px;border-radius:10px;font-weight:700;font-size:0.9rem;z-index:200;opacity:0;transition:opacity .3s}.toast.show{opacity:1}.footer{text-align:center;padding:40px 20px;color:#475569;font-size:0.78rem;border-top:1px solid #1e293b;margin-top:40px}.footer a{color:#64748b}@media(max-width:640px){.nav{padding:16px 20px}.cart-panel{width:100%}}</style></head><body><nav class="nav"><a href="/" class="nav-logo">DeskBuddy</a><div class="nav-links"><a href="/store">Store</a><a href="/support">Support</a><a href="/companion">App</a><button class="btn btn-secondary" onclick="toggleCart()" style="padding:8px 14px;font-size:0.82rem">Cart <span class="cart-badge" id="cartBadge">0</span></button></div></nav><div class="container"><div style="color:#38bdf8;font-size:0.78rem;font-weight:700;text-transform:uppercase;letter-spacing:0.1em;text-align:center">Store</div><h1 style="text-align:center;font-size:2rem;font-weight:800;margin:8px 0 4px">DeskBuddy Products</h1><p style="text-align:center;color:#64748b;font-size:0.9rem">Free shipping across Canada and the US.</p><div class="store-grid">'+items+'</div></div><div class="cart-overlay" id="cartOverlay" onclick="closeCart()"></div><div class="cart-panel" id="cartPanel"><div style="display:flex;justify-content:space-between;align-items:center;margin-bottom:20px"><h2 style="font-size:1.2rem">Your Cart</h2><button onclick="closeCart()" style="background:none;border:none;color:#94a3b8;font-size:1.3rem;cursor:pointer">&times;</button></div><div id="cartItems"><p style="color:#64748b;font-size:0.85rem">Your cart is empty.</p></div><div class="cart-total" id="cartTotal"></div><input type="email" id="checkout-email" placeholder="Email for receipt (optional)" /><button class="btn btn-primary" style="width:100%;justify-content:center" id="checkoutBtn" onclick="checkout()" disabled>Checkout</button><p style="color:#64748b;font-size:0.7rem;margin-top:8px;text-align:center">Powered by Stripe. Secure payment.</p></div><div class="toast" id="toast"></div><footer class="footer">DeskBuddy &copy; 2026 &middot; <a href="/">Home</a> &middot; <a href="/support">Support</a></footer><script>var cart=JSON.parse(localStorage.getItem("deskbuddy_cart")||"[]");function saveCart(){localStorage.setItem("deskbuddy_cart",JSON.stringify(cart));updateCartUI()}function addToCart(slug,name,price){var i=cart.findIndex(function(c){return c.slug===slug});if(i>=0){cart[i].qty++}else{cart.push({slug:slug,name:name,price:parseInt(price),qty:1})}saveCart();showToast(name+" added!")}function removeFromCart(slug){cart=cart.filter(function(c){return c.slug!==slug});saveCart()}function toggleCart(){document.getElementById("cartPanel").classList.toggle("open");document.getElementById("cartOverlay").classList.toggle("open")}function closeCart(){document.getElementById("cartPanel").classList.remove("open");document.getElementById("cartOverlay").classList.remove("open")}function updateCartUI(){var b=document.getElementById("cartBadge"),t=cart.reduce(function(s,i){return s+i.qty},0);b.textContent=t;b.style.display=t>0?"inline":"none";var e=document.getElementById("cartItems");if(cart.length===0){e.innerHTML="Your cart is empty."}else{e.innerHTML=cart.map(function(c){return "<div class=cart-item><div><strong>"+c.name+"</strong> x"+c.qty+"<br><span style=color:#64748b;font-size:0.75rem>$"+(c.price*c.qty/100).toFixed(2)+"</span></div><button onclick=removeFromCart(\""+c.slug+"\")>&times;</button></div><\/div>"}).join("")}document.getElementById("cartTotal").textContent=cart.length?"Total: $"+(cart.reduce(function(s,i){return s+i.price*i.qty},0)/100).toFixed(2):"";document.getElementById("checkoutBtn").disabled=cart.length===0}async function checkout(){var email=document.getElementById("checkout-email").value.trim(),items=cart.map(function(c){return{slug:c.slug,qty:c.qty}}),btn=document.getElementById("checkoutBtn");btn.disabled=true;btn.textContent="Redirecting...";try{var r=await fetch("/api/checkout",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({items:items,email:email||undefined})}),d=await r.json();if(d.url){window.location.href=d.url;return}showToast(d.error||"Checkout failed");btn.disabled=false;btn.textContent="Checkout"}catch(e){showToast("Network error");btn.disabled=false;btn.textContent="Checkout"}}function showToast(msg){var t=document.getElementById("toast");t.textContent=msg;t.classList.add("show");setTimeout(function(){t.classList.remove("show")},2500)}document.querySelectorAll(".store-add").forEach(function(b){b.addEventListener("click",function(){addToCart(b.dataset.slug,b.dataset.name,b.dataset.price);toggleCart()})});updateCartUI();<\/script><\/body><\/html>';
}

function STORE_SUCCESS_PAGE() {
  return '<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>Order Confirmed &mdash; DeskBuddy</title><style>*,*::before,*::after{margin:0;padding:0;box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#0a0e17;color:#e2e8f0;display:flex;align-items:center;justify-content:center;min-height:100vh;padding:20px}.card{background:#111827;border:1px solid #1e293b;border-radius:16px;padding:48px;text-align:center;max-width:480px}.icon{font-size:3.5rem;margin-bottom:16px}h1{font-size:1.6rem;color:#38bdf8;margin-bottom:8px}p{color:#94a3b8;font-size:0.92rem;line-height:1.5;margin-bottom:24px}.btn{display:inline-block;background:#38bdf8;color:#0a0e17;padding:12px 28px;border-radius:8px;font-weight:700;text-decoration:none;transition:transform .2s}.btn:hover{transform:translateY(-1px)}<\/style><\/head><body><div class="card"><div class="icon">&#x2705;<\/div><h1>Order Confirmed!<\/h1><p>Your DeskBuddy is being prepared. You will receive a confirmation email with tracking details once shipped.<\/p><a href="/" class="btn">Back to Home<\/a><\/div><\/body><\/html>';
}

function SUPPORT_PAGE() {
  return '<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>Support &mdash; DeskBuddy</title><style>*,*::before,*::after{margin:0;padding:0;box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#0a0e17;color:#e2e8f0;min-height:100vh}.nav{display:flex;align-items:center;justify-content:space-between;padding:18px 40px;max-width:1200px;margin:0 auto}.nav-logo{font-size:1.3rem;font-weight:800;color:#38bdf8;text-decoration:none}.nav-links{display:flex;gap:28px}.nav-links a{color:#94a3b8;text-decoration:none;font-size:0.88rem}.nav-links a:hover{color:#e2e8f0}.container{max-width:560px;margin:0 auto;padding:60px 20px}h1{font-size:1.8rem;color:#38bdf8;margin-bottom:8px}.sub{color:#64748b;font-size:0.92rem;margin-bottom:32px}.form-group{margin-bottom:16px}.form-group label{display:block;color:#94a3b8;font-size:0.8rem;margin-bottom:4px;text-transform:uppercase;letter-spacing:0.05em}.form-group input,.form-group textarea{width:100%;padding:10px 12px;border-radius:8px;border:1px solid #334155;background:#111827;color:#f8fafc;font-size:0.9rem;outline:none}.form-group input:focus,.form-group textarea:focus{border-color:#38bdf8}.form-group textarea{min-height:120px;resize:vertical}.btn{display:inline-flex;align-items:center;gap:8px;padding:12px 28px;border-radius:8px;font-weight:700;font-size:0.92rem;text-decoration:none;transition:all .2s;cursor:pointer;border:none}.btn-primary{background:#38bdf8;color:#0a0e17}.btn-primary:hover{background:#7dd3fc;transform:translateY(-1px)}#msg{color:#38bdf8;font-size:0.85rem;margin-top:12px;min-height:20px}.footer{text-align:center;padding:40px 20px;color:#475569;font-size:0.78rem;border-top:1px solid #1e293b}.footer a{color:#64748b}<\/style><\/head><body><nav class="nav"><a href="/" class="nav-logo">DeskBuddy<\/a><div class="nav-links"><a href="/store">Store<\/a><a href="/support">Support<\/a><a href="/companion">App<\/a><\/div><\/nav><div class="container"><h1>Contact Support<\/h1><p class="sub">Have a question or issue with your DeskBuddy? Send us a message and we will reply within 24 hours.<\/p><div class="form-group"><label>Name<\/label><input type="text" id="name" placeholder="Your name" /><\/div><div class="form-group"><label>Email<\/label><input type="email" id="email" placeholder="you@example.com" /><\/div><div class="form-group"><label>Subject<\/label><input type="text" id="subject" placeholder="What is this about?" /><\/div><div class="form-group"><label>Message<\/label><textarea id="message" placeholder="Describe your issue..."><\/textarea><\/div><button class="btn btn-primary" onclick="submitTicket()">Send Message<\/button><div id="msg"><\/div><\/div><footer class="footer">DeskBuddy &copy; 2026 &middot; <a href="/">Home<\/a> &middot; <a href="/store">Store<\/a><\/footer><script>async function submitTicket(){var n=document.getElementById("name").value.trim(),e=document.getElementById("email").value.trim(),s=document.getElementById("subject").value.trim(),m=document.getElementById("message").value.trim();if(!n||!e||!s||!m){document.getElementById("msg").textContent="All fields are required.";return}try{var r=await fetch("/api/support",{method:"POST",headers:{"Content-Type":"application/json"},body:JSON.stringify({name:n,email:e,subject:s,message:m})}),d=await r.json();if(d.ok){document.getElementById("msg").textContent="Message sent! We will get back to you soon.";document.querySelectorAll("input,textarea").forEach(function(el){el.value=""})}else{document.getElementById("msg").textContent=d.error||"Something went wrong."}}catch(err){document.getElementById("msg").textContent="Network error."}}<\/script><\/body><\/html>';
}

function ADMIN_SETUP_HTML() {
  return '<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>Admin Setup</title><style>*,*::before,*::after{margin:0;padding:0;box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#0a0e17;color:#e2e8f0;display:flex;align-items:center;justify-content:center;min-height:100vh;padding:20px}.card{background:#111827;border:1px solid #1e293b;border-radius:16px;padding:40px;max-width:480px;text-align:center}h1{font-size:1.4rem;color:#38bdf8;margin-bottom:12px}p{color:#64748b;font-size:0.9rem;line-height:1.5;margin-bottom:20px}code{background:#1e293b;padding:4px 8px;border-radius:4px;font-size:0.85rem;color:#38bdf8}.footer{margin-top:24px;color:#475569;font-size:0.75rem}</style></head><body><div class="card"><h1>Admin Not Configured</h1><p>Set an admin password with:</p><code>npx wrangler secret put ADMIN_PASSWORD</code><p>Then reload this page.</p><div class="footer">DeskBuddy Admin</div></div></body></html>';
}
