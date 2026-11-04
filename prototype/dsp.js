// BoomTown Beats Pump 96 — DSP core (browser prototype).
// A tempo-synced volume curve ("sidechain pump" without a sidechain): the user draws the curve, it repeats every
// Rate (1/32 .. 2 bars), Depth scales it, Offset shifts its phase, Smooth rounds its corners, and Band picks whether it
// works on the whole signal, only the lows or only the highs (complementary split, so the bands sum back flat).
// Transport: the host gives the beat position (ppq) per block; when the host is stopped the curve free-runs.
function pumpModule(){
  'use strict';
  const TAU = Math.PI * 2;
  const dbl = d => Math.pow(10, d / 20);
  const clampf = (v, a, b) => v < a ? a : (v > b ? b : v);

  function setLP2(o, fs, f, q){
    f = clampf(f, 20, fs * 0.45);
    const w = TAU * f / fs, sn = Math.sin(w), cs = Math.cos(w), al = sn / (2 * q), a0 = 1 + al;
    o[0] = (1 - cs) / 2 / a0; o[1] = (1 - cs) / a0; o[2] = o[0]; o[3] = -2 * cs / a0; o[4] = (1 - al) / a0;
  }
  function setHP2(o, fs, f, q){
    f = clampf(f, 5, fs * 0.45);
    const w = TAU * f / fs, sn = Math.sin(w), cs = Math.cos(w), al = sn / (2 * q), a0 = 1 + al;
    o[0] = (1 + cs) / 2 / a0; o[1] = -(1 + cs) / a0; o[2] = o[0]; o[3] = -2 * cs / a0; o[4] = (1 - al) / a0;
  }
  function bq(c, s, x){ const y = c[0] * x + s[0]; s[0] = c[1] * x - c[3] * y + s[1]; s[1] = c[2] * x - c[4] * y; return y; }

  const RATES = ['1/32', '1/16', '1/8', '1/4', '1/2', '1/1', '2/1'];
  const RATE_BEATS = [0.125, 0.25, 0.5, 1, 2, 4, 8];
  const BANDS = ['Full', 'Lows', 'Highs'];
  const SHAPES = ['Classic', 'Smooth', 'Hard', 'Gate', 'Ramp', 'Bounce', 'Custom'];
  // curve points: [x, y] with x in 0..1 (one Rate period), y = gain 0..1; x of the first/last point is fixed at 0/1
  const SHAPE_POINTS = [
    [[0,0],[0.04,0.05],[0.1,0.35],[0.2,0.68],[0.32,0.86],[0.5,0.96],[1,1]],          // Classic: fast dip, sidechain-style release
    [[0,0.1],[0.12,0.2],[0.3,0.55],[0.5,0.82],[0.7,0.95],[1,1]],                     // Smooth
    [[0,0],[0.18,0],[0.24,0.9],[0.4,1],[1,1]],                                       // Hard: hold low, snap back
    [[0,1],[0.5,1],[0.5,0],[1,0]],                                                   // Gate: on/off halves
    [[0,0],[1,1]],                                                                   // Ramp
    [[0,1],[0.25,0.3],[0.5,1],[0.75,0.3],[1,1]],                                     // Bounce: two dips per period
  ];
  const LUT = 512;
  const DEFAULTS = { depth:80, rate:3, shape:0, smooth:2, offset:0, band:0, split:150, out:0, bypass:0 };
  const LOG = { split:1 };
  const NOSMOOTH = { rate:1, shape:1, band:1 };
  const BLOCK = 32;

  // sample the point list into a LUT; points with equal x make a vertical edge (the later point wins after it)
  function buildLut(pts, lut){
    const n = lut.length; let j = 0;                 // n = LUT + 1 entries: x = 0 .. 1 inclusive
    for (let i = 0; i < n; i++){
      const x = i / (n - 1);
      while (j < pts.length - 2 && pts[j + 1][0] <= x) j++;
      const a = pts[j], b = pts[j + 1], dx = b[0] - a[0];
      lut[i] = dx <= 1e-9 ? b[1] : a[1] + (b[1] - a[1]) * clampf((x - a[0]) / dx, 0, 1);
    }
  }
  function sanitize(pts){
    const p = pts.map(q => [clampf(+q[0] || 0, 0, 1), clampf(+q[1] || 0, 0, 1)]).sort((a, b) => a[0] - b[0]);
    if (p.length < 2) return [[0, 1], [1, 1]];
    p[0][0] = 0; p[p.length - 1][0] = 1;
    return p;
  }

  class Engine {
    constructor(fs){
      this.fs = fs;
      this.tgt = Object.assign({}, DEFAULTS);
      this.cur = Object.assign({}, DEFAULTS);
      this.cLP = new Float64Array(5);
      this.cHP = new Float64Array(5);
      const s4 = () => [new Float64Array(2), new Float64Array(2)];
      this.sLP = [s4(), s4()]; this.sHP = [s4(), s4()];
      this.lut = new Float64Array(LUT + 1); this.points = SHAPE_POINTS[0].map(p => p.slice()); buildLut(this.points, this.lut);
      this.bpm = 120; this.ppq = 0; this.playing = false; this.freePpq = 0;
      this.g = 1; this.phase = 0; this.bandAmt = [1, 1];     // per band: how much of the curve applies (lows, highs)
      this.mCount = 0; this.inPk = 0; this.outPk = 0;
      this.onMeter = null;
      this.design(); this.derive();
    }
    setParams(p){
      if ('shape' in p && p.shape !== this.tgt.shape && (p.shape | 0) < SHAPE_POINTS.length) this.setPoints(SHAPE_POINTS[p.shape | 0]);
      Object.assign(this.tgt, p);
    }
    setPoints(pts){ this.points = sanitize(pts); buildLut(this.points, this.lut); }
    // ppq = beat position at the start of the next block (null when the host is stopped)
    setTransport(ppq, bpm){
      if (bpm > 20 && bpm < 999) this.bpm = bpm;
      if (ppq === null || ppq === undefined){ this.playing = false; }
      else { this.playing = true; this.ppq = ppq; }
    }
    smooth(){
      const t = this.tgt, c = this.cur, a = 1 - Math.exp(-BLOCK / (this.fs * 0.025)); let moved = false;
      for (const k in t){
        const target = t[k], v = c[k]; if (v === target) continue;
        let nv;
        if (NOSMOOTH[k]) nv = target;
        else if (LOG[k]){ nv = v * Math.pow(target / v, a); if (Math.abs(nv / target - 1) < 1e-4) nv = target; }
        else { nv = v + (target - v) * a; if (Math.abs(nv - target) < 1e-5) nv = target; }
        c[k] = nv; moved = true;
      }
      return moved;
    }
    // Linkwitz-Riley 4 split: lows + highs sum to an allpass (flat magnitude)
    design(){ setLP2(this.cLP, this.fs, this.cur.split, 0.7071); setHP2(this.cHP, this.fs, this.cur.split, 0.7071); }
    derive(){
      const c = this.cur;
      this.depth = c.depth / 100;
      this.period = RATE_BEATS[clampf(c.rate | 0, 0, 6)];
      this.off = c.offset / 100;
      this.kSm = c.smooth <= 0.01 ? 1 : 1 - Math.exp(-1 / (this.fs * c.smooth / 1000));
      this.outG = dbl(c.out); this.byp = c.bypass;
      const b = c.band | 0;
      this.split = b !== 0;
      this.applyLo = b !== 2; this.applyHi = b !== 1;
    }
    curveAt(ph){
      const x = ph * LUT, i = Math.min(LUT - 1, Math.floor(x)), f = x - i;
      return this.lut[i] + (this.lut[i + 1] - this.lut[i]) * f;
    }
    process(iL, iR, oL, oR, n){
      const dpq = this.bpm / 60 / this.fs;
      let pos = this.playing ? this.ppq : this.freePpq;
      for (let b0 = 0; b0 < n; b0 += BLOCK){
        const e = Math.min(n, b0 + BLOCK);
        if (this.smooth()) this.design();
        this.derive();
        const per = this.period, off = this.off;
        for (let s = b0; s < e; s++){
          let ph = pos / per + off; ph -= Math.floor(ph);
          const target = 1 - this.depth * (1 - this.curveAt(ph));
          this.g += (target - this.g) * this.kSm;
          const g = this.g;
          pos += dpq;
          const inL = iL[s], inR = iR[s];
          let yl, yr;
          if (!this.split){ yl = inL * g; yr = inR * g; }
          else {
            const s0 = this.sLP[0], s1 = this.sLP[1], h0 = this.sHP[0], h1 = this.sHP[1];
            const lol = bq(this.cLP, s0[1], bq(this.cLP, s0[0], inL)), lor = bq(this.cLP, s1[1], bq(this.cLP, s1[0], inR));
            const hil = bq(this.cHP, h0[1], bq(this.cHP, h0[0], inL)), hir = bq(this.cHP, h1[1], bq(this.cHP, h1[0], inR));
            const gl = this.applyLo ? g : 1, gh = this.applyHi ? g : 1;
            yl = lol * gl + hil * gh; yr = lor * gl + hir * gh;
          }
          yl *= this.outG; yr *= this.outG;
          yl += (inL - yl) * this.byp; yr += (inR - yr) * this.byp;
          oL[s] = yl; oR[s] = yr;
          const pi = Math.max(Math.abs(inL), Math.abs(inR)); if (pi > this.inPk) this.inPk = pi;
          const po = Math.max(Math.abs(yl), Math.abs(yr)); if (po > this.outPk) this.outPk = po;
          this.phase = ph;
        }
        this.mCount += e - b0;
      }
      this.ppq = pos; this.freePpq = pos;               // when the host stops, the curve free-runs from here
      if (this.mCount >= this.fs / 60){
        this.mCount = 0;
        if (this.onMeter) this.onMeter({ in:this.inPk, out:this.outPk, phase:this.phase, gain:this.g });
        this.inPk = 0; this.outPk = 0;
      }
    }
  }
  return { Engine, DEFAULTS, RATES, RATE_BEATS, BANDS, SHAPES, SHAPE_POINTS, sanitize, buildLut, LUT };
}
