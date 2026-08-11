import { handleTelemetry } from './routes/telemetry.js';
import { handleStore } from './routes/store.js';
import { handleCompanion } from './routes/companion.js';
import { handleSupport } from './routes/support.js';
import { handleAdmin } from './routes/admin.js';
import { LANDING_PAGE } from './pages/landing.js';

export default {
  async fetch(request, env) {
    const url = new URL(request.url);
    const path = url.pathname;

    const corsHeaders = {
      'Access-Control-Allow-Origin': '*',
      'Access-Control-Allow-Methods': 'GET, POST, PUT, DELETE, OPTIONS',
      'Access-Control-Allow-Headers': 'Content-Type, Authorization',
    };

    if (request.method === 'OPTIONS') {
      return new Response(null, { status: 204, headers: corsHeaders });
    }

    // Route dispatch — each handler returns Response if matched, null if skipped
    const handlers = [handleTelemetry, handleStore, handleCompanion, handleSupport, handleAdmin];
    for (const handler of handlers) {
      const response = await handler(request, env, path, url, corsHeaders);
      if (response) return response;
    }

    // Catch-all: landing page with geo-detection
    const langParam = url.searchParams.get('lang');
    const country = (request.cf && request.cf.country) || '';
    const lang = langParam || (country === 'BR' ? 'pt' : 'en');
    return new Response(LANDING_PAGE('2.5 MB', 'B303AFFC9D5DFBC2053237B483BA12FB34ECC8D84487612C45BAAD2F8105E9CB', lang), {
      headers: { 'Content-Type': 'text/html; charset=utf-8', ...corsHeaders },
    });
  },
};
