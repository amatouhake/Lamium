// Lamium mockup kit. Load after lamium-data.js (generated from src/), then draw with Lamium.*.
// Every color, size, row and string comes from that file: a mockup never retypes them.
// Drawing mirrors src/ui/Widgets.cpp, ListViewWidgets.cpp and SettingsTableView.cpp in GUI units.
// How to make a mockup with it: docs/demos/README.md "Making a mockup".
(function () {
  'use strict';
  const D = window.LAMIUM_DATA;
  const errors = [];
  const L = (window.Lamium = { data: D, lang: 1, errors });

  // ---- Errors are shown on the page, never only in the console ----
  function report(message) {
    errors.push(message);
    let box = document.getElementById('lk-errors');
    if (!box && document.body) {
      box = document.createElement('div');
      box.id = 'lk-errors';
      box.className = 'lk-errors';
      document.body.prepend(box);
    }
    if (box) box.textContent = 'このモックアップでエラー:\n' + errors.join('\n');
  }
  L.report = report;
  // The browser's ResizeObserver note is not the mockup's fault.
  window.addEventListener('error', (e) => /ResizeObserver loop/.test(e.message) || report(`${e.message} (${(e.filename || '').split('/').pop()}:${e.lineno})`));
  window.addEventListener('unhandledrejection', (e) => report('Promise: ' + (e.reason && e.reason.message || e.reason)));
  if (!D) { report('lamium-data.js が読み込まれていません（kit.js より前に読み込む）'); return; }

  // ---- Language and strings ----
  const codes = D.locales; // en, ja, zh_CN, zh_TW
  L.setLang = (code) => { const i = codes.indexOf(code); if (i >= 0) L.lang = i; };
  L.langCode = () => codes[L.lang];
  L.pick = (value) => (Array.isArray(value) ? value[L.lang] ?? value[0] : value ?? '');
  // A real string by its key in src/ui/Translations*.h; {} (and {:.1f}-style) fill from args in order.
  L.t = (key, ...args) => {
    const entry = D.strings[key];
    if (!entry) { report(`文字列キーがありません: ${key}`); return key; }
    let i = 0;
    return L.pick(entry).replace(/\{[^}]*\}/g, (m) => (i < args.length ? String(args[i++]) : m));
  };
  L.has = (key) => key in D.strings;

  // ---- Colors (palette tokens only) ----
  const pal = D.palette.lamium, van = D.palette.vanilla;
  function hexOf(name) {
    const v = name.startsWith('vanilla.') ? van[name.slice(8)] : pal[name];
    if (!v) { report(`色トークンがありません: ${name}`); return '#ff00ff'; }
    return v;
  }
  L.color = (name, alpha = 1) => {
    const h = hexOf(name);
    if (alpha >= 1) return h;
    const n = parseInt(h.slice(1), 16);
    return `rgba(${n >> 16},${(n >> 8) & 255},${n & 255},${alpha})`;
  };
  (function tokens() {
    const root = document.documentElement.style;
    for (const [k, v] of Object.entries(pal)) root.setProperty('--lm-' + k, v);
    for (const [k, v] of Object.entries(van)) root.setProperty('--lmv-' + k, v);
  })();
  const S = D.sizes, W = S.widgets, T = S.settings;
  L.sizes = S;

  // ---- Text measure (the game font is approximated by the stage font) ----
  const canvas = document.createElement('canvas').getContext('2d');
  L.textWidth = (text) => {
    canvas.font = '8px ' + getComputedStyle(document.documentElement).getPropertyValue('--game-font');
    return Math.ceil(canvas.measureText(String(text)).width);
  };

  // ---- Primitives: each adds an absolutely placed element to `p` (a stage or a group) ----
  function box(p, x, y, w, h) {
    const e = document.createElement('div');
    Object.assign(e.style, { left: x + 'px', top: y + 'px', width: Math.max(0, w) + 'px', height: Math.max(0, h) + 'px' });
    p.append(e);
    return e;
  }
  L.box = box;
  L.fill = (p, x, y, w, h, color, alpha = 1) => { const e = box(p, x, y, w, h); e.style.background = L.color(color, alpha); return e; };
  L.frame = (p, x, y, w, h, color, alpha = 1) => {
    const e = box(p, x, y, w, h);
    e.style.boxShadow = `inset 0 0 0 1px ${L.color(color, alpha)}`;
    return e;
  };
  // A group: children are placed relative to (x, y); use for a panel that moves as one.
  L.group = (p, x = 0, y = 0) => box(p, x, y, 0, 0);
  L.label = (p, x, y, w, text, { color = 'text', align = 'left', shadow = true, alpha = 1 } = {}) => {
    const e = box(p, x, y, w, 9);
    e.className = 'lk-t' + (shadow ? '' : ' flat') + (align === 'right' ? ' r' : align === 'center' ? ' c' : '');
    e.style.color = L.color(color, alpha);
    e.textContent = text;
    e.title = text;
    return e;
  };
  L.panel = (p, x, y, w, h, { floating = false, frame = false } = {}) => {
    const e = L.fill(p, x, y, w, h, 'panel', floating ? S.opacity.floating : S.opacity.screen);
    if (frame) L.frame(p, x, y, w, h, 'white', .14);
    return e;
  };
  L.rule = (p, x, y, w, alpha = .14) => L.fill(p, x, y, w, 1, 'white', alpha);
  L.switch = (p, x, y, on) => {
    const w = W.switchWidth, h = W.switchHeight, knob = h - 2, kx = on ? x + w - 1 - knob : x + 1;
    L.fill(p, x, y, w, h, on ? 'accentDeep' : 'off');
    L.frame(p, x, y, w, h, on ? 'accent' : 'controlEdge');
    L.fill(p, kx, y + 1, knob, knob, on ? 'knobOn' : 'knobOff');
    L.fill(p, kx, y + 1 + knob - 2, knob, 2, on ? 'gripOn' : 'gripOff');
  };
  L.slider = (p, x, y, w, fraction) => {
    const kw = W.sliderKnobWidth, kh = W.sliderKnobHeight, th = W.sliderTrackHeight;
    const f = Math.min(1, Math.max(0, fraction || 0)), kx = x + (w - kw) * f, ty = y + (kh - th) / 2, split = kx + kw / 2;
    L.fill(p, x, ty, split - x, th, 'accentDeep');
    L.fill(p, split, ty, x + w - split, th, 'off');
    L.frame(p, x, ty, w, th, 'controlEdge');
    L.fill(p, x + 1, ty, split - x - 1, 1, 'accent');
    L.fill(p, kx, y, kw, kh, 'knobOn');
    L.fill(p, kx, y + kh - 2, kw, 2, 'gripOff');
    L.frame(p, kx, y, kw, kh, 'controlEdge');
  };
  L.arrow = (p, x, y, left, color = 'dim') => {
    for (let i = 0; i < 3; i++) L.fill(p, left ? x + i : x + 2 - i, y + 2 - i, 1, 1 + 2 * i, color);
  };
  L.chevron = (p, x, y, expanded, color = 'dim') => {
    for (let i = 0; i < 3; i++) {
      if (expanded) L.fill(p, x + i, y + 1 + i, 5 - 2 * i, 1, color);
      else L.fill(p, x + 1 + i, y + i, 1, 5 - 2 * i, color);
    }
  };
  // `- value +` for numbers, `< value >` for choices; warn draws it in the warning color.
  L.stepper = (p, x, y, w, h, text, { numeric = false, warn = false, low = true, high = true, cell = T.arrowWidth } = {}) => {
    const ty = y + (h - 10) / 2 + 1;
    for (const side of [0, 1]) {
      const cx = side ? x + w - cell : x, enabled = side ? high : low;
      L.fill(p, cx, y, cell, h, 'keyFill');
      const mark = enabled ? 'dim' : 'faint';
      if (numeric) L.label(p, cx, ty, cell, side ? '+' : '-', { color: mark, align: 'center' });
      else L.arrow(p, cx + Math.floor((cell - 3) / 2), y + Math.floor((h - 5) / 2), side === 0, mark);
    }
    L.frame(p, x, y, w, h, warn ? 'warning' : 'keyEdge');
    L.label(p, x + cell + 1, ty, w - 2 * cell - 2, text, { color: warn ? 'warning' : 'text', align: 'center' });
  };
  // Buttons: plain, primary (main action, or on/selected), danger (plain until pointed at), armed (danger after one press).
  L.button = (p, x, y, w, h, text, { kind = 'plain', disabled = false, hovered = false } = {}) => {
    const look = disabled ? ['keyFill', 'keyEdge', 'faint']
      : kind === 'primary' ? ['accentDeep', 'accent', 'text']
      : kind === 'armed' ? ['danger', 'dangerEdge', 'text']
      : kind === 'danger' && hovered ? ['keyFill', 'dangerEdge', 'dangerText']
      : ['keyFill', 'keyEdge', 'text'];
    L.fill(p, x, y, w, h, look[0]);
    if (hovered && !disabled) L.fill(p, x, y, w, h, 'white', .1);
    L.frame(p, x, y, w, h, look[1]);
    L.label(p, x, y + (h - 9) / 2 + 1, w, text, { color: look[2], align: 'center' });
  };
  L.badge = (p, x, y, text, color = 'experimental') => {
    const w = L.textWidth(text) + 5;
    L.frame(p, x, y + 1, w, T.rowHeight - 3, color);
    L.label(p, x + 3, y + 2, w - 3, text, { color });
    return w;
  };
  L.field = (p, x, y, w, h, { text = '', placeholder = '', focused = false } = {}) => {
    L.fill(p, x, y, w, h, 'shade', .45);
    L.frame(p, x, y, w, h, focused ? 'accent' : 'keyEdge');
    if (text) L.label(p, x + 4, y + 2, w - 8, text + (focused ? '_' : ''));
    else if (placeholder) L.label(p, x + 4, y + 2, w - 8, placeholder, { color: 'faint' });
  };
  L.scrollbar = (p, x, top, track, thumbTop, thumb, hot = false) => {
    L.fill(p, x, top, 2, track, 'white', .08);
    L.fill(p, x, thumbTop, 2, thumb, hot ? 'dim' : 'keyEdge');
  };
  // A tip on Lamium's own screens: the floating panel, a keyEdge edge (warning when it warns), 4 units of padding.
  L.tip = (p, x, y, lines, { warn = false, width } = {}) => {
    lines = [].concat(lines);
    const pad = W.tipPad, w = width ?? Math.max(...lines.map(L.textWidth)) + 2 * pad, h = lines.length * 10 + 2 * pad - 1;
    L.fill(p, x, y, w, h, 'panel', S.opacity.floating);
    L.frame(p, x, y, w, h, warn ? 'warning' : 'keyEdge');
    lines.forEach((line, i) => L.label(p, x + pad, y + pad + i * 10, w - 2 * pad, line));
    return { w, h };
  };
  // One of several on a dark panel: names over a one-unit rule, the chosen one on a faint accent face over an accent bar.
  L.tabs = (p, x, y, w, names, chosen) => {
    let cx = x;
    names.forEach((name, i) => {
      const tw = L.textWidth(name) + 2 * W.tabPad;
      if (i === chosen) { L.fill(p, cx, y, tw, W.tabHeight, 'accent', .16); L.fill(p, cx, y + W.tabHeight - 1, tw, 1, 'accent'); }
      L.label(p, cx, y + 2, tw, name, { color: i === chosen ? 'text' : 'dim', align: 'center' });
      cx += tw;
    });
    L.rule(p, x, y + W.tabHeight, w);
  };
  L.rowBackground = (p, x, y, w, h, selected, hovered) => {
    if (selected) { L.fill(p, x, y, w, h, 'accent', .16); L.frame(p, x, y, w, h, 'accent', .9); }
    else if (hovered) L.fill(p, x, y, w, h, 'white', .07);
  };
  L.guide = (p, x, y, last) => {
    L.fill(p, x, y, 1, last ? T.rowHeight / 2 : T.rowHeight, 'white', .18);
    L.fill(p, x + 1, y + T.rowHeight / 2, 5, 1, 'white', .18);
  };

  // ---- Keys ----
  const keyNames = { 8: 'BACKSPACE', 9: 'TAB', 13: 'ENTER', 16: 'SHIFT', 17: 'CTRL', 18: 'ALT', 27: 'ESC', 32: 'SPACE',
    33: 'PAGE UP', 34: 'PAGE DOWN', 35: 'END', 36: 'HOME', 37: 'LEFT', 38: 'UP', 39: 'RIGHT', 40: 'DOWN', 45: 'INSERT', 46: 'DELETE' };
  // The key column's names for a chord from lamium-data.js ({device, code} tokens). Keyboard names
  // approximate the game's own (it names keys at run time); mouse and wheel use Lamium's strings.
  L.keyNames = (chord) => (chord || []).map(({ device, code, text }) => {
    if (text) return L.pick(text);
    if (device === 'mouse') return code <= 3 ? L.t(['mouse.left', 'mouse.right', 'mouse.middle'][code - 1]) : L.t('mouseButton', code);
    if (device === 'wheel') return L.t(code > 0 ? 'wheelUp' : 'wheelDown');
    if ((code >= 0x30 && code <= 0x39) || (code >= 0x41 && code <= 0x5a)) return String.fromCharCode(code);
    if (code >= 0x70 && code <= 0x87) return 'F' + (code - 0x6f);
    return keyNames[code] || '#' + code;
  });
  // Key caps; tone: plain, filled (same chord elsewhere), outline (overlap or a game click), fixed (not a binding).
  L.keycaps = (p, x, y, width, keys, { tone = 'plain' } = {}) => {
    const fixed = tone === 'fixed', filled = tone === 'filled';
    const edge = fixed ? 'faint' : tone === 'plain' ? 'keyEdge' : 'warning';
    const ink = fixed ? 'faint' : tone === 'outline' ? 'warning' : filled ? 'panel' : 'text';
    let used = 0;
    keys.forEach((key, i) => {
      if (i) { if (used + 6 > width) return; L.label(p, x + used, y + 1, 6, '+', { color: 'faint', align: 'center' }); used += 6; }
      const cw = Math.min(L.textWidth(key) + 6, width - used);
      if (cw < 8) return;
      if (!fixed) L.fill(p, x + used, y, cw, W.capHeight, filled ? 'warning' : 'keyFill');
      L.frame(p, x + used, y, cw, W.capHeight, edge);
      if (!fixed) L.fill(p, x + used + 1, y + W.capHeight - 2, cw - 2, 1, 'shade', filled ? .25 : .5);
      L.label(p, x + used + 3, y + 1, cw - 5, key, { color: ink, shadow: !filled });
      used += cw;
    });
    return used;
  };

  // ---- Stage: a GUI-unit canvas scaled to its column ----
  // stage(container, {width, height, world}) -> the element to draw on (1 px = 1 GUI unit).
  L.stage = (container, { width = 640, height = 380, world = true } = {}) => {
    const outer = document.createElement('div');
    outer.className = 'lk-stage-outer';
    const stage = document.createElement('div');
    stage.className = 'lk-stage';
    Object.assign(stage.style, { width: width + 'px', height: height + 'px' });
    if (world) { const w = document.createElement('div'); w.className = 'lk-world'; stage.append(w); }
    outer.append(stage);
    container.append(outer);
    const fit = () => {
      const scale = Math.max(.25, outer.clientWidth / width);
      stage.style.transform = `scale(${scale})`;
      outer.style.height = Math.ceil(height * scale) + 'px';
    };
    new ResizeObserver(fit).observe(outer);
    fit();
    stage.size = { width, height };
    return stage;
  };

  // ---- Rows ----
  // Copies of the real rows for a category ('section.view' ... or 'nav.hotkeys'), every feature open.
  L.rows = (nav) => {
    const list = D.rows[nav];
    if (!list) { report(`行の一覧がありません: ${nav}`); return []; }
    return list.map((r) => ({ ...r }));
  };
  const indexOf = (rows, id) => {
    const i = rows.findIndex((r) => r.id === id);
    if (i < 0) report(`行がありません: ${id}`);
    return i;
  };
  // Helpers for a proposal: add, change or remove rows by id, marking what changed.
  L.rows.find = (rows, id) => rows[indexOf(rows, id)];
  L.rows.insertAfter = (rows, id, ...added) => {
    let i = indexOf(rows, id);
    if (i < 0) return rows;
    if (rows[i].kind === 'feature') while (rows[i + 1] && rows[i + 1].child) i++; // after its children
    rows.splice(i + 1, 0, ...added.map((r) => ({ changed: true, ...r })));
    return rows;
  };
  L.rows.change = (rows, id, patch) => { const r = L.rows.find(rows, id); if (r) Object.assign(r, patch, { changed: true }); return rows; };
  L.rows.remove = (rows, id) => {
    const i = indexOf(rows, id);
    if (i < 0) return rows;
    let n = 1;
    if (rows[i].kind === 'feature') while (rows[i + n] && rows[i + n].child) n++;
    rows.splice(i, n);
    return rows;
  };
  // A text for a new row: one string for every language, or [en, ja, zh_CN, zh_TW].
  L.text = (value) => (Array.isArray(value) ? value : [value, value, value, value]);

  // ---- The settings screen, as SettingsTableView draws it ----
  // settings(stage, {nav, rows, open, selected, first, footer, hint, x, y}) -> {rowY(id), left, top, table}
  //   nav: the sidebar item ('section.view' ... 'nav.hotkeys'); rows: from L.rows(nav), edited by a proposal;
  //   open: ids of open features (default: the selected one); selected: a row id; footer: the description shown.
  L.settings = (stage, opts = {}) => {
    const nav = opts.nav || 'section.view';
    const all = opts.rows || L.rows(nav);
    const hotkeys = nav === 'nav.hotkeys';
    const open = new Set(opts.open || (opts.selected ? [opts.selected] : []));
    const rows = [];
    let parentOpen = true;
    for (const r of all) {
      if (r.kind === 'feature') { parentOpen = hotkeys || open.has(r.id); rows.push({ ...r, expanded: parentOpen && !hotkeys }); }
      else if (r.child) { if (parentOpen) rows.push(r); }
      else rows.push(r);
    }
    const left = opts.x ?? Math.round((stage.size.width - T.width) / 2), top = opts.y ?? Math.round((stage.size.height - T.height) / 2);
    const g = L.group(stage, left, top);
    const w = T.width, h = T.height;
    const footerTop = h - T.footerHeight, rowsTop = T.headerHeight + T.theadHeight;
    const visible = Math.max(0, Math.floor((footerTop - rowsTop - 2) / T.rowHeight));
    const scroll = rows.length > visible, shift = scroll ? 4 : 0; // rowBarGutter
    const first = Math.max(0, Math.min(opts.first || 0, rows.length - visible));
    const keyX = T.keyX - shift, stateX = T.stateX - shift, stepperX = T.stepperX - shift, sliderX = T.sliderX - shift;
    const tableLeft = T.sidebarWidth, tableWidth = w - T.sidebarWidth, rowWidth = tableWidth - shift - 2;

    L.panel(g, 0, 0, w, h, { frame: true });
    // Header
    L.label(g, T.pad, 6, 80, 'Lamium');
    L.label(g, T.pad + L.textWidth('Lamium') + 4, 6, 60, opts.version || '0.8.0', { color: 'faint' });
    L.field(g, T.searchX, 4, T.searchWidth, 12, { placeholder: L.t('searchPlaceholder') + '  Ctrl+F', text: opts.query || '' });
    L.frame(g, T.closeX, 4, T.closeWidth, 12, 'keyEdge');
    L.label(g, T.closeX, 6, T.closeWidth, L.t('closeButton'), { color: 'dim', align: 'center' });
    L.rule(g, 0, T.headerHeight - 1, w);
    // Sidebar
    const navTop = T.headerHeight, navBottom = footerTop, items = D.navigation;
    L.fill(g, tableLeft - 1, navTop, 1, navBottom - navTop, 'white', .14);
    const pinnedFrom = items.length - T.pinnedItems;
    items.forEach((item, i) => {
      const pinned = i >= pinnedFrom;
      const y = pinned ? navBottom - 4 - (T.pinnedItems - (i - pinnedFrom)) * T.navItemHeight : navTop + 4 + i * T.navItemHeight;
      if (i === pinnedFrom) L.rule(g, 7, y - 4, T.sidebarWidth - 14);
      const active = item.key === nav;
      if (active) { L.fill(g, 1, y, T.sidebarWidth - 2, T.navItemHeight, 'accent', .16); L.fill(g, 1, y, 2, T.navItemHeight, 'accent'); }
      L.label(g, 8, y + 3, T.sidebarWidth - 14, L.t(item.key), { color: active ? 'text' : 'dim' });
    });
    // Column heads
    const theadY = T.headerHeight + 2;
    if (hotkeys) L.label(g, T.nameX, theadY, keyX - T.nameX - T.gap, L.t('column.action'), { color: 'faint' });
    else {
      L.label(g, T.nameX, theadY, stateX - T.nameX - T.gap, L.t('column.feature'), { color: 'faint' });
      L.label(g, stateX - 6, theadY, T.stateWidth + 12, L.t('column.state'), { color: 'faint', align: 'center' });
    }
    L.label(g, keyX, theadY, T.keyWidth, L.t('column.key'), { color: 'faint' });
    const resetX = (hotkeys ? keyX - T.gap - 64 : stateX - 6) - T.gap - T.headActionWidth;
    L.button(g, resetX, T.headerHeight, T.headActionWidth, 11, L.t(hotkeys ? 'reset.keys' : 'reset.section'), { kind: 'danger' });
    L.rule(g, tableLeft, rowsTop - 1, tableWidth);
    // Rows
    const rowY = {};
    let selectedRow = null;
    for (let i = first; i < Math.min(rows.length, first + visible); i++) {
      const r = rows[i], y = rowsTop + (i - first) * T.rowHeight, x0 = tableLeft + 1;
      if (r.id) rowY[r.id] = y + top;
      if (r.kind === 'group') {
        L.label(g, T.nameX, y + 4, tableWidth - 2 * T.pad, L.pick(r.name), { color: 'accent' });
        L.fill(g, x0, y + T.rowHeight - 1, rowWidth, 1, 'accent', .3);
        continue;
      }
      if (i % 2) L.fill(g, x0, y, rowWidth, T.rowHeight, 'white', .025);
      if (r.kind === 'feature' && r.expanded) L.fill(g, x0, y, rowWidth, T.rowHeight, 'white', .05);
      const selected = opts.selected && r.id === opts.selected;
      if (selected) selectedRow = r;
      L.rowBackground(g, x0, y, rowWidth, T.rowHeight, selected, false);
      if (r.changed) L.fill(g, x0 - 1, y, 2, T.rowHeight, 'info'); // what the proposal changed, for the review only
      const dimmed = opts.paused && r.kind === 'option';
      const row = dimmed ? L.group(g) : g;
      if (dimmed) row.style.opacity = .45;
      const keys = r.keys ? L.keyNames(r.keys) : null;
      const keyCell = () => {
        if (!r.keys) return;
        if (keys.length) L.keycaps(row, keyX, y + 2, T.keyWidth, keys, { tone: r.keyTone || 'plain' });
        else L.label(row, keyX, y + 3, T.keyWidth, L.t('unbound'), { color: 'faint' });
      };
      if (r.kind === 'feature') {
        const nameX = hotkeys ? T.nameX : T.nameX + 9;
        if (r.children && !hotkeys) L.chevron(row, T.nameX, y + 5, r.expanded);
        const right = r.open ? stateX - T.gap - 14 : stateX - T.gap;
        const name = L.pick(r.name);
        L.label(row, nameX, y + 3, Math.min(L.textWidth(name) + 2, right - nameX), name);
        let cursor = nameX + Math.min(right - nameX, L.textWidth(name)) + 5;
        if (r.children && !hotkeys) { L.label(row, cursor, y + 3, 20, String(r.children), { color: 'faint' }); cursor += L.textWidth(String(r.children)) + 5; }
        if (r.experimental && cursor + 8 < right) L.badge(row, cursor, y, L.t('experimental'));
        if (!hotkeys && r.control && r.control.type === 'switch') L.switch(row, stateX + (T.stateWidth - W.switchWidth) / 2, y + (T.rowHeight - W.switchHeight) / 2, r.control.on);
        keyCell();
        if (r.open && !hotkeys) L.button(row, stateX - (38 - T.stateWidth), y + 2, 38, W.capHeight, L.t('open'));
        continue;
      }
      L.guide(row, T.nameX + 3, y, r.last);
      const name = L.pick(r.name), nameColor = 'dim';
      const c = r.control;
      if (r.kind === 'option' && c && !hotkeys) {
        if (c.type === 'switch') {
          L.label(row, T.nameX + 12, y + 3, stateX - T.gap - T.nameX - 12, name, { color: nameColor });
          L.switch(row, stateX + (T.stateWidth - W.switchWidth) / 2, y + (T.rowHeight - W.switchHeight) / 2, c.on);
        } else if (c.type === 'slider') {
          L.label(row, T.nameX + 12, y + 3, sliderX - T.gap - T.nameX - 12, name, { color: nameColor });
          L.slider(row, sliderX, y + 2, T.sliderWidth, c.position);
          L.label(row, sliderX + T.sliderWidth + 4, y + 3, T.sliderValueWidth, L.pick(c.value), { align: 'right' });
        } else {
          L.label(row, T.nameX + 12, y + 3, stepperX - T.gap - T.nameX - 12, name, { color: nameColor });
          L.stepper(row, stepperX, y + 1, T.stepperWidth, T.rowHeight - 2, L.pick(c.value), { warn: c.warn });
        }
      } else if (r.kind === 'link') {
        const link = L.pick(r.link), right = keyX + T.keyWidth, lx = Math.max(stateX, Math.min(keyX, right - L.textWidth(link)));
        L.label(row, T.nameX + 12, y + 3, lx - T.gap - T.nameX - 12, name, { color: nameColor });
        L.label(row, lx, y + 3, right - lx, link, { color: 'accent' });
      } else {
        const end = r.open ? stateX - T.gap - 14 : (r.keys || r.kind === 'fixed' ? keyX : stateX) - T.gap;
        L.label(row, T.nameX + 12, y + 3, end - T.nameX - 12, name, { color: nameColor });
        if (r.open) L.button(row, stateX - (38 - T.stateWidth), y + 2, 38, W.capHeight, L.t('open'));
      }
      keyCell();
    }
    if (scroll) {
      const track = visible * T.rowHeight, thumb = Math.max(8, track * visible / rows.length);
      L.scrollbar(g, tableLeft + tableWidth - 3, rowsTop, track, rowsTop + (track - thumb) * first / Math.max(1, rows.length - visible), thumb);
    }
    // Footer: two description lines, then how to operate it.
    L.rule(g, 0, footerTop, w);
    const help = opts.footer ?? (selectedRow && selectedRow.help ? L.pick(selectedRow.help) : '');
    String(help).split('\n').slice(0, T.descriptionLines).forEach((line, i) =>
      L.label(g, T.pad, footerTop + 3 + i * T.descriptionLineHeight, w - 2 * T.pad - T.gap, line, { color: opts.footerWarn ? 'warning' : 'text' }));
    L.label(g, T.pad, footerTop + 30, w - 2 * T.pad, opts.hint ?? L.t('tableHint'), { color: 'faint' });
    return {
      group: g, left, top, first, rowY: (id) => rowY[id], rowsTop: top + rowsTop, keyX: left + keyX, stateX: left + stateX,
      // The row under a stage position, for a mockup that reacts to clicks; null outside the rows.
      rowAt(x, y) {
        if (x < left + tableLeft || x >= left + w) return null;
        const i = Math.floor((y - top - rowsTop) / T.rowHeight);
        return i >= 0 && i < visible ? rows[first + i] || null : null;
      },
    };
  };
  // A stage position (GUI units) from a pointer event on the page.
  L.pointer = (stage, event) => {
    const rect = stage.getBoundingClientRect(), scale = rect.width / stage.size.width;
    return { x: (event.clientX - rect.left) / scale, y: (event.clientY - rect.top) / scale };
  };

  // ---- The page around the stages ----
  L.page = {};
  // A row of choices for the reader (案, 言語, 状態). onChange(value) redraws.
  L.page.choice = (container, label, options, value, onChange) => {
    const g = document.createElement('div');
    g.className = 'lk-cgroup';
    g.innerHTML = `<span></span><div class="lk-seg" role="group"></div>`;
    g.firstChild.textContent = label;
    const seg = g.lastChild;
    seg.setAttribute('aria-label', label);
    for (const [v, text] of options) {
      const b = document.createElement('button');
      b.type = 'button';
      b.textContent = text;
      b.setAttribute('aria-pressed', String(v === value));
      b.addEventListener('click', () => {
        seg.querySelectorAll('button').forEach((x) => x.setAttribute('aria-pressed', 'false'));
        b.setAttribute('aria-pressed', 'true');
        onChange(v);
      });
      seg.append(b);
    }
    container.append(g);
    return g;
  };
  L.page.languages = (container, onChange) =>
    L.page.choice(container, 'Language', codes.map((c, i) => [c, ['English', '日本語', '简体中文', '繁體中文'][i]]), L.langCode(), (c) => { L.setLang(c); onChange(); });
  // compare(container, cases): today first, then each proposal, side by side; each case draws on its own stage.
  //   case: {kind: 'today' | 'proposal', title, note, width, height, world, draw(stage)}
  L.page.compare = (container, cases) => {
    container.replaceChildren();
    container.classList.add('lk-compare');
    if (!cases.length || cases[0].kind !== 'today') report('比較は「今」(kind: today) から始める');
    for (const c of cases) {
      const section = document.createElement('section');
      section.className = 'lk-case ' + (c.kind || 'proposal');
      const head = document.createElement('header');
      head.innerHTML = '<span class="lk-tag"></span><h2></h2>';
      head.firstChild.textContent = c.kind === 'today' ? '今' : '案';
      head.lastChild.textContent = c.title || '';
      section.append(head);
      const stage = L.stage(section, { width: c.width || 640, height: c.height || 380, world: c.world !== false });
      try { c.draw(stage); } catch (e) { report(`${c.title || c.kind}: ${e.message}`); }
      if (c.note) { const n = document.createElement('p'); n.className = 'lk-note'; n.textContent = c.note; section.append(n); }
      container.append(section);
    }
  };
  // Draw once the game font has loaded, so measured widths match what is drawn.
  L.ready = (fn) => (document.fonts ? document.fonts.ready : Promise.resolve()).then(() => {
    try { fn(); } catch (e) { report(e.message); }
  });
})();
