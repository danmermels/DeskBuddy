export function COMPANION_PAGE(fileSize, checksum) {
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
