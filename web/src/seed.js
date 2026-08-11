export async function seedProducts(db) {
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
