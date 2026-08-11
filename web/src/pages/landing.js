export function LANDING_PAGE(fileSize, checksum, lang) {
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
  .btn-buy{display:inline-block;width:100%;padding:12px;border-radius:8px;font-weight:700;font-size:0.9rem;text-decoration:none;transition:all .2s;text-align:center;cursor:pointer;opacity:1}
  .btn-buy:hover{transform:translateY(-1px);opacity:0.9}
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
  <select id="langSelect" style="background:#111827;color:#94a3b8;border:1px solid #334155;border-radius:6px;padding:6px 10px;font-size:0.82rem;cursor:pointer;outline:none;margin-left:8px">
    <option value="en">EN</option>
    <option value="pt">PT</option>
  </select>
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
<script>
(function(){
  var sel = document.getElementById('langSelect');
  if (!sel) return;
  var params = new URLSearchParams(window.location.search);
  var current = params.get('lang') || localStorage.getItem('deskbuddy_lang') || 'en';
  sel.value = current;
  sel.addEventListener('change', function(){
    localStorage.setItem('deskbuddy_lang', sel.value);
    window.location.search = '?lang=' + sel.value;
  });
})();
</script>
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
