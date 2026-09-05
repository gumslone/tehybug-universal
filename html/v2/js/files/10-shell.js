/* TeHyBug web UI v2 — shell: layout, navigation, form helpers, save flow,
 * dialogs, and the restart / go-live choreography every page shares. */
(function () {
  'use strict';
  const T = window.TeHyBug;
  const html = T.html, raw = T.raw, $ = T.$, $$ = T.$$;

  /* ---------------- Form & layout helpers ---------------- */
  const UI = T.UI = {
    pagehead(title, sub) {
      return html`<div class="pagehead"><h1>${title}</h1>${sub ? html`<p class="sub">${sub}</p>` : ''}</div>`;
    },
    card(o) {
      return html`<section class="card ${o.cls || ''}" ${o.id ? raw('id="' + T.esc(o.id) + '"') : ''}>
        ${o.title ? html`<div class="card-head"><h2>${o.icon ? T.icon(o.icon) : ''}${o.title}</h2>${o.actions ? html`<div class="actions">${o.actions}</div>` : ''}</div>` : ''}
        ${o.body}</section>`;
    },
    field(o) {
      const type = o.type || 'text';
      const input = o.textarea
        ? html`<textarea id="${o.id}" rows="${o.rows || 3}" placeholder="${o.placeholder || ''}" ${raw(o.attrs || '')}>${o.value == null ? '' : o.value}</textarea>`
        : html`<input id="${o.id}" type="${type}" value="${o.value == null ? '' : o.value}" placeholder="${o.placeholder || ''}" ${raw(o.attrs || '')}>`;
      return html`<div class="field" data-field="${o.id}">
        <label for="${o.id}">${o.label}${o.labelHint ? html` <span class="hint">${o.labelHint}</span>` : ''}</label>
        ${o.button ? html`<div class="with-btn">${input}${o.button}</div>` : input}
        ${o.after || ''}
        ${o.hint ? html`<div class="hint">${o.hint}</div>` : ''}
      </div>`;
    },
    password(o) {
      return UI.field(Object.assign({}, o, {
        type: 'password',
        attrs: (o.attrs || '') + ' autocomplete="off"',
        button: html`<button type="button" class="icon-btn field-btn" data-toggle-password="${o.id}" aria-label="Show password">${T.icon('eye')}</button>`
      }));
    },
    toggle(o) {
      return html`<label class="switch ${o.cls || ''}"><input type="checkbox" id="${o.id}" ${o.checked ? 'checked' : ''} ${o.disabled ? 'disabled' : ''}><span class="sw"></span><span class="sw-text"><span class="sw-label">${o.label}</span>${o.hint ? html`<span class="hint">${o.hint}</span>` : ''}</span></label>`;
    },
    choice(o) {
      return html`${o.options.map(opt => html`<label class="choice ${opt.value === o.value ? 'selected' : ''} ${opt.disabled ? 'disabled' : ''}">
        <input type="radio" name="${o.name}" value="${opt.value}" ${opt.value === o.value ? 'checked' : ''} ${opt.disabled ? 'disabled' : ''}>
        <span class="choice-body"><span class="choice-label">${opt.label}</span>${opt.hint ? html`<span class="hint">${opt.hint}</span>` : ''}</span></label>`)}`;
    },
    select(o) {
      return html`<div class="field" data-field="${o.id}"><label for="${o.id}">${o.label}</label>
        <select id="${o.id}">${o.options.map(opt => html`<option value="${opt.value}" ${String(opt.value) === String(o.value) ? 'selected' : ''}>${opt.label}</option>`)}</select>
        ${o.hint ? html`<div class="hint">${o.hint}</div>` : ''}</div>`;
    },
    note(kind, body) {
      const icon = { info: 'info', warn: 'alert-triangle', danger: 'alert-triangle', ok: 'check' }[kind] || 'info';
      return html`<div class="note note-${kind}">${T.icon(icon)}<div>${body}</div></div>`;
    },
    btn(o) {
      const tag = o.href ? 'a' : 'button';
      return raw('<' + tag + (o.href ? ' href="' + T.esc(o.href) + '"' + (o.external ? ' target="_blank" rel="noopener"' : '') : ' type="button"') +
        ' class="btn ' + T.esc(o.cls || '') + '"' + (o.id ? ' id="' + T.esc(o.id) + '"' : '') + (o.disabled ? ' disabled' : '') + ' ' + (o.attrs || '') + '>' +
        (o.icon ? T.icon(o.icon).__html : '') + T.esc(o.label) + '</' + tag + '>');
    },
    // "Fill from my sensors" link under a template field
    fill(target, kind, label) {
      return html`<div class="hint">${label || 'Fill from my sensors:'} <a href="#" data-fill="${kind}" data-target="${target}">${kind === 'query' ? 'build the query string' : kind === 'json' ? 'build the JSON' : 'list my readings'}</a> <span class="units-label">(${T.units() === 'imperial' ? '°F' : '°C'})</span></div>`;
    },
    // placeholder chips that insert %key% into a field
    chips(target, keys) {
      keys = keys || T.Readings.known();
      if (!keys.length) return html`<div class="hint">Waiting for readings from the device…</div>`;
      return html`<div class="chips">${keys.map(k => html`<button type="button" class="chip" data-insert="%${k}%" data-target="${target}" title="${T.Readings.name(k)}"><code>%${k}%</code></button>`)}</div>`;
    },
    kv(rows) {
      return html`<dl class="kv">${rows.filter(r => r[1] != null && r[1] !== '').map(r => html`<dt>${r[0]}</dt><dd>${r[1]}</dd>`)}</dl>`;
    },
    table(head, rows, cls) {
      return html`<div class="table-wrap"><table class="table ${cls || ''}">${head ? html`<thead><tr>${head.map(h => html`<th>${h}</th>`)}</tr></thead>` : ''}<tbody>${rows.map(r => html`<tr>${r.map(c => html`<td>${c}</td>`)}</tr>`)}</tbody></table></div>`;
    },
    disclosure(title, body, open, cls) {
      return html`<details class="disclosure ${cls || ''}" ${open ? 'open' : ''}><summary>${T.icon('chevron-right')}${title}</summary><div class="disclosure-body">${body}</div></details>`;
    },
    // Clock-from-the-network settings, shared by the Data log and Display
    // pages. The time zone is stored as the POSIX string the firmware needs;
    // the list maps the browser's zone name to it, anything else is typed in.
    clockFields(c) {
      const zones = T.TIMEZONES;
      const stored = c.timezone == null ? '' : String(c.timezone);
      let browserZone = '';
      try { browserZone = Intl.DateTimeFormat().resolvedOptions().timeZone || ''; } catch (e) { /* no Intl */ }
      const guessed = zones.find(z => z[0] === browserZone);
      const known = zones.find(z => z[1] === stored);
      const selected = stored ? (known ? known[1] : '__custom') : (guessed ? guessed[1] : 'UTC0');
      return html`
        ${UI.toggle({ id: 'ntpActive', label: 'Set the clock from the internet', checked: c.ntpActive !== false, hint: 'At start-up, whenever WiFi is up (a sleeping battery board only does it while its clock is unset, so wakes stay short). The clock chip keeps the time in between.' })}
        <div class="fields-inline mt">
          ${UI.select({ id: 'timezoneSelect', label: 'Time zone', options: zones.map(z => ({ value: z[1], label: z[0] })).concat([{ value: '__custom', label: 'Custom (POSIX TZ string)' }]), value: selected, hint: !stored && guessed ? 'From your browser — stored when you save this page.' : '' })}
          ${UI.field({ id: 'ntpServer', label: 'NTP server', value: c.ntpServer || 'pool.ntp.org', placeholder: 'pool.ntp.org', attrs: 'autocomplete="off" spellcheck="false"' })}
        </div>
        <div id="timezone-custom" ${selected === '__custom' ? '' : 'hidden'}>${UI.field({ id: 'timezone', label: 'POSIX TZ string', value: stored, placeholder: 'CET-1CEST,M3.5.0,M10.5.0/3', attrs: 'autocomplete="off" spellcheck="false"', hint: html`What the ESP8266 understands, e.g. <code>CET-1CEST,M3.5.0,M10.5.0/3</code> for Central Europe or <code>EST5EDT,M3.2.0,M11.1.0</code> for New York.` })}</div>`;
    },
    // the clock settings as config keys; throws when the custom zone is empty
    clockValues() {
      const sel = T.val('timezoneSelect');
      const tz = sel === '__custom' ? T.val('timezone').trim() : sel;
      if (sel === '__custom' && !tz) throw T.fail('Enter a POSIX TZ string, or pick a zone from the list', 'timezone');
      return { ntpActive: T.checked('ntpActive'), ntpServer: T.val('ntpServer').trim() || 'pool.ntp.org', timezone: tz === 'UTC0' ? '' : tz };
    },
    unitsSeg() {
      const u = T.units();
      return html`<div class="seg" data-units data-nosave><button type="button" data-value="metric" class="${u === 'metric' ? 'on' : ''}">°C</button><button type="button" data-value="imperial" class="${u === 'imperial' ? 'on' : ''}">°F</button></div>`;
    },
    // the reading → placeholder → unit reference used by several pages
    placeholderList() {
      const units = T.units();
      const keys = T.Readings.known();
      if (!keys.length) return html`<div class="empty">No readings received from the device yet. They appear a moment after it connects.</div>`;
      return html`<div class="ph-list">${keys.map(k => { const kk = T.Readings.keyFor(k, units); return html`<div><code>%${kk}%</code><span>${T.Readings.name(k)}</span><span class="u">${T.Readings.unit(kk)}</span></div>`; })}</div>`;
    }
  };

  T.val = id => { const el = document.getElementById(id); return el ? el.value : ''; };
  T.checked = id => { const el = document.getElementById(id); return !!(el && el.checked); };
  T.num = (id, def) => { const v = parseFloat(T.val(id)); return isFinite(v) ? v : def; };
  T.int = (id, def) => { const v = parseInt(T.val(id), 10); return isFinite(v) ? v : def; };
  T.radio = name => { const el = document.querySelector('input[name="' + name + '"]:checked'); return el ? el.value : ''; };
  T.setChecked = (id, on) => { const el = document.getElementById(id); if (el) el.checked = !!on; };
  T.setRadio = (name, value) => { const el = document.querySelector('input[name="' + name + '"][value="' + value + '"]'); if (el) el.checked = true; syncChoices(document); };
  T.show = (id, on) => { const el = document.getElementById(id); if (el) el.hidden = !on; };
  // a validation failure: message for the toast, field to focus
  T.fail = (message, fieldId) => { const e = new Error(message); e.fieldId = fieldId; return e; };
  T.isUrl = u => /^https?:\/\/\S+$/i.test(String(u || '').trim());

  function syncChoices(root) {
    $$('.choice input[type="radio"]', root).forEach(r => { const c = r.closest('.choice'); if (c) c.classList.toggle('selected', r.checked); });
    $$('.weekdays input', root).forEach(i => { const l = i.closest('label'); if (l) l.classList.toggle('on', i.checked); });
  }

  /* ---------------- Shell ---------------- */
  const Shell = T.Shell = { current: null, dirty: false, subs: [] };
  const GROUPS = [{ id: 'top', title: '' }, { id: 'setup', title: 'Set up' }, { id: 'more', title: 'More' }];

  function layout() {
    return html`<div class="app">
      ${window.TEHYBUG_DEMO ? html`<div class="demo-banner">Demo — a simulated TeHyBug; nothing here reaches a real device</div>` : ''}
      <header class="topbar">
        <button type="button" class="icon-btn menu-btn" id="menu-btn" aria-label="Menu">${T.icon('menu')}</button>
        <div class="brand">TeHyBug<span class="brand-sub" id="brand-sub"></span></div>
        <div class="spacer"></div>
        <span class="pill neutral" id="conn-pill">${T.icon('wifi')}<span>connecting…</span></span>
      </header>
      <nav class="nav" id="nav" aria-label="Pages"></nav>
      <div class="scrim" id="scrim"></div>
      <main class="main" id="main"></main>
      <div class="savebar" id="savebar" hidden>
        <span class="save-status" id="save-status"></span>
        <button type="button" class="btn btn-primary" id="save-btn">${T.icon('save')}<span id="save-label">Save</span></button>
      </div>
      <div class="toasts" id="toasts" role="status" aria-live="polite"></div>
      <div id="dialogs"></div>
    </div>`;
  }

  Shell.buildNav = () => {
    const pages = T.Pages.visible().filter(p => p.nav);
    const cur = Shell.current ? Shell.current.id : '';
    const groups = GROUPS.map(g => {
      const list = pages.filter(p => p.nav.group === g.id).sort((a, b) => (a.nav.order || 0) - (b.nav.order || 0));
      if (!list.length) return '';
      return html`<div class="nav-group">${g.title ? html`<div class="nav-title">${g.title}</div>` : ''}
        ${list.map(p => html`<a href="#/${p.id}" data-page="${p.id}" class="${cur === p.id ? 'active' : ''}">${T.icon(p.nav.icon)}${p.title}</a>`)}</div>`;
    });
    T.render($('#nav'), html`<div class="nav-head"><div class="brand">TeHyBug</div><div class="spacer"></div><button type="button" class="icon-btn" id="nav-close" aria-label="Close menu">${T.icon('x')}</button></div>
      ${groups}
      <div class="nav-group"><div class="nav-title">Links</div>
        <a href="https://tehybug.com" target="_blank" rel="noopener">${T.icon('globe')}tehybug.com${T.icon('external-link', 'ext')}</a>
        <a href="https://www.tindie.com/stores/gumslone/" target="_blank" rel="noopener">${T.icon('shopping-cart')}Buy on Tindie${T.icon('external-link', 'ext')}</a>
        <a href="${T.REPO}" target="_blank" rel="noopener">${T.icon('github')}GitHub${T.icon('external-link', 'ext')}</a>
      </div>
      <div class="nav-foot hint">Web UI ${T.UI_VERSION}</div>`);
  };
  function setActiveNav(id) { $$('#nav a[data-page]').forEach(a => a.classList.toggle('active', a.getAttribute('data-page') === id)); }
  // Scroll lock for sheets and the drawer. iOS Safari ignores overflow:hidden
  // on the document, so the body is pinned at its current offset instead.
  let lockCount = 0, lockedY = 0, lockedPage = '';
  function lockScroll() {
    if (lockCount++ > 0) return;
    lockedY = window.scrollY || 0;
    lockedPage = Shell.current ? Shell.current.id : '';
    document.body.classList.add('locked');
    document.body.style.top = -lockedY + 'px';
  }
  function unlockScroll() {
    if (lockCount === 0) return;
    if (--lockCount > 0) return;
    document.body.classList.remove('locked');
    document.body.style.top = '';
    // back to where the page was, unless a link in the sheet went elsewhere
    window.scrollTo(0, (Shell.current && Shell.current.id === lockedPage) ? lockedY : 0);
  }
  // the sheets currently open, so a page change can close the ones that may be closed
  const openDialogs = new Set();
  Shell.closeDialogs = () => { Array.from(openDialogs).forEach(d => { if (d.dismissable) d.dismiss(); }); };
  function openDrawer() {
    const nav = $('#nav');
    if (nav.classList.contains('open')) return;
    nav.classList.add('open');
    $('#scrim').classList.add('open');
    lockScroll();
    const first = $('#nav a[data-page]');
    if (first) first.focus();
  }
  function closeDrawer() {
    const nav = $('#nav');
    if (!nav.classList.contains('open')) return;
    nav.classList.remove('open');
    $('#scrim').classList.remove('open');
    unlockScroll();
  }

  function updatePill() {
    const pill = $('#conn-pill');
    if (!pill) return;
    if (Shell.deviceGone) { pill.className = 'pill neutral'; T.render(pill, html`${T.icon('radio')}<span>${Shell.deviceGone}</span>`); return; }
    const on = T.State.online;
    pill.className = 'pill ' + (on ? 'on' : 'off');
    T.render(pill, html`${T.icon(on ? 'wifi' : 'wifi-off')}<span>${on ? 'Connected' : 'Reconnecting…'}</span>`);
  }
  function updateBrand() {
    const el = $('#brand-sub');
    if (el) el.textContent = { universal: 'universal', display: 'Display', generic: 'first-gen' }[T.board()] || '';
  }

  function saveCfg(page) {
    if (!page) return null;
    return typeof page.save === 'function' ? page.save() : (page.save || null);
  }
  function updateSaveBar() {
    const bar = $('#savebar');
    const cfg = saveCfg(Shell.current);
    bar.hidden = !cfg;
    if (cfg) $('#save-label').textContent = cfg.label || 'Save';
    // Until the configuration has loaded, a page shows firmware defaults;
    // saving then would write those defaults over the device's real settings.
    $('#save-btn').disabled = Shell.saving || (!!cfg && !T.State.configLoaded);
    document.documentElement.style.setProperty('--savebar-h', cfg ? bar.offsetHeight + 'px' : '0px');
    Shell.setDirty(Shell.dirty);
  }
  Shell.setDirty = on => {
    Shell.dirty = !!on;
    const st = $('#save-status');
    if (!st) return;
    const cfg = saveCfg(Shell.current);
    if (cfg && !T.State.configLoaded) {
      st.textContent = 'The settings have not loaded from the device yet — retrying; saving is off until they do.';
      st.className = 'save-status blocked';
      return;
    }
    st.textContent = on ? 'Unsaved changes' : (cfg && cfg.reboot ? 'Saving restarts the device' : '');
    st.className = 'save-status' + (on ? ' dirty' : '');
  };

  function unwire() {
    Shell.subs.forEach(off => off());
    Shell.subs = [];
  }
  Shell.show = (id, opts) => {
    opts = opts || {};
    let page = T.Pages.get(id);
    if (!page || (page.boards && T.State.infoLoaded && page.boards.indexOf(T.board()) < 0)) page = T.Pages.get('dashboard');
    if (Shell.current && Shell.current.unmount) { try { Shell.current.unmount(); } catch (e) { console.error(e); } }
    unwire();
    Shell.closeDialogs();
    Shell.current = page;
    Shell.dirty = false;
    Shell.drawnWithoutConfig = !T.State.configLoaded;
    // Each page gets a fresh element to mount on: listeners a page adds in
    // mount() die with it. On the shared #main they piled up across visits,
    // so a page's handler ran once per visit made so far.
    const main = $('#main');
    const root = document.createElement('div');
    root.className = 'page page-' + page.id;
    T.render(root, page.render());
    main.innerHTML = '';
    main.appendChild(root);
    syncChoices(root);
    Object.keys(page.on || {}).forEach(evt => Shell.subs.push(T.Bus.on(evt, page.on[evt].bind(page))));
    if (page.mount) page.mount(root);
    setActiveNav(page.id);
    closeDrawer();
    updateSaveBar();
    document.title = page.title + ' · TeHyBug';
    if (!opts.keepScroll) window.scrollTo(0, 0);
    if (location.hash !== '#/' + page.id) { history.replaceState(null, '', '#/' + page.id); }
  };
  // re-render the current page from the state — never over someone's edits
  Shell.rerender = () => { if (Shell.current && !Shell.dirty) Shell.show(Shell.current.id, { keepScroll: true }); };

  async function onHashChange() {
    const id = (location.hash || '').replace(/^#\/?/, '') || 'dashboard';
    if (Shell.current && Shell.current.id === id) return;
    if (Shell.dirty) {
      const ok = await Shell.confirm({ title: 'Leave without saving?', body: 'The changes on this page have not been saved yet.', okLabel: 'Discard changes', danger: true });
      if (!ok) { history.replaceState(null, '', '#/' + Shell.current.id); return; }
    }
    Shell.show(id);
  }

  /* ---------------- Toasts & dialogs ---------------- */
  Shell.toast = (msg, kind, ms) => {
    const box = $('#toasts');
    if (!box) return;
    const el = document.createElement('div');
    el.className = 'toast ' + (kind || '');
    if (kind === 'danger') el.setAttribute('role', 'alert');
    T.render(el, html`${T.icon(kind === 'danger' || kind === 'warn' ? 'alert-triangle' : 'check')}<span>${msg}</span>`);
    box.appendChild(el);
    setTimeout(() => el.remove(), ms || 3200);
  };

  const FOCUSABLE = 'a[href], button:not([disabled]), input:not([disabled]):not([type="hidden"]), select:not([disabled]), textarea:not([disabled]), [tabindex]:not([tabindex="-1"])';
  let dialogSeq = 0;
  Shell.dialog = o => {
    const host = $('#dialogs');
    const wrap = document.createElement('div');
    wrap.className = 'overlay';
    let buttons = o.buttons || [];
    const titleId = o.title ? 'dlg-title-' + (++dialogSeq) : '';
    const prevFocus = document.activeElement;
    let closed = false;
    const entry = { dismissable: o.dismissable !== false };
    const api = {
      el: wrap,
      close() {
        if (closed) return;
        closed = true;
        openDialogs.delete(entry);
        wrap.remove();
        unlockScroll();
        if (prevFocus && typeof prevFocus.focus === 'function' && document.contains(prevFocus)) prevFocus.focus();
      },
      body() { return $('.dialog-body', wrap); },
      setBody(tpl) { T.render($('.dialog-body', wrap), tpl); syncChoices(wrap); },
      setButtons(list) { buttons = list; T.render($('.dialog-buttons', wrap), renderButtons()); $('.dialog-buttons', wrap).hidden = !list.length; }
    };
    function renderButtons() {
      return html`${buttons.map((b, i) => html`<button type="button" class="btn ${b.cls || ''}" data-btn="${i}" ${b.disabled ? 'disabled' : ''}>${b.icon ? T.icon(b.icon) : ''}${b.label}</button>`)}`;
    }
    T.render(wrap, html`<div class="dialog ${o.cls || ''}" role="dialog" aria-modal="true" tabindex="-1" ${titleId ? raw('aria-labelledby="' + titleId + '"') : ''}>
      ${o.title ? html`<h2 id="${titleId}">${o.title}</h2>` : ''}
      <div class="dialog-body">${o.body}</div>
      <div class="dialog-buttons" ${buttons.length ? '' : 'hidden'}>${renderButtons()}</div></div>`);
    syncChoices(wrap);
    const dismiss = () => { api.close(); if (o.onDismiss) o.onDismiss(); };
    entry.dismiss = dismiss; // so a page change resolves a pending confirm as "no"
    wrap.addEventListener('click', e => {
      const b = e.target.closest('[data-btn]');
      if (b) { const def = buttons[+b.getAttribute('data-btn')]; if (def && def.onClick) def.onClick(api, b); else api.close(); return; }
      if (o.dismissable !== false && e.target === wrap) dismiss();
    });
    wrap.addEventListener('change', () => syncChoices(wrap));
    // Keyboard: Escape closes a dismissable sheet; Tab cycles inside it so
    // focus never lands on the page underneath (aria-modal hides that page
    // from screen readers, so a keyboard user would otherwise be stranded).
    wrap.addEventListener('keydown', e => {
      if (e.key === 'Escape') { if (o.dismissable !== false) { e.preventDefault(); dismiss(); } return; }
      if (e.key !== 'Tab') return;
      const items = $$(FOCUSABLE, wrap).filter(el => el.offsetParent !== null);
      if (!items.length) { e.preventDefault(); return; }
      const first = items[0], last = items[items.length - 1];
      const sheet = $('.dialog', wrap);
      if (e.shiftKey && (document.activeElement === first || document.activeElement === sheet)) { e.preventDefault(); last.focus(); }
      else if (!e.shiftKey && document.activeElement === last) { e.preventDefault(); first.focus(); }
    });
    entry.api = api;
    openDialogs.add(entry);
    host.appendChild(wrap);
    lockScroll();
    // focus the sheet itself, so its title and body are read before the buttons
    $('.dialog', wrap).focus();
    return api;
  };
  Shell.confirm = o => new Promise(resolve => {
    Shell.dialog({
      title: o.title, body: o.body, onDismiss: () => resolve(false),
      buttons: [
        { label: o.cancelLabel || 'Cancel', onClick: a => { a.close(); resolve(false); } },
        { label: o.okLabel || 'OK', cls: o.danger ? 'btn-danger' : 'btn-primary', onClick: a => { a.close(); resolve(true); } }
      ]
    });
  });

  /* ---------------- Save flow ---------------- */
  Shell.save = async () => {
    const page = Shell.current;
    const cfg = saveCfg(page);
    if (!cfg || !page.collect) return;
    if (!T.State.configLoaded) { Shell.toast('The settings have not loaded from the device yet', 'warn'); return; }
    $$('.field', $('#main')).forEach(clearFieldError);
    let data;
    try {
      data = page.collect();
    } catch (e) {
      Shell.toast(e.message || 'Please check the highlighted field', 'danger', 5000);
      if (e.fieldId) {
        const el = document.getElementById(e.fieldId);
        if (el) {
          // the toast alone is silent to a screen reader (focus moves first),
          // so the message also lands in the field itself
          const f = el.closest('.field');
          if (f) {
            f.classList.add('invalid');
            const err = document.createElement('div');
            err.className = 'hint field-error';
            err.id = e.fieldId + '-err';
            err.textContent = e.message || 'Please check this field';
            f.appendChild(err);
            el.setAttribute('aria-invalid', 'true');
            el.setAttribute('aria-describedby', err.id);
          }
          el.focus();
        }
      }
      return;
    }
    if (cfg.confirm) { const ok = await cfg.confirm(data); if (!ok) return; }
    const btn = $('#save-btn');
    Shell.saving = true;
    btn.disabled = true;
    try {
      const payload = Object.assign({}, data, cfg.reboot ? { reboot: true } : {});
      await T.Api.saveConfig(payload);
      T.applyConfig(data);
      Shell.setDirty(false);
      T.Bus.emit('saved', { page: page.id, data });
      // Leaving setup mode on a battery board, or switching WiFi off with
      // offline mode on any board, takes this interface away with the
      // restart — say so instead of waiting for a device that will not answer.
      if (data.configModeActive === false && (data.offlineModeActive === true || !T.isDisplay())) { T.LeftSetup.show(data.offlineModeActive ? 'offline' : 'live'); return; }
      if (cfg.reboot) {
        const back = await T.Restart.wait();
        if (!back) return; // the user gave up waiting: nothing more to offer
        if (cfg.afterRestart) cfg.afterRestart(data);
      } else {
        Shell.toast('Saved');
        Shell.rerender();
      }
      if (page.offersGoLive && T.State.configLoaded && T.State.config.configModeActive !== false && T.destinations().length) T.GoLive.suggest();
    } catch (e) {
      Shell.toast('Not saved: ' + (e.message || e), 'danger', 6000);
    } finally {
      Shell.saving = false;
      updateSaveBar();
    }
  };

  /* ---------------- Restart choreography ---------------- */
  T.Restart = {
    // Shows a blocking sheet while the device reboots and polls /api/info
    // until it answers again. Resolves true when it is back, false when the
    // user gives up.
    async wait(o) {
      o = o || {};
      const text = o.text || 'The device saved the settings and is restarting. This takes about 10–15 seconds.';
      const waiting = () => html`<div class="restart"><div class="spinner"></div>
          <p role="status" aria-live="polite">${text}</p>
          <p class="hint" id="restart-sub" aria-hidden="true">Waiting for it to come back…</p></div>`;
      const dlg = Shell.dialog({ title: o.title || 'Restarting…', dismissable: false, body: waiting() });
      const started = Date.now();
      await T.sleep(o.initialMs || 4000);
      for (;;) {
        const deadline = Date.now() + (o.timeoutMs || 75000);
        let back = false;
        while (Date.now() < deadline) {
          try { T.applyInfo(await T.Api.info()); back = true; break; } catch (e) { /* still down */ }
          const sub = $('#restart-sub', dlg.el);
          if (sub) sub.textContent = 'Waiting for it to come back… ' + Math.round((Date.now() - started) / 1000) + ' s';
          await T.sleep(1500);
        }
        if (back) break;
        // not back within the window: let the user decide, then keep polling
        // in this same sheet rather than stacking a second one
        const again = await new Promise(res => {
          dlg.setBody(html`<div class="restart">${T.icon('alert-triangle')}
            <p role="status" aria-live="polite"><strong>The device has not answered yet.</strong></p>
            <p class="hint">If its address changed, open it at the new address. On a battery board that just left setup mode this is expected: the interface only runs in setup mode.</p></div>`);
          dlg.setButtons([
            { label: 'Keep waiting', onClick: a => { a.setButtons([]); a.setBody(waiting()); res(true); } },
            { label: 'Reload page', cls: 'btn-primary', onClick: () => location.reload() },
            { label: 'Close', onClick: () => res(false) }
          ]);
        });
        if (!again) { dlg.close(); return false; }
      }
      try { T.applyConfig(await T.Api.config()); } catch (e) { /* the page keeps what it has */ }
      dlg.close();
      Shell.toast('Device is back online');
      Shell.rerender();
      return true;
    }
  };

  /* ---------------- Go live ---------------- */
  T.GoLive = {
    open() {
      const c = T.State.config;
      const dest = T.destinations();
      const display = T.isDisplay();
      // Offline mode outranks every network destination in the firmware: with
      // it stored, "going live" means running with WiFi off.
      const offline = !!c.offlineModeActive;
      const power = c.sleepModeActive ? 'deep' : (c.lightSleepModeActive ? 'light' : 'on');
      const iaq = !!T.State.seen.iaq;
      const body = html`
        <p>${offline
          ? html`Offline mode is switched on, so leaving setup mode runs the device <strong>with WiFi off</strong>: it logs to its memory module and nothing is sent. Switch offline mode off on <a href="#/datalog">Data log</a> first if you want it to send.`
          : html`Going live ends setup mode and starts sending. ${display ? '' : 'The device restarts and this interface stops being served.'}`}</p>
        ${dest.length
          ? html`<p class="mb0"><strong>Readings go to</strong></p><ul>${dest.map(d => html`<li>${d.label} <span class="hint">${d.detail}</span></li>`)}</ul>`
          : UI.note('warn', html`Nothing is switched on yet, so the device would go live and send nowhere — and with nothing to serve it just returns to setup mode. Set a destination on <a href="#/senddata">Send data</a> first.`)}
        ${display || offline ? '' : html`<h3 class="mt">Power</h3>${UI.choice({ name: 'golive-power', value: power, options: [
          { value: 'deep', label: 'Deep sleep — battery', hint: 'Powers down between sends (≈20 µA). Months on a battery; unreachable while asleep.' },
          { value: 'light', label: 'Light sleep', hint: 'Sleeps between sends but keeps WiFi associated, so it wakes fast. A middle ground.' },
          { value: 'on', label: 'Always on — USB / mains', hint: 'WiFi stays connected (≈80 mA). Needed for BME680 air-quality values.' }
        ] })}${iaq ? UI.note('info', 'A BME680 is attached: its air-quality values (IAQ, eCO₂, bVOC) need the sensor powered continuously — choose Always on to keep them.') : ''}`}
        ${display
          ? UI.note('info', 'On the Display Weatherstation the screen, clock, alarms and this interface keep running. Only the TEHYBUG setup access point goes away — open this page at the device address afterwards.')
          : UI.note('warn', html`To get back into setup later: press <strong>RESET</strong>, then <strong>MODE</strong> within a second — the LED turns ${T.led('blue')}. If it stays dark, press RESET twice about a second apart, then MODE.`)}`;
      const dlg = Shell.dialog({
        title: 'Go live', body,
        buttons: [
          { label: 'Cancel' },
          { label: offline ? 'Go offline' : 'Go live', cls: 'btn-primary', icon: 'radio', disabled: !dest.length, onClick: async (a, btn) => {
            btn.disabled = true;
            const data = { configModeActive: false };
            if (!display && !offline) { const p = T.radio('golive-power'); data.sleepModeActive = p === 'deep'; data.lightSleepModeActive = p === 'light'; }
            try {
              await T.Api.saveConfig(Object.assign({ reboot: true }, data));
              T.applyConfig(data);
              a.close();
              if (offline) T.LeftSetup.show('offline');
              else if (display) { await T.Restart.wait({ title: 'Going live…', text: 'The device is restarting into live mode. The screen keeps running; this page comes back in about 15 seconds.' }); }
              else T.LeftSetup.show('live');
            } catch (e) { btn.disabled = false; Shell.toast('Could not go live: ' + e.message, 'danger', 6000); }
          } }
        ]
      });
      return dlg;
    },
    suggest() {
      Shell.dialog({
        title: 'Saved — go live now?',
        body: html`<p>The settings are stored. The device only starts sending once it leaves setup mode.</p>
          <ul>${T.destinations().map(d => html`<li>${d.label} <span class="hint">${d.detail}</span></li>`)}</ul>`,
        buttons: [{ label: 'Not yet' }, { label: 'Go live', cls: 'btn-primary', icon: 'radio', onClick: a => { a.close(); T.GoLive.open(); } }]
      });
    }
  };

  // The final screen on a battery board once it left setup mode: no device
  // to reconnect to, so stop trying and explain what happens next.
  T.LeftSetup = {
    show(kind) {
      T.Live.stop();
      Shell.deviceGone = kind === 'offline' ? 'Device is offline-logging' : 'Device is live';
      updatePill();
      const cloud = T.destinations().some(d => d.id === 'cloud');
      Shell.dialog({
        title: kind === 'offline' ? 'Offline mode is on' : 'Your TeHyBug is live',
        dismissable: true,
        body: html`
          <p>${kind === 'offline'
            ? (T.isDisplay()
              ? 'The device restarted with WiFi off. The screen, clock and alarms keep running and it writes to its data log on the log interval; nothing is sent.'
              : 'The device restarted with WiFi off. It now wakes on the log interval, measures, writes a line to its data log and sleeps again.')
            : 'The device restarted and sends its first readings right away, then on its own schedule. This page is no longer served — that is normal.'}</p>
          ${cloud ? html`<p>Add the device to your account on <a href="https://tehybug.com" target="_blank" rel="noopener">tehybug.com</a> with the key from the dashboard; the first reading shows up within a minute or so.</p>` : ''}
          ${UI.note('info', html`<strong>To change anything later:</strong> press <strong>RESET</strong>, then <strong>MODE</strong> within a second, until the LED turns ${T.led('blue')}. If the LED stays dark, press RESET twice about a second apart, then MODE.${kind === 'offline' && T.isDisplay() ? ' Holding the right button for 10 seconds switches WiFi back on as well.' : ''} Then open this address again.`)}`,
        buttons: [{ label: 'Close' }]
      });
    }
  };

  /* ---------------- Global interactions ---------------- */
  function clearFieldError(f) {
    f.classList.remove('invalid');
    const err = $('.field-error', f);
    if (err) err.remove();
    $$('[aria-invalid]', f).forEach(inp => { inp.removeAttribute('aria-invalid'); inp.removeAttribute('aria-describedby'); });
  }
  function markDirty(e) {
    if (e.target.closest('[data-nosave]') || e.target.closest('.overlay')) return;
    const f = e.target.closest('.field');
    if (f) clearFieldError(f);
    Shell.setDirty(true);
  }
  function doFill(targetId, kind) {
    if (!T.Suggest.have()) { Shell.toast('No sensor readings received yet — wait a moment and try again', 'warn', 4000); return; }
    const el = document.getElementById(targetId);
    if (!el) return;
    const units = T.units();
    if (kind === 'query') {
      const cur = el.value || '';
      const base = cur.indexOf('://') > 0 ? cur.split('?')[0] : T.CLOUD_URL;
      el.value = base + '?' + T.Suggest.query(units);
    } else if (kind === 'json') el.value = T.Suggest.json(units);
    else if (kind === 'log') el.value = T.Suggest.logTemplate(units);
    el.dispatchEvent(new Event('input', { bubbles: true }));
  }
  function insertAt(targetId, text) {
    const el = document.getElementById(targetId);
    if (!el) return;
    const s = el.selectionStart == null ? el.value.length : el.selectionStart;
    const e = el.selectionEnd == null ? s : el.selectionEnd;
    el.value = el.value.slice(0, s) + text + el.value.slice(e);
    el.focus();
    try { el.setSelectionRange(s + text.length, s + text.length); } catch (err) { /* number inputs */ }
    el.dispatchEvent(new Event('input', { bubbles: true }));
  }
  T.setUnits = u => {
    T.Store.set('units', u);
    $$('.seg[data-units] button').forEach(b => b.classList.toggle('on', b.getAttribute('data-value') === u));
    $$('.units-label').forEach(s => { s.textContent = '(' + (u === 'imperial' ? '°F' : '°C') + ')'; });
    T.Bus.emit('units', u);
  };

  function bindEvents() {
    document.addEventListener('click', e => {
      const t = e.target;
      if (t.closest('#menu-btn')) { openDrawer(); return; }
      if (t.closest('#nav-close') || t === $('#scrim')) { closeDrawer(); return; }
      if (t.closest('#nav a')) { closeDrawer(); return; }
      if (t.closest('#save-btn')) { Shell.save(); return; }
      if (t.closest('[data-golive]')) { e.preventDefault(); T.GoLive.open(); return; }
      const copy = t.closest('[data-copy]');
      if (copy) { e.preventDefault(); T.copy(copy.getAttribute('data-copy')).then(ok => Shell.toast(ok ? 'Copied' : 'Could not copy — select it and copy by hand', ok ? '' : 'warn')); return; }
      const fill = t.closest('[data-fill]');
      if (fill) { e.preventDefault(); doFill(fill.getAttribute('data-target'), fill.getAttribute('data-fill')); return; }
      const ins = t.closest('[data-insert]');
      if (ins) { e.preventDefault(); insertAt(ins.getAttribute('data-target'), ins.getAttribute('data-insert')); return; }
      const pw = t.closest('[data-toggle-password]');
      if (pw) { const inp = document.getElementById(pw.getAttribute('data-toggle-password')); if (inp) inp.type = inp.type === 'password' ? 'text' : 'password'; return; }
      const seg = t.closest('.seg[data-units] button');
      if (seg) { T.setUnits(seg.getAttribute('data-value')); return; }
    });
    const main = $('#main');
    main.addEventListener('input', markDirty);
    main.addEventListener('change', e => {
      markDirty(e);
      syncChoices(main);
      if (e.target.id === 'timezoneSelect') T.show('timezone-custom', e.target.value === '__custom');
    });
    window.addEventListener('hashchange', onHashChange);
    window.addEventListener('beforeunload', e => { if (Shell.dirty) { e.preventDefault(); e.returnValue = ''; } });
    window.addEventListener('resize', () => updateSaveBar());
    // the drawer becomes the permanent sidebar at the breakpoint (app.css
    // ≥900px); an open one must not keep the page locked past it
    const desktop = matchMedia('(min-width: 900px)');
    const onBreakpoint = () => { if (desktop.matches) closeDrawer(); };
    if (desktop.addEventListener) desktop.addEventListener('change', onBreakpoint); else desktop.addListener(onBreakpoint);
    window.addEventListener('resize', onBreakpoint);
  }

  // The device page pulls the stylesheet in non-blocking (media="print" until
  // loaded); don't paint the app unstyled while it is still on its way.
  async function stylesheetReady() {
    const link = $('link[href*="style.php"]');
    // only the device page uses the print-until-loaded trick; a plain
    // stylesheet link (demo.html) has already applied or has a sheet
    if (!link || link.media !== 'print' || link.sheet) return;
    await new Promise(res => {
      const t = setTimeout(res, 3000);
      link.addEventListener('load', () => { clearTimeout(t); res(); }, { once: true });
    });
  }

  Shell.boot = async () => {
    await stylesheetReady();
    T.render($('#page') || document.body, layout());
    bindEvents();
    Shell.buildNav();
    updatePill();
    T.render($('#main'), html`<div class="restart"><div class="spinner"></div><p class="hint">Connecting to your TeHyBug…</p></div>`);
    T.Bus.on('online', updatePill);
    T.Bus.on('offline', updatePill);
    T.Bus.on('info', () => {
      Shell.buildNav();
      updateBrand();
      if (Shell.current && Shell.current.boards && Shell.current.boards.indexOf(T.board()) < 0) Shell.show('dashboard');
    });
    // a config push (or reload after a restart): pages that patch themselves
    // declare on.config; the others are simply drawn again — unless edited
    T.Bus.on('config', () => {
      const p = Shell.current;
      updateSaveBar();
      if (!p) return;
      // Drawn from defaults while the configuration was still missing: the
      // edits were made on top of the wrong values, so redraw regardless and
      // say so — saving them would have written defaults over real settings.
      if (Shell.drawnWithoutConfig) {
        Shell.drawnWithoutConfig = false;
        const hadEdits = Shell.dirty;
        Shell.dirty = false;
        Shell.show(p.id, { keepScroll: true });
        if (hadEdits) Shell.toast('The settings arrived from the device — the page was refreshed, please redo your changes', 'warn', 6000);
        return;
      }
      if (!(p.on && p.on.config)) Shell.rerender();
    });
    // the websocket coming (back) up is the moment to fetch a config that failed earlier
    T.Bus.on('online', () => { if (!T.State.configLoaded) loadConfigWithRetry(); });
    const [info, config] = await Promise.all([T.Api.info().catch(() => null), T.Api.config().catch(() => null)]);
    if (info) T.applyInfo(info);
    if (config) T.applyConfig(config);
    Shell.show((location.hash || '').replace(/^#\/?/, '') || 'dashboard');
    if (!config) { Shell.toast('Could not load the settings from the device yet — retrying', 'warn', 6000); loadConfigWithRetry(); }
    T.Live.start();
  };
  // Keeps asking for the configuration until the device answers: pages show
  // firmware defaults until then, and Save stays off (see updateSaveBar).
  let configRetryTimer = null;
  async function loadConfigWithRetry() {
    clearTimeout(configRetryTimer);
    if (T.State.configLoaded) return;
    try { T.applyConfig(await T.Api.config()); }
    catch (e) { configRetryTimer = setTimeout(loadConfigWithRetry, 5000); }
  }
  document.addEventListener('DOMContentLoaded', () => { T.Shell.boot(); });
})();
