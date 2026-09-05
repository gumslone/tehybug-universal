/* Firmware: what runs, how to update it from the phone, and the changelog. */
(function () {
  'use strict';
  const T = window.TeHyBug, html = T.html, UI = T.UI, $ = T.$;
  const RAW = T.REPO + '/raw/main/firmware/';
  const BUILDS = [
    ['esp8285', 'TeHyBug universal & Mini (ESP8285)', 'universal'],
    ['esp8285_debug', 'universal & Mini, with serial debug output', 'universal'],
    ['display', 'Display Weatherstation', 'display'],
    ['display_debug', 'Display Weatherstation, with serial debug output', 'display'],
    ['generic', 'First-generation TeHyBug (esp-01, 1 MB)', 'generic'],
    ['generic_debug', 'First-generation, with serial debug output', 'generic']
  ];
  let picked = null;

  function fileMatchesBoard(name) {
    const b = T.buildName();
    if (!b) return true;
    const m = /tehybug\.ino\.([a-z0-9_]+)\.bin/i.exec(name);
    return !m || m[1].replace('_debug', '') === b;
  }

  async function install() {
    if (!picked) return;
    const mismatch = !fileMatchesBoard(picked.name);
    const ok = await T.Shell.confirm({
      title: 'Install ' + picked.name + '?',
      okLabel: mismatch ? 'Install anyway' : 'Install',
      danger: mismatch,
      body: html`<p>${T.fmt.bytes(picked.size)} will be uploaded and written to the device, which then restarts. Your settings are kept. <strong>Do not cut the power while it installs.</strong></p>
        ${mismatch ? UI.note('danger', html`This file does not look like the <strong>${T.buildName()}</strong> build your device reports. The wrong build can leave the device without a working web interface.`) : ''}`
    });
    if (!ok) return;
    const bar = $('#ota-progress'), fill = $('#ota-progress div'), status = $('#ota-status'), btn = $('#ota-install');
    bar.hidden = false;
    btn.disabled = true;
    status.textContent = 'Uploading…';
    try {
      await T.Api.uploadFirmware(picked, p => { fill.style.width = Math.round(p * 100) + '%'; status.textContent = 'Uploading… ' + Math.round(p * 100) + '%'; });
      fill.style.width = '100%';
      status.textContent = 'Uploaded — installing.';
      await T.Restart.wait({ title: 'Installing…', text: 'The device is writing the new firmware and restarting. This takes up to a minute.', initialMs: 8000, timeoutMs: 120000 });
    } catch (e) {
      status.textContent = '';
      T.Shell.dialog({ title: 'Update failed', body: html`<p>${e.message}</p><p class="hint">The device keeps its previous firmware. Check that the file is the right build and try again.</p>`, buttons: [{ label: 'Close' }] });
    } finally { btn.disabled = !picked; }
  }

  // Compares the installed version with the newest GitHub release. Versions
  // are semantic since 1.0.0; builds before that reported a date stamp,
  // which compares as older than any semantic version.
  function versionParts(v) {
    const m = /^v?(\d+)\.(\d+)\.(\d+)/.exec(String(v || '').trim());
    return m ? [+m[1], +m[2], +m[3]] : null;
  }
  function newerThan(a, b) {
    const pa = versionParts(a), pb = versionParts(b);
    if (!pa) return false;
    if (!pb) return true;
    for (let i = 0; i < 3; i++) { if (pa[i] !== pb[i]) return pa[i] > pb[i]; }
    return false;
  }
  function checkLatest() {
    const target = $('#update-check');
    if (!target) return;
    fetch('https://api.github.com/repos/gumslone/tehybug-universal/releases/latest', { cache: 'no-store' })
      .then(r => { if (!r.ok) throw new Error(r.status); return r.json(); })
      .then(rel => {
        const el = $('#update-check');
        if (!el) return;
        const latest = rel.tag_name || rel.name || '';
        const installed = T.State.info.gumboardVersion;
        if (newerThan(latest, installed)) {
          T.render(el, html`${UI.note('info', html`<strong>${latest.replace(/^v/, '')} is available</strong> (you run ${installed || '?'}). ${rel.html_url ? html`<a href="${rel.html_url}" target="_blank" rel="noopener">What changed</a> · ` : ''}download your board's file below and install it.`)}`);
        } else {
          T.render(el, html`You run the newest release${latest ? ' (' + latest.replace(/^v/, '') + ')' : ''}.`);
        }
      })
      .catch(() => { const el = $('#update-check'); if (el) T.render(el, html`Could not check GitHub for a newer release — see the changelog below.`); });
  }

  function loadChangelog() {
    const target = $('#changelog');
    if (!target) return;
    // quotes too: the link URL lands in an href attribute on the device's origin
    const escape = s => s.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g, '&quot;').replace(/'/g, '&#39;');
    const inline = s => escape(s).replace(/`([^`]+)`/g, '<code>$1</code>').replace(/\*\*([^*]+)\*\*/g, '<strong>$1</strong>').replace(/\[([^\]]+)\]\((https?:[^)\s"'<>]+)\)/g, '<a href="$2" target="_blank" rel="noopener">$1</a>');
    function md(text) {
      const out = [];
      let inList = false, para = [];
      const closeList = () => { if (inList) { out.push('</ul>'); inList = false; } };
      const flush = () => { if (para.length) { out.push('<p>' + para.join(' ') + '</p>'); para = []; } };
      text.split('\n').forEach(line => {
        let m;
        if ((m = /^###\s+(.*)/.exec(line))) { flush(); closeList(); out.push('<h4>' + inline(m[1]) + '</h4>'); }
        else if ((m = /^##\s+(.*)/.exec(line))) { flush(); closeList(); out.push('<h3 class="mt">' + inline(m[1]) + '</h3>'); }
        else if (/^#\s+/.test(line)) { flush(); closeList(); }
        else if ((m = /^\s*[-*]\s+(.*)/.exec(line))) { flush(); if (!inList) { out.push('<ul>'); inList = true; } out.push('<li>' + inline(m[1]) + '</li>'); }
        else if (line.trim() === '') { flush(); closeList(); }
        else para.push(inline(line));
      });
      flush(); closeList();
      return out.join('\n');
    }
    fetch('https://raw.githubusercontent.com/gumslone/tehybug-universal/main/CHANGELOG.md', { cache: 'no-store' })
      .then(r => { if (!r.ok) throw new Error(r.status); return r.text(); })
      .then(text => { target.innerHTML = md(text); })
      .catch(() => { T.render(target, html`<p class="hint">Could not load the changelog — read it on <a href="${T.REPO}/blob/main/CHANGELOG.md" target="_blank" rel="noopener">GitHub</a>.</p>`); });
  }

  T.definePage({
    id: 'firmware', title: 'Firmware',
    nav: { group: 'more', icon: 'download', order: 3 },
    render() {
      const i = T.State.info;
      const mine = T.buildName();
      return html`${UI.pagehead('Firmware')}
        ${UI.card({ title: 'Installed', icon: 'cpu', body: html`${UI.kv([
          ['Version', i.gumboardVersion ? i.gumboardVersion + ' (build ' + (i.fwBuild || '—') + ')' : '…'],
          ['Board', T.boardName() + (mine ? ' — the ' + mine + ' build' : '')],
          ['Room for an update', i.freeSketchSpace ? T.fmt.bytes(i.freeSketchSpace) : '']
        ])}<div id="update-check" class="hint mt">Checking for a newer release…</div>` })}
        ${UI.note('info', html`<strong>Update only if you need to.</strong> If the device does what you want, leave it: every update can change behaviour you rely on. Update for a feature you miss or a problem you have, and note your current version first.`)}
        ${UI.card({ title: 'Install an update', icon: 'upload', body: html`
          <div class="filepick" data-nosave>
            <input type="file" id="ota-file" accept=".bin,.bin.gz">
            <label for="ota-file" class="btn">${T.icon('file-text')} Choose .bin file</label>
            <span class="fname hint" id="ota-name">No file chosen</span>
          </div>
          <div class="row mt"><button type="button" class="btn btn-primary" id="ota-install" disabled>${T.icon('upload')} Install</button><span class="hint" id="ota-status"></span></div>
          <div class="progress mt" id="ota-progress" hidden><div></div></div>
          <p class="hint mt">Download the file for your board below, pick it here, install. The device flashes it and restarts; settings are kept. The device's own bare upload page is at <a href="/update" target="_blank" rel="noopener">/update</a>.</p>` })}
        ${UI.card({ title: 'Downloads', icon: 'download', body: html`
          <p class="hint">The current binaries from the repository. <a href="${T.REPO}/releases/latest" target="_blank" rel="noopener">Releases on GitHub</a> list what changed.</p>
          ${UI.table(['Build', 'For', ''], BUILDS.map(b => [html`<code>${b[0]}</code>${b[2] === mine && !/_debug/.test(b[0]) ? html` <span class="badge ok">your board</span>` : ''}`, b[1], html`<a class="btn btn-sm" href="${RAW}tehybug.ino.${b[0]}.bin" target="_blank" rel="noopener">${T.icon('download')} Download</a>`]))}
          <p class="hint">The <code>_debug</code> builds print over serial and are larger — only for troubleshooting.</p>` })}
        ${UI.card({ title: 'Changelog', icon: 'list', body: html`<div id="changelog" class="small"><p class="hint">Loading…</p></div>` })}`;
    },
    mount(root) {
      loadChangelog();
      checkLatest();
      root.addEventListener('change', e => {
        if (e.target.id !== 'ota-file') return;
        picked = e.target.files && e.target.files[0] ? e.target.files[0] : null;
        $('#ota-name').textContent = picked ? picked.name + ' (' + T.fmt.bytes(picked.size) + ')' : 'No file chosen';
        $('#ota-install').disabled = !picked;
        $('#ota-status').textContent = picked && !fileMatchesBoard(picked.name) ? 'This does not look like the ' + T.buildName() + ' build.' : '';
      });
      root.addEventListener('click', e => { if (e.target.closest('#ota-install')) install(); });
    },
    unmount() { picked = null; }
  });
})();
