import { seedProducts } from '../seed.js';

export async function handleStore(request, env, path, url, corsHeaders) {
  if (request.method === 'GET' && path === '/store.js') {
    const obj = await env.FIRMWARE.get('companion/store.js');
    if (!obj) return new Response('Not Found', { status: 404 });
    return new Response(obj.body, { headers: { 'Content-Type': 'application/javascript; charset=utf-8', 'Cache-Control': 'public, max-age=3600' } });
  }

  if (request.method === 'GET' && path === '/store') {
    let products = await env.DB.prepare('SELECT * FROM products WHERE active=1 ORDER BY sort_order').all();
    if (!products.results || products.results.length === 0) {
      await seedProducts(env.DB);
      products = await env.DB.prepare('SELECT * FROM products WHERE active=1 ORDER BY sort_order').all();
    }
    const { STORE_PAGE } = await import('../pages/store.js');
    return new Response(STORE_PAGE(products.results || []), { headers: { 'Content-Type': 'text/html; charset=utf-8', ...corsHeaders } });
  }

  if (request.method === 'GET' && path === '/api/products') {
    const products = await env.DB.prepare('SELECT * FROM products WHERE active=1 ORDER BY sort_order').all();
    return Response.json(products.results || [], { headers: corsHeaders });
  }

  if (request.method === 'POST' && path === '/api/checkout') {
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
      method:'POST', headers:{ Authorization:`Bearer ${stripeKey}`, 'Content-Type':'application/x-www-form-urlencoded' }, body: params.toString()
    });
    const session = await stripeResp.json();
    if (session.url) {
      await env.DB.prepare('INSERT INTO orders (stripe_session_id, customer_email, total_cents, items_json, status) VALUES (?1,?2,?3,?4,?)')
        .bind(session.id, body.email||'', line_items.reduce((s,i)=>s+i.price_data.unit_amount*i.quantity,0), JSON.stringify(line_items), 'pending').run();
      return Response.json({ url:session.url }, { headers:corsHeaders });
    }
    return Response.json({ error:session.error?.message||'Stripe error' }, { status:400, headers:corsHeaders });
  }

  if (request.method === 'GET' && path === '/store/success') {
    return new Response(STORE_SUCCESS_HTML, { headers:{ 'Content-Type':'text/html;charset=utf-8', ...corsHeaders }});
  }

  if (request.method === 'POST' && path === '/webhook/stripe') {
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
  }

  return null;
}

const STORE_SUCCESS_HTML = '<!DOCTYPE html><html lang="en"><head><meta charset="UTF-8"><meta name="viewport" content="width=device-width,initial-scale=1.0"><title>Order Confirmed — DeskBuddy</title><style>*,*::before,*::after{margin:0;padding:0;box-sizing:border-box}body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#0a0e17;color:#e2e8f0;display:flex;align-items:center;justify-content:center;min-height:100vh;padding:20px}.card{background:#111827;border:1px solid #1e293b;border-radius:16px;padding:48px;text-align:center;max-width:480px}.icon{font-size:3.5rem;margin-bottom:16px}h1{font-size:1.6rem;color:#38bdf8;margin-bottom:8px}p{color:#94a3b8;font-size:0.92rem;line-height:1.5;margin-bottom:24px}.btn{display:inline-block;background:#38bdf8;color:#0a0e17;padding:12px 28px;border-radius:8px;font-weight:700;text-decoration:none;transition:transform .2s}.btn:hover{transform:translateY(-1px)}</style></head><body><div class="card"><div class="icon">&#x2705;</div><h1>Order Confirmed!</h1><p>Your DeskBuddy is being prepared. You will receive a confirmation email with tracking details once shipped.</p><a href="/" class="btn">Back to Home</a></div></body></html>';
