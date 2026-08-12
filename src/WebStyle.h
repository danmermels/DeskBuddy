#ifndef WEB_STYLE_H
#define WEB_STYLE_H

#include <Arduino.h>

static const char WEB_STYLE_CSS[] PROGMEM = R"rawcss(
/* DeskBuddy shared stylesheet */
body{font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,sans-serif;background:#0f172a;color:#f8fafc;margin:0;padding:20px;display:flex;flex-direction:column;align-items:center}
.header{display:flex;justify-content:space-between;align-items:center;width:100%;max-width:600px;margin-bottom:10px;padding:0 10px;box-sizing:border-box}
.header h1{margin:0;font-size:1.6rem;color:#38bdf8;font-weight:800;letter-spacing:-.025em}
.settings-header{display:flex;align-items:center;width:100%;max-width:650px;margin-bottom:15px;gap:15px;padding:0 10px;box-sizing:border-box}
.settings-header h1{margin:0;font-size:1.6rem;color:#38bdf8;font-weight:800}
.back-btn{color:#94a3b8;cursor:pointer;text-decoration:none;font-weight:600;font-size:.95rem;display:flex;align-items:center;gap:6px;transition:color .2s,transform .2s}
.back-btn:hover{color:#38bdf8;transform:translateX(-3px)}
.back-btn svg{width:24px;height:24px;fill:currentColor}
.cog-btn{color:#94a3b8;cursor:pointer;transition:color .2s,transform .3s ease;display:flex;align-items:center;justify-content:center}
.cog-btn:hover{color:#38bdf8;transform:rotate(45deg)}
.cog-btn svg{width:24px;height:24px;fill:currentColor}
h1{font-size:1.5rem;color:#38bdf8;text-align:center;margin-bottom:20px}
h2{font-size:1.25rem;color:#38bdf8;margin-top:0;margin-bottom:15px;border-bottom:1px solid #334155;padding-bottom:8px}
.subtitle{color:#94a3b8;text-align:center;font-size:.9rem;margin-bottom:20px}
.help{color:#64748b;font-size:.8rem;text-align:center;margin-top:12px}
.card{background:#1e293b;border-radius:12px;padding:20px;margin:10px;width:100%;max-width:600px;box-shadow:0 4px 6px rgba(0,0,0,.1);border:1px solid #334155;box-sizing:border-box}
.ai-card{background:linear-gradient(135deg,#1e293b 0%,#0f172a 100%);border:1px solid #38bdf8;box-shadow:0 0 15px rgba(56,189,248,.15);position:relative;overflow:hidden}
.points-card{background:#1e293b;border-radius:12px;padding:20px;margin:10px 0;width:100%;max-width:600px;box-shadow:0 4px 6px rgba(0,0,0,.1);border:1px solid #334155;box-sizing:border-box;display:flex;align-items:center;justify-content:space-between;gap:16px;flex-wrap:wrap}
.metric{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #334155;align-items:center}
.metric:last-child{border:none}
.label{color:#94a3b8}
.value{font-weight:bold}
.badge{padding:4px 8px;border-radius:6px;font-size:.8rem}
.badge-present{background:#15803d;color:#bbf7d0}
.badge-away{background:#991b1b;color:#fca5a5}
.score-high{color:#4ade80}
.score-med{color:#fbbf24}
.score-low{color:#f87171}
.btn{background:#38bdf8;color:#0f172a;font-weight:bold;border:none;padding:8px 16px;border-radius:8px;cursor:pointer;font-family:inherit;font-size:.95rem;transition:opacity .2s,transform .1s,background-color .2s;display:inline-flex;align-items:center;justify-content:center}
.btn:hover{opacity:.9}
.btn:active{transform:scale(.98)}
.btn-secondary{background:#475569;color:#f8fafc}
.btn-secondary:hover{background:#334155}
.btn-purple{background:#7c3aed;color:#f5f3ff}
.btn-purple:hover{background:#8b5cf6}
.btn-danger{background:#ef4444;color:#fff}
.btn-danger:hover{background:#dc2626}
.btn-scan{background:#6366f1;color:#fff;padding:6px 14px;border-radius:6px;border:none;cursor:pointer;font-family:inherit;font-size:.85rem;font-weight:bold;white-space:nowrap;transition:opacity .2s}
.btn-scan:hover{opacity:.85}
.btn-scan:disabled{opacity:.5;cursor:wait}
button{background:#38bdf8;color:#0f172a;border:none;border-radius:6px;padding:8px 16px;font-weight:bold;cursor:pointer;transition:opacity .2s}
button:hover{opacity:.9}
.settings-input{background:#0f172a;color:#f8fafc;border:1px solid #334155;padding:8px 10px;border-radius:6px;outline:none;font-family:inherit;font-size:.95rem;box-sizing:border-box}
.settings-input:focus{border-color:#38bdf8;box-shadow:0 0 0 2px rgba(56,189,248,.2)}
.settings-input-short{background:#0f172a;color:#f8fafc;border:1px solid #334155;padding:6px 10px;border-radius:6px;width:120px;text-align:right;font-family:inherit;font-size:.95rem}
.settings-select{background:#0f172a;color:#f8fafc;border:1px solid #334155;padding:6px 10px;border-radius:6px;font-family:inherit;font-size:.95rem}
.input-group{display:flex;gap:8px;margin-bottom:15px}
input[type="text"]{background:#0f172a;border:1px solid #334155;border-radius:6px;padding:8px 12px;color:#f8fafc;font-size:.95rem;outline:none}
.input-group input[type="text"]{flex:1}
input[type="text"]:focus{border-color:#38bdf8}
.field-group{padding:10px 0;border-bottom:1px solid #334155}
.field-group:last-child{border:none}
.field-label{color:#94a3b8;font-size:.9rem;margin-bottom:4px;display:block}
.field-help{color:#64748b;font-size:.75rem;margin-top:3px;display:block}
.notice{background:#1e3a5f;border:1px solid #38bdf8;border-radius:8px;padding:10px 15px;margin:10px;width:100%;max-width:650px;box-sizing:border-box;font-size:.85rem;color:#38bdf8;text-align:center}
.ssid-row{display:flex;gap:8px;align-items:flex-end}
.ssid-row input{flex:1}
#apList{max-height:0;overflow:hidden;transition:max-height .3s ease;margin-top:0}
#apList.open{max-height:260px;overflow-y:auto;margin-top:8px;border:1px solid #334155;border-radius:6px;background:#0f172a}
.ap-item{display:flex;justify-content:space-between;align-items:center;padding:8px 12px;cursor:pointer;border-bottom:1px solid #1e293b;font-size:.9rem;transition:background .15s}
.ap-item:last-child{border:none}
.ap-item:hover{background:#1e293b}
.ap-item .ap-name{color:#f8fafc}
.ap-item .ap-rssi{color:#64748b;font-size:.8rem}
.ap-item .ap-locked{color:#f59e0b;font-size:.75rem;margin-left:6px}
.panel-header-row{display:flex;justify-content:space-between;align-items:center;margin-bottom:20px}
.tft-display-entry{display:flex;flex-direction:column;align-items:center;justify-content:center;padding:16px;background:rgba(15,23,42,.6);border-radius:12px;border:1px solid rgba(51,65,85,.5);margin-bottom:8px;transition:background .2s ease,border-color .2s ease;text-align:center;box-sizing:border-box;width:100%}
.tft-display-entry:hover{background:rgba(30,41,59,.8);border-color:rgba(56,189,248,.4)}
.tft-display-entry.ai-display-entry{border:1px solid rgba(56,189,248,.3);background:linear-gradient(135deg,rgba(30,41,59,.5) 0%,rgba(15,23,42,.7) 100%);box-shadow:inset 0 0 12px rgba(56,189,248,.05)}
.tft-display-time{font-size:.75rem;font-weight:600;color:#64748b;letter-spacing:.05em;margin-bottom:6px;font-variant-numeric:tabular-nums}
.tft-display-text{font-size:1.25rem;font-weight:300;line-height:1.4;color:#e2e8f0;max-width:90%}
.tft-display-entry.ai-display-entry .tft-display-text{color:#38bdf8;text-shadow:0 0 10px rgba(56,189,248,.15)}
.ai-badge{display:none;font-size:.75rem;color:#64748b;text-transform:uppercase;letter-spacing:.05em;font-weight:bold}
.ai-loading-container{display:flex;align-items:center;justify-content:center;gap:8px;color:#fbbf24;font-size:.85rem}
.ai-spinner{width:16px;height:16px;border:2px solid transparent;border-top-color:currentColor;border-radius:50%;animation:spin .8s linear infinite}
@keyframes spin{to{transform:rotate(360deg)}}
.mqtt-status{display:flex;align-items:center;gap:6px;font-size:.8rem;color:#94a3b8;background:#0f172a;padding:4px 10px;border-radius:20px;border:1px solid #334155}
.status-dot{width:8px;height:8px;border-radius:50%;display:inline-block;box-shadow:0 0 8px currentColor}
.status-connected{background:#10b981;color:#10b981}
.status-disconnected{background:#ef4444;color:#ef4444}
#mqttConsole::-webkit-scrollbar{width:6px}
#mqttConsole::-webkit-scrollbar-track{background:#0f172a}
#mqttConsole::-webkit-scrollbar-thumb{background:#334155;border-radius:3px}
#mqttConsole::-webkit-scrollbar-thumb:hover{background:#38bdf8}
.expand-toggle{background:none;border:none;color:#64748b;cursor:pointer;padding:4px;display:flex;align-items:center;transition:color .2s,transform .3s ease}
.expand-toggle:hover{color:#38bdf8}
.expand-toggle svg{width:18px;height:18px;fill:currentColor;transition:transform .3s ease}
.expand-toggle.expanded svg{transform:rotate(180deg)}
.timer-display{font-family:'SF Mono',Consolas,'Courier New',monospace;font-size:2rem;font-weight:700;color:#38bdf8;letter-spacing:.02em;background:#0f172a;border:1px solid #334155;border-radius:10px;padding:10px 14px;text-align:center}
.timer-btn{background:#0f172a;border:1px solid #334155;color:#38bdf8;border-radius:6px;padding:6px 14px;font-size:.9rem;font-weight:700;cursor:pointer;transition:border-color .2s}
.timer-btn:hover{border-color:#38bdf8}
.timer-btn.ghost{background:#334155;color:#f8fafc}
.task-list{display:flex;flex-direction:column;gap:4px;height:364px;overflow-y:auto;padding:8px;background:#0f172a;border:1px solid #334155;border-radius:8px;box-sizing:border-box;margin-bottom:15px}
.task-list::-webkit-scrollbar{width:6px}
.task-list::-webkit-scrollbar-track{background:#0f172a}
.task-list::-webkit-scrollbar-thumb{background:#334155;border-radius:3px}
.task-list::-webkit-scrollbar-thumb:hover{background:#38bdf8}
.task-item{height:40px;box-sizing:border-box;display:flex;align-items:center;justify-content:space-between;background:none;border-bottom:1px solid #1e293b;padding:0 8px;gap:8px;flex-shrink:0}
.task-item:last-child{border-bottom:none}
.task-item.completed{opacity:.5}
.task-item.completed span{text-decoration:line-through}
.task-left{display:flex;align-items:center;gap:10px;flex:1;min-width:0}
.task-left input[type="checkbox"]{width:18px;height:18px;cursor:pointer;accent-color:#38bdf8;flex-shrink:0}
.task-text{font-size:.95rem;color:#e2e8f0;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}
.delete-btn{background:none;border:none;color:#9ca3af;cursor:pointer;padding:4px;display:flex;align-items:center;transition:color .2s;flex-shrink:0}
.delete-btn:hover{color:#6b7280}
.delete-btn svg{width:18px;height:18px;fill:currentColor}
.empty-state{display:flex;align-items:center;justify-content:center;height:100%;color:#64748b;font-size:.9rem}
.points-total{font-size:2.2rem;font-weight:800;letter-spacing:-.03em;color:#e2e8f0}
.points-badge{padding:4px 12px;border-radius:999px;font-size:.85rem;font-weight:700;text-transform:uppercase}
.points-badge.poor{background:rgba(244,63,94,.15);color:#fb7185;border:1px solid #f43f5e}
.points-badge.good{background:rgba(251,191,36,.15);color:#fbbf24;border:1px solid #f59e0b}
.points-badge.excellent{background:rgba(52,211,153,.15);color:#34d399;border:1px solid #10b981}
.points-months{display:flex;gap:6px;flex-wrap:wrap;max-width:320px}
.points-chip{padding:3px 8px;border-radius:6px;font-size:.72rem;font-weight:600;background:#0f172a;color:#94a3b8;border:1px solid #334155;white-space:nowrap}
.points-chip.current{color:#38bdf8;border-color:#38bdf8}
.points-chip .pt{margin-left:4px;font-weight:800}
.cal-btn{background:#0f172a;border:1px solid #334155;color:#38bdf8;border-radius:6px;padding:4px 10px;font-size:.85rem;font-weight:700;cursor:pointer;transition:border-color .2s}
.cal-btn:hover{border-color:#38bdf8}
.cal-grid{display:grid;grid-template-columns:repeat(7,1fr);gap:4px}
.cal-head{text-align:center;font-size:.7rem;color:#64748b;text-transform:uppercase;font-weight:600;padding:2px 0}
.cal-cell{background:#0f172a;border:1px solid #334155;border-radius:8px;min-height:52px;padding:4px;box-sizing:border-box;display:flex;flex-direction:column;gap:3px;cursor:pointer;transition:border-color .15s}
.cal-cell:hover{border-color:#38bdf8}
.cal-cell.today{border-color:#38bdf8}
.cal-cell.out{opacity:.35;pointer-events:none}
.cal-daynum{font-size:.75rem;font-weight:700;color:#cbd5e1}
.cal-cell.today .cal-daynum{color:#38bdf8}
.cal-dots{display:flex;gap:3px;flex-wrap:wrap;margin-top:auto}
.cal-dot{display:inline-block;width:8px;height:8px;border-radius:50%}
.cal-count{font-size:.68rem;color:#94a3b8;font-weight:600}
.slider{-webkit-appearance:none;width:100%;height:6px;border-radius:3px;background:#334155;outline:none;margin:10px 0}
.slider::-webkit-slider-thumb{-webkit-appearance:none;appearance:none;width:18px;height:18px;border-radius:50%;background:#38bdf8;cursor:pointer;transition:transform .1s}
.slider::-webkit-slider-thumb:hover{transform:scale(1.2)}
.chart-container{position:relative;width:100%;height:220px;margin-top:15px}
canvas{display:block;background:#0b0f19;border-radius:8px;border:1px solid #334155;width:100%;height:100%}
.legend{display:flex;justify-content:center;flex-wrap:wrap;gap:12px;margin-top:12px;font-size:.8rem}
.legend-item{display:flex;align-items:center;gap:4px}
.legend-color{width:12px;height:12px;border-radius:3px}
.toggle-container{display:flex;align-items:center;gap:6px;font-size:.85rem;margin:4px 0}
.file-table{width:100%;border-collapse:collapse;margin-top:10px}
.file-table th,.file-table td{padding:12px;text-align:left;border-bottom:1px solid #334155}
.file-table th{color:#94a3b8;font-weight:600;font-size:.9rem;text-transform:uppercase;letter-spacing:.05em}
.file-table tr:hover{background:rgba(56,189,248,.03)}
.file-name{font-weight:600;color:#f1f5f9;display:flex;align-items:center;gap:8px}
.file-size{color:#94a3b8;font-family:monospace}
.actions{display:flex;gap:8px;align-items:center}
.upload-zone{border:2px dashed #475569;border-radius:8px;padding:30px;text-align:center;cursor:pointer;transition:border-color .2s,background-color .2s;margin-bottom:15px}
.upload-zone.dragover{border-color:#38bdf8;background:rgba(56,189,248,.05)}
.upload-zone svg{width:40px;height:40px;fill:#64748b;margin-bottom:10px}
.upload-zone p{margin:0;color:#94a3b8;font-size:.95rem}
.progress-bar-container{display:none;background:#0f172a;border-radius:6px;height:12px;overflow:hidden;margin-top:15px;border:1px solid #334155}
.progress-bar{height:100%;width:0%;background:linear-gradient(90deg,#38bdf8,#06b6d4);transition:width .1s ease}
.status-msg{margin-top:10px;font-size:.9rem;text-align:center;display:none}
.status-msg.success{color:#4ade80;display:block}
.status-msg.error{color:#f87171;display:block}
)rawcss";

#endif // WEB_STYLE_H
