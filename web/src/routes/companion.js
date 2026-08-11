export async function handleCompanion(request, env, path, corsHeaders) {
  if (request.method === 'GET' && path === '/companion') {
    const hasFile = await env.FIRMWARE.head('companion/windows-setup.exe');
    const { COMPANION_PAGE } = await import('../pages/companion.js');
    return new Response(COMPANION_PAGE(hasFile ? '2.5 MB' : null, 'B303AFFC9D5DFBC2053237B483BA12FB34ECC8D84487612C45BAAD2F8105E9CB'), {
      headers: { 'Content-Type': 'text/html; charset=utf-8', ...corsHeaders },
    });
  }

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

  return null;
}
