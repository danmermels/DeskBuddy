# DeskBuddy — Craft-Market Business Plan, Marketing & Roadmap (v2)

> Status: **committed direction — finished product, small-batch craft sales.** v1 was scenario analysis (open-source + kits + premium tier). After the author's decision, this v2 is re-run around selling **assembled devices** as a craft product on **Etsy / Mercado Livre / Amazon**, with a **portfolio of independent ESP-based products** (DeskBuddy is product #1) as the income goal. Firmware is not yet finished (multi-language pending). All figures are modeled estimates.
>
> **Companion docs:** `architecture.md`, `behaviour-assessment.md`, `MQTTcommands.md`, `ConstantsBreakdown.md`.

---

## 1. Executive Summary

DeskBuddy is an ESP32-C3 desktop companion that combines **mmWave presence sensing**, **personality-driven coaching**, **productivity analytics**, and **task management** in a privacy-first gadget. Three buyer groups matter now:

1. **Work-from-home people** — want desk-time awareness, break/stretch nudges, focus tracking, without being on camera.
2. **People who need help organizing** — task journals, due-date nagging, diligence scores, daily routines.
3. **People who want some company** — the 4 personas (Coach, Critic, Sweet, Friend) and the character faces (DeskBuddy, DeskAura, DeskCat, DeskWho, DeskBit) make it feel alive, not like a dashboard.

**The thesis changed from v1.** The author has chosen:

- **Product:** assembled, ready-to-use devices (ESP32-C3 devboard + GC9A01 round screen + LD2410 radar, 3D-printed enclosure). BOM ≈ **US$10**.
- **Distribution:** small-batch craft sales on Etsy, Mercado Livre, Amazon — **below certification radar for now**, executed with care and a graduated compliance plan.
- **Goal:** a small **portfolio** ("a roster") of **independent ESP-based products** — DeskBuddy first, then 2–3 unrelated ESP devices, launched **one at a time**, together producing a living income.
- **Status:** firmware still in development — **multi-language support** (Portuguese, Spanish, English) is the last major build item and is itself a market-unlock (see §6).

The strategic insight from v1 still holds and gets *stronger* in a craft context: **the personality and behaviour engine are the product; the radar and screen are the delivery vehicle.** Etsy and Mercado Livre buyers pay for personality, story, and gifting — exactly what DeskBuddy is. Nobody in that market sells a device that *talks to you at your desk*.

---

## 2. Product & Positioning

### 2.1 What ships in the box (consumer framing)

- DeskBuddy unit (enclosure, screen, radar, devboard, ready to plug into USB).
- USB-C cable (device-only — **do not ship an AC charger**, see §5 compliance note).
- 1-page quick-start (language + name + persona selection on first boot).
- A printed **test certificate** ("I was hand-tested on <date>") — trust signal, review lever.

### 2.2 Positioning statements

| Buyer | Message |
|---|---|
| WFH | "Knows when you're at your desk and when you've been sitting too long — without a camera." |
| Organizing | "Reminds you about the task you keep forgetting. Tracks your diligence so you can see the week." |
| Companion | "Pick a coach, a critic, a sweetheart, or a friend. Some people just want the roasts." |
| Gift buyer | "The desk buddy for the person who works from home and misses having a co-worker." |

### 2.3 The out-of-box AI decision (critical for a consumer product)

The current "bring your own Groq key" model **does not work for non-technical buyers** — nobody on Etsy will create a developer API key. Three options:

| Option | Fit |
|---|---|
| **Offline-first default (recommended)** | Ship with AI **off**, personality mode **on**. The local quote library (~300 quotes: 4 personas × 5 quotes × 15 events) already delivers ~90% of the charm with zero cost, zero support, works anywhere. |
| Optional AI via on-device key entry | Power users can paste a Groq key in the web settings page. Free for you, keeps the AI story alive. |
| Bundled AI / small subscription | Later, only after the offline-first base is proven and support tooling exists. |

**This is the single most important consumerization decision:** default = offline personas; AI = optional upgrade. It keeps marginal cost at $0, kills a whole class of support tickets, and makes certification/enforcement a non-issue for the AI layer.

---

## 3. Market Analysis (craft edition)

### 3.1 Where the buyers are

| Marketplace | Buyer reality | Fit for DeskBuddy | Fees |
|---|---|---|---|
| **Etsy** | Gift & novelty marketplace, personality-driven, high willingness to pay, strongest US/EU traffic | **Best fit** — "companion/gift/WFH decor" is an Etsy-native story | ~6.5% + ~3% payment + $0.20 listing; 12–15% offsite ads when triggered |
| **Mercado Livre** | Dominant in Brazil (pt), Mexico (es), Argentina (es); price-sensitive, huge volume | **Second priority** — unlocked by pt-BR/es firmware + listings; local production = no import friction | ~13–17% + payment/shipping |
| **Amazon (MFN)** | Highest volume, but most compliance friction for uncertified electronics; referral + closing fees eat margin | **Optional / later** — deprioritized until certification decision | ~15% referral + ~$1.80 closing + your shipping |

**Prioritization:** Etsy first (story + margins + lenient on certs) → Mercado Livre second (volume + multi-language payoff) → Amazon last (compliance friction highest; revisit at the §5 trigger).

### 3.2 What you're competing with (craft-market lens)

- **LaMetric TIME (~$199)** — premium pixel clock; not in the Etsy/LatAm craft lane; expensive for Mercado Livre buyers.
- **Aqara FP2 (~$58–83)** — presence sensor only; no display, no personality; not gift-able.
- **Divoom/Timebox (~$50–110)** — pixel-art fun, but no sensing and no coaching.
- **Etsy "funny desk gadget" category** — dozens of low-tech gag items (signs, mugs) sell 100s/mo; **no direct competitor does an animated AI companion**. This is white space.

### 3.3 TAM / SAM / SOM (revised)

- **TAM:** smart desk gadgets + productivity devices + gift novelty.
- **SAM:** WFH-focused gift & self-buyers in US/UK/CA (Etsy) + Brazil/Mexico/Argentina (Mercado Livre).
- **SOM (24 mo):** with multi-language and a small portfolio, **1,000–6,000 units cumulative** is the realistic band. Base case ≈ **40–60 units/month at steady state**.

### 3.4 SWOT (updated for craft model)

| Strengths | Weaknesses |
|---|---|
| ~$10 BOM → strong craft margins | Firmware not finished (multi-language pending) |
| Offline personality works out-of-box | English-only content today — LatAm launch blocked until pt/es |
| No camera = easy privacy sell | 3D-printed case durability/surface finish vs. retail competitors |
| 3 marketplaces, 2 marketplaces speak pt/es | Hand-assembly throughput is the bottleneck |
| Gift-able story + meme-able "roast me" content | Single maintainer; support load from non-technical buyers |

| Opportunities | Threats |
|---|---|
| LatAm white space (imports are expensive there) | Marketplace certification enforcement (esp. ML-Brazil ANATEL, Amazon) |
| Gift seasons (Q4, Valentine's) | Bad reviews on a new listing can kill it |
| 3-language launch doubles/triples addressable buyers | Commodity presence sensors keep undercutting |
| Staggered portfolio of independent ESP products (diversification; each runs its own niche) | Assembly time ceiling per person (~100 units/mo across the roster) |
| "Roast me" virality drives organic listing traffic | Currency/LatAm logistics complexity |
| Module-certification lessons transfer per product | Each unrelated product needs its own build tooling + firmware cycle |

---

## 4. Business Model — Finished-Product Craft Play

### 4.1 Unit economics (with your ~$10 BOM)

| Line | Amount |
|---|---|
| BOM (devboard + screen + radar + case + wiring) | $10.00 |
| Packaging + inserts (box, cable, test card) | $2.00 |
| Platform fees (varies) | $5–11 |
| Shipping | buyer-paid or $4–8 |
| **Net per unit (before your labour)** | **$28–52 depending on price + platform** |

### 4.2 Price × platform → net/unit

| Price | Etsy (~9.5% fees) | Mercado Livre (~17%) | Amazon MFN (~15%+closing) |
|---|---|---|---|
| **$49** | ~$32 | ~$28 | ~$28 |
| **$69** | ~$47 | ~$42 | ~$42 |
| **$89** | ~$62 | ~$56 | ~$55 |

*Rounding; excludes buyer-paid shipping and your assembly labour. Mercado Livre numbers assume the local-currency equivalent.*

**Recommended craft pricing:** **$69–89** on Etsy (personality + gift premium), **$59–79-equivalent** on Mercado Livre (local competitiveness). Stay **out of the $39-and-under trap** — that's where commodity sellers fight on price.

### 4.3 "What it takes to live from this" (the honest math)

| Monthly net target | Units/mo @ ~$40 net | Units/mo @ ~$55 net |
|---|---|---|
| $1,000 | ~25 | ~18 |
| $2,000 | ~50 | ~36 |
| $3,000 | ~75 | ~55 |
| $4,000 | ~100 | ~73 |

**The constraint is not demand — it's assembly throughput.** At a realistic 1–1.5 h/unit (wire + flash + test + box), 50 units/month = 50–75 h/month just in production, before support and firmware time. **To make a living, you need either the portfolio to average ~35–55 net/unit, or assembly below ~45 min/unit.** Levers:

- **Batch operations:** flash/configure via one jig; test harness that checks radar + screen in <5 min; 10-unit box packs.
- **Pre-certified module stack** (see §5) — devboard + module radar keep wiring to 3 connections.
- **Pre-sold inventory during spikes:** Q4 (Etsy) and November (ML Cyber Week) need 2–3× normal stock ready in October.

> **Portfolio note:** the $2,000–4,000/mo targets are reached across **multiple products × 15–25 units/mo each**, not by DeskBuddy alone hitting 100 units/mo. Since each product has its own board, screen, and firmware (no shared tooling), throughput planning is **per-product** — the jig/test harness must be rebuilt for each new device (see §4.4).

### 4.4 The portfolio play ("a roster of products")

The roster is **not** DeskBuddy derivatives — it's **3–4 independent, unrelated ESP-based products**. DeskBuddy is product #1; the rest are other device ideas on different boards/screens. What unites them is the **business**, not the hardware.

**Why a roster (diversification):**
- No single product has to carry the whole living — 3–4 products at 15–25 units/mo each replaces one product trying to hit 100/mo.
- Demand risk is spread: a flop or a seasonal dip in one niche doesn't kill the business; different niches peak at different times.
- Marketplace accounts, review track record, and seller reputation accumulate and compound across every product you list.

**What actually transfers between products (the playbook):**
- Marketplace accounts + reputation + listing/review patterns (founder batch → reviews → organic rank).
- Module-certification lessons and the "USB-only, pre-certified modules, small-batch" compliance playbook (§5).
- Support scripts, setup-video/FAQ templates, and first-boot UX standards.
- The maker-brand trust: "the builder of desk companions" — each product cross-promotes the others.

**What does *not* transfer (per-product cost):**
- Different boards/screens → new flashing jig, test harness, wiring skills, and firmware cycle for each.
- New enclosure design, listing photos, and localization content.
- The certification "small-batch" clock restarts from zero for each product.

**The stagger rule (one at a time):**
1. DeskBuddy launches first and must clear its Phase 1 gate (§8) — steady ≥ 15 units/mo, ≥ 4.5★, support under control.
2. Only then does the **next unrelated product** enter development (own tooling, own cycle).
3. Hard cap: **1 product in development + 1 being scaled** at any time — solo-maintainer protection.

**Portfolio mix example (base case, ~$2,500/mo):** DeskBuddy 20 @ ~$47 net + Product B 18 @ ~$35 + Product C 12 @ ~$30 ≈ **50 units, ~$2,350/mo net** — with no single product exceeding 20 units/mo.

---

## 5. Production, Compliance & Risk Reality

### 5.1 Production flow (target < 45 min/unit)

1. Pre-flash + pre-configure boards in batches (language/persona default).
2. Assemble (3 wires + screen + radar to devboard) → glue into case.
3. Automated soak test (radar reports presence, screen cycles faces, WiFi + captive portal up).
4. Test certificate card, pack, list.

### 5.2 Compliance reality check (honest, not legal advice)

"Below the radar" is a **volume strategy, not a free pass**. What's actually true:

- **The module stack helps you.** The ESP32-C3 module and the HLK-LD2410 are **individually FCC/CE certified**. Shipping an assembly of pre-certified modules is *materially* lower-risk than a bare-chip design, and it's your best defensive card.
- **Etsy:** effectively no electronics enforcement at craft volume. Lowest friction.
- **Mercado Livre Brazil:** the most likely enforcement point — **ANATEL** homologation can be requested for RF electronics; Brazil's agency is more active than most. Start with small ML batches, watch for listing flags, and have the module datasheets/certificates ready to answer questions.
- **Amazon:** highest friction. They can request FCC test reports/invoices, and **shipping an AC adapter/charger triggers adapter-compliance document requests**. Rule: **USB-only, no charger in the box** — on all marketplaces.
- **Trigger to certify properly:** when a single product approaches **~500 units/year** or a marketplace demands documentation, budget for a real compliance pass: **FCC/CE testing ~$2,000–5,000 per product; ANATEL ~$1,000–3,000 + agent fees**. Until then, keep batches small and diversified across the portfolio.

Keep the module certificates on file in `tools/` as a cheap insurance asset.

### 5.3 Support load (non-technical buyers)

- Setup = plugin + captive portal (WiFi provisioning is already built). Optimize **first-boot UX**: language picker, name entry, persona picker — this is the make-or-break moment for reviews.
- Ship with **AI off by default** (§2.3) → removes API-key support entirely.
- A 2-minute setup video + written FAQ attached to each listing cuts tickets by ~50%.

---

## 6. Multi-Language: The Market Unlock

Multi-language is **not a nice-to-have — it's the second market engine.** The codebase already has localization groundwork (`PROGMEM LangStrings` struct, `.rodata` baseline ~19.4 KB noted in `behaviour-assessment.md` §8). Scope:

| Language | Unlocks | Priority |
|---|---|---|
| **pt-BR** | Mercado Livre Brazil (largest ML market) | **1** |
| **es (MX/AR)** | Mercado Livre Mexico + Argentina, plus your own local market | **2** |
| **en** | Etsy US/UK/CA | baseline |
| fr/de (later) | Etsy EU | 3 |

Content to localize: UI strings, **all persona quotes (~300)**, journal/task screens, weather descriptions, web dashboard, setup wizard. Production method: AI-assisted translation → **native-review pass** (an auto-translated roast is a bad roast). Multi-language is why a single product can run on both Etsy (en) and Mercado Livre (pt/es) with zero extra hardware.

---

## 7. Marketing Strategy (craft edition)

### 7.1 Positioning & messaging

Pillars: **privacy-first** ("knows you're at your desk, never watches you") · **personality** ("pick your coach, critic, sweetheart, or friend") · **companion/gift** ("for the person who works from home and misses a co-worker") · **organizing** ("reminds you about the task you keep forgetting").

### 7.2 Marketplace SEO & conversion

- **Etsy:** keywords in title/tags — *desk companion, WFH gift, productivity gadget, smart clock, presence sensor, ADHD gift, home office decor, funny desk gadget*. 6–10 photos: hero (lit, on a desk), size reference, faceplates gallery, persona examples, setup steps, gift-ready packaging. Enable personalization (engraved name / chosen persona / face colour).
- **Mercado Livre:** same in **pt-BR and es**; Mercado Ads for the first 30–60 days to seed sales velocity (reviews), then let organic ranking carry.
- **Reviews program:** every box gets a QR code → 2-step review ask; the hand-test card is the hook. A new listing needs its first 10 reviews fast — price the first 10 units as a break-even "founder batch."

### 7.3 The demand engine (feeds all listings)

The v1 insight still applies and now doubles as **organic listing traffic**:

1. **"DeskBuddy roasts me"** — Critic-persona screenshots/clips; the viral hook.
2. **Build log + demo videos** on Reddit (r/homeassistant, r/esp32, r/desksetup, r/ADHD_Programmers), Hackaday, YouTube/TikTok, Hacker News Show HN.
3. **Screen-content showcases** (faceplates, mood ring, task diligence) — designed to be re-shared.
4. **Creator seeding:** 3–5 demo units to desk-setup / smart-home creators. A single strong video can feed a marketplace listing for months.

### 7.4 Seasonality

- **Etsy Q4 (Oct–Dec)** can be 40%+ of the year. Have 2–3× stock in October, gift packaging ready, "guaranteed by Christmas" cut-offs advertised in September.
- **Mercado Livre:** Brazil's Black Friday (November) + Christmas; prep in October.
- **Valentine's & "back to school/dorm" (Aug–Sep)** secondary spikes — Lean into "an office plant that talks back."

### 7.5 KPI dashboard

| Metric | Base case (12 mo) |
|---|---|
| Etsy units/listing/mo (steady state) | 15–30 |
| Mercado Livre units/mo (after pt launch) | 10–25 |
| Avg net/unit | $40–50 |
| Reviews avg (Etsy + ML) | ≥ 4.5★, ≥ 10 reviews per product |
| Support tickets per 10 units | < 1 |
| Assembly throughput | ≤ 45 min/unit |

---

## 8. Roadmap with Probable Outcomes

Each phase has a **gate** — don't advance production scale until the gate clears.

### Phase 0 — Finish product (months 0–3)

| Task | Detail |
|---|---|
| **Multi-language** | pt-BR + es content, first-boot language/persona wizard, web dashboard localization |
| **Enclosure** | Finalize STLs for all variants; surface-finish pass (paint/vinyl); assembly guide |
| **Consumer UX** | First-boot wizard, AI-off-by-default, setup video, FAQ |
| **Production kit** | Jig, test harness, batch flashing, test-certificate card |
| **Compliance pack** | Collect module certificates; USB-only rule confirmed |
| **Launch assets** | 10+ photos, listing copy ×3 languages, pricing final |

**Gate: 3 hand-built units pass 48-h soak; first-boot wizard works with zero printed instructions.**

### Phase 1 — First listings (months 3–5)

| Task | Detail |
|---|---|
| Etsy US launch + Mercado Livre BR (pt) launch | 20–30-unit founder batch, break-even pricing |
| Seeding | Reddit/Show HN/Hackaday posts; first 10 reviews program |
| Ads | Mercado Ads 30-day test, Etsy offsite ads enabled, stop losers weekly |

**Gate: ≥ 10 reviews ≥ 4.5★ across listings; ≥ 4 units/week by month 5; support < 1 ticket/10 units.**

| Outcome | Result |
|---|---|
| Conservative (p≈0.35) | 12–20 units/mo, reviews slow — tighten listing/photos, extend ad test 1 month |
| **Base (p≈0.45)** | **20–35 units/mo, strong reviews, clear repeat-buyer signal** |
| Optimistic (p≈0.2) | 50+ units/mo, one creator video goes 50k+ views |

### Phase 2 — Scale DeskBuddy (months 6–12)

| Task | Detail |
|---|---|
| Mercado Livre **Mexico + Argentina (es)** | multi-language payoff |
| Q4 prep | 2–3× inventory by October, gift cut-off promos |
| Optimize assembly | < 45 min/unit, batch production, QC log |
| **Product B pre-study** | Research only (niche, board choice, unit economics) — no build until DeskBuddy's gate clears |

**Gate: ≥ 40 DeskBuddy units/mo, ≥ 50% margin/unit, reviews ≥ 4.5★ sustained — only then does Product B enter development.**

| Outcome | Result |
|---|---|
| Conservative (p≈0.35) | 30–40 units/mo, ~$1,200/mo net — side income |
| **Base (p≈0.45)** | **50–70 units/mo, ~$2,000–2,800/mo net — replaces a part-time job** |
| Optimistic (p≈0.2) | 100+ units/mo in Q4, ~$4,000/mo net — approaches a living |

### Phase 3 — Roster of products (months 12–24)

| Task | Detail |
|---|---|
| **Product B development** (unrelated ESP device, own board/tooling/firmware) | full R&D cycle: design → enclosure → pilot → listing; repeats DeskBuddy's Phase 0–1 pattern |
| **Certification decision (per product)** | revisit FCC/CE/ANATEL at the §5 trigger (~500 units/yr per product) or if a marketplace demands it |
| Optional premium layer for DeskBuddy | facepacks/persona packs as digital add-ons (new revenue, no inventory) |
| Amazon MFN evaluation | only after the certification decision |
| **Product C pre-study** | research only; product in development must be launched before C builds |

**Gate (rolling): exactly 1 product in development + 1 being scaled; each new product launches only after the prior one clears its listing gate.**

| Outcome | Result |
|---|---|
| Conservative (p≈0.35) | 60–80 units/mo across 2–3 products, ~$2,500/mo net |
| **Base (p≈0.45)** | **80–120 units/mo across 3–4 products, ~$3,500–5,000/mo net — a genuine living from the roster** |
| Optimistic (p≈0.2) | 150+ units/mo incl. seasons, ~$7,000/mo net + digital upsell |

---

## 9. Risks & Mitigations (updated)

| Risk | Severity | Mitigation |
|---|---|---|
| **Assembly throughput ceiling** | High | Jig + test harness + batch ops per product; roster so no single product must hit 100/mo; target < 45 min/unit |
| **Certification enforcement (ML-Brazil / Amazon)** | Medium | Start Etsy-first, small ML batches, pre-certified modules, USB-only box; graduate at ~500 units/yr **per product** |
| **Spreading thin across unrelated products** | High | Hard cap: **1 product in development + 1 being scaled**; Product B build starts only after DeskBuddy's gate clears |
| **Per-product tooling cost** | Medium | Each board/screen needs its own jig + firmware cycle — plan Product B/C budgets in advance; reuse playbook, not hardware |
| **Bad review kills a new listing** | Medium | Founder batch at break-even for fast reviews; QC test-card in box; fast seller-support responses |
| **Non-technical buyer support** | Medium | AI-off default, first-boot wizard, video + FAQ; no API keys to explain |
| **Firmware still unfinished (multi-language)** | High | Phase 0 locks content scope; native-review pass; ship en first, pt/es as point releases if needed |
| **Currency/logistics in LatAm** | Medium | Local production + local-currency pricing; ML Envios for shipping |
| **Persona/content quality in translation** | Medium | AI-assisted translation + native review; keep roasts culturally appropriate per market |
| **Solo-maintainer burnout** | High | Cap concurrent products (1 dev + 1 scaling); batch production; time-box support; don't over-order inventory |
| **Groq/AI dependency** | Low (offline-first) | Default mode needs no AI; AI is optional upgrade |

---

## 10. Appendix — Cheat Sheets

### 10.1 Net/unit matrix ($10 BOM + $2 packaging)

| Price | Etsy | Mercado Livre | Amazon MFN |
|---|---|---|---|
| $49 | ~$32 | ~$28 | ~$28 |
| $69 | ~$47 | ~$42 | ~$42 |
| $89 | ~$62 | ~$56 | ~$55 |

### 10.2 Income targets

| Monthly net | @ $40/unit | @ $55/unit |
|---|---|---|
| $1,000 | 25 units | 18 units |
| $2,000 | 50 units | 36 units |
| $3,000 | 75 units | 55 units |
| $4,000 | 100 units | 73 units |

### 10.3 Roster mix to ~$2,500/mo (base case)

DeskBuddy 20 @ ~$47 net + Product B 18 @ ~$35 + Product C 12 @ ~$30 ≈ **50 units, ~$2,350/mo net** — no single product exceeds 20 units/mo; each is an independent ESP device on its own board.

### 10.4 The one-line strategy

> Sell the **personality**, not the sensor: ship an offline-first, multi-language, hand-tested companion at $69–89 on Etsy and $59–79 on Mercado Livre, run it as product #1 in a roster of independent ESP devices launched one at a time, and scale volume only as fast as assembly throughput — and certification — allow.
