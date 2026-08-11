export async function handleAdmin(request, env, path, url, corsHeaders) {
  if (request.method === 'GET' && path === '/admin') {
    const obj = await env.FIRMWARE.get('companion/admin.html');
    if (!obj) return new Response(ADMIN_SETUP_HTML, { headers:{'Content-Type':'text/html;charset=utf-8',...corsHeaders}});
    return new Response(obj.body, { headers:{ 'Content-Type':'text/html; charset=utf-8',...corsHeaders }});
  }

  if (request.method === 'POST' && path === '/admin/auth') {
    const { password } = await request.json();
    return Response.json({ ok: password && password === env.ADMIN_PASSWORD }, { headers:corsHeaders });
  }

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

  return null;
}

const ADMIN_SETUP_HTML = '<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>Admin Setup</title><style>*,*::before,*::after{margin:0;padding:0;box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#0a0e17;color:#e2e8f0;display:flex;align-items:center;justify-content:center;min-height:100vh;padding:20px}.card{background:#111827;border:1px solid #1e293b;border-radius:16px;padding:40px;max-width:480px;text-align:center}h1{font-size:1.4rem;color:#38bdf8;margin-bottom:12px}p{color:#64748b;font-size:0.9rem;line-height:1.5;margin-bottom:20px}code{background:#1e293b;padding:4px 8px;border-radius:4px;font-size:0.85rem;color:#38bdf8}.footer{margin-top:24px;color:#475569;font-size:0.75rem}</style></head><body><div class="card"><h1>Admin Not Configured</h1><p>Set an admin password with:</p><code>npx wrangler secret put ADMIN_PASSWORD</code><p>Then reload this page.</p><div class="footer">DeskBuddy Admin</div></div></body></html>';
