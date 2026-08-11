export async function handleSupport(request, env, path, corsHeaders) {
  if (request.method === 'GET' && path === '/support') {
    const { SUPPORT_PAGE } = await import('../pages/support.js');
    return new Response(SUPPORT_PAGE(), { headers:{ 'Content-Type':'text/html;charset=utf-8', ...corsHeaders }});
  }

  if (request.method === 'POST' && path === '/api/support') {
    const { name, email, subject, message } = await request.json();
    if (!name || !email || !subject || !message) {
      return Response.json({ ok:false, error:'All fields required' }, { status:400, headers:corsHeaders });
    }
    await env.DB.prepare('INSERT INTO support_tickets (name, email, subject, message) VALUES (?1,?2,?3,?4)')
      .bind(name, email, subject, message).run();
    return Response.json({ ok:true }, { headers:corsHeaders });
  }

  return null;
}
