// ClaudeGauge management page — Pages/Buttons editor.
// Talks to the device's JSON API (/api/layout, /api/buttons). No build step,
// no framework — kept small since it's served straight from LittleFS.
(() => {
  const SCALE = 3;
  const DEVICE_W = 240, DEVICE_H = 135;

  const COLOR_HEX = {
    white: '#ffffff', green: '#33cc33', amber: '#ffb400', tomato: '#ff5533',
    lavender: '#b9b6ff', peach: '#ffe0b0', ice: '#a8f0ff', dim: '#666677',
  };

  const ACTIONS = [
    { value: 'none', label: 'None' },
    { value: 'next_page', label: 'Next page' },
    { value: 'prev_page', label: 'Previous page' },
    { value: 'toggle_backlight', label: 'Toggle backlight' },
  ];

  // ---- Tabs ----
  document.querySelectorAll('.tab-btn').forEach(btn => {
    btn.addEventListener('click', () => {
      document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
      document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));
      btn.classList.add('active');
      document.getElementById('tab-' + btn.dataset.tab).classList.add('active');
      if (btn.dataset.tab === 'pages') loadLayout();
      if (btn.dataset.tab === 'buttons') loadButtons();
    });
  });

  // ============================================================
  // Pages / widget editor
  // ============================================================
  let layout = { pages: [] };
  let selectedPageIdx = -1;
  let selectedWidgetIdx = -1;
  let dragWidget = null;

  const pageListEl = document.getElementById('pageList');
  const canvas = document.getElementById('pageCanvas');
  const ctx = canvas.getContext('2d');

  async function loadLayout() {
    const res = await fetch('/api/layout');
    if (!res.ok) return;
    layout = await res.json();
    if (selectedPageIdx < 0 && layout.pages.length) selectedPageIdx = 0;
    renderPageList();
    renderCanvas();
  }

  function renderPageList() {
    pageListEl.innerHTML = '';
    layout.pages.forEach((p, i) => {
      const li = document.createElement('li');
      li.draggable = true;
      li.dataset.idx = i;
      if (i === selectedPageIdx) li.classList.add('selected');
      li.innerHTML = `
        <input type="checkbox" ${p.enabled ? 'checked' : ''} title="enabled">
        <span class="name">${p.id}</span>
        <button class="del-page" title="delete">x</button>`;
      li.querySelector('input').addEventListener('change', e => {
        layout.pages[i].enabled = e.target.checked;
      });
      li.querySelector('.del-page').addEventListener('click', e => {
        e.stopPropagation();
        layout.pages.splice(i, 1);
        selectedPageIdx = layout.pages.length ? 0 : -1;
        renderPageList();
        renderCanvas();
      });
      li.addEventListener('click', () => {
        selectedPageIdx = i;
        selectedWidgetIdx = -1;
        renderPageList();
        renderCanvas();
      });
      li.addEventListener('dragstart', () => li.classList.add('dragging'));
      li.addEventListener('dragend', () => li.classList.remove('dragging'));
      li.addEventListener('dragover', e => e.preventDefault());
      li.addEventListener('drop', e => {
        e.preventDefault();
        const draggingEl = pageListEl.querySelector('.dragging');
        const fromIdx = parseInt(draggingEl.dataset.idx, 10);
        const toIdx = i;
        if (fromIdx === toIdx) return;
        const [moved] = layout.pages.splice(fromIdx, 1);
        layout.pages.splice(toIdx, 0, moved);
        selectedPageIdx = toIdx;
        renderPageList();
      });
      pageListEl.appendChild(li);
    });
  }

  document.getElementById('addPageBtn').addEventListener('click', () => {
    const id = prompt('Page id (short, unique):', 'page' + (layout.pages.length + 1));
    if (!id) return;
    layout.pages.push({ id, enabled: true, widgets: [] });
    selectedPageIdx = layout.pages.length - 1;
    renderPageList();
    renderCanvas();
  });

  function currentPage() {
    return selectedPageIdx >= 0 ? layout.pages[selectedPageIdx] : null;
  }

  function renderCanvas() {
    ctx.fillStyle = '#000';
    ctx.fillRect(0, 0, canvas.width, canvas.height);
    const page = currentPage();
    document.getElementById('editorTitle').textContent = page ? 'Page: ' + page.id : 'Select a page';
    if (!page) return;

    page.widgets.forEach((w, i) => {
      const x = w.x * SCALE, y = w.y * SCALE;
      const color = COLOR_HEX[w.color] || '#fff';
      ctx.strokeStyle = i === selectedWidgetIdx ? '#ff9944' : color;
      ctx.lineWidth = i === selectedWidgetIdx ? 2 : 1;
      ctx.fillStyle = color;
      ctx.font = '11px sans-serif';

      if (w.type === 'donutGauge') {
        const r = (w.r || 34) * SCALE;
        ctx.beginPath();
        ctx.arc(x, y, r, 0, Math.PI * 2);
        ctx.stroke();
        ctx.fillText(w.label || w.bind || 'gauge', x - r, y + r + 14);
      } else if (w.type === 'bar') {
        const w2 = (w.w || 60) * SCALE, h2 = (w.h || 12) * SCALE;
        ctx.strokeRect(x, y, w2, h2);
        ctx.fillText(w.bind || 'bar', x, y - 4);
      } else if (w.type === 'clock7seg') {
        ctx.strokeRect(x, y, 200 * SCALE, 42 * SCALE);
        ctx.fillText('clock', x, y - 4);
      } else { // text
        ctx.fillText(w.label || w.bind || 'text', x, y + 10);
        ctx.strokeRect(x - 2, y - 2, 60, 16);
      }
    });
  }

  canvas.addEventListener('mousedown', e => {
    const page = currentPage();
    if (!page) return;
    const rect = canvas.getBoundingClientRect();
    const mx = (e.clientX - rect.left) * (canvas.width / rect.width) / SCALE;
    const my = (e.clientY - rect.top) * (canvas.height / rect.height) / SCALE;
    // Hit-test from topmost widget down.
    for (let i = page.widgets.length - 1; i >= 0; i--) {
      const w = page.widgets[i];
      const hitR = w.type === 'donutGauge' ? (w.r || 34) : 30;
      if (Math.abs(mx - w.x) < hitR && Math.abs(my - w.y) < hitR) {
        selectedWidgetIdx = i;
        dragWidget = { offX: mx - w.x, offY: my - w.y };
        renderWidgetProps();
        renderCanvas();
        return;
      }
    }
    selectedWidgetIdx = -1;
    renderWidgetProps();
    renderCanvas();
  });

  canvas.addEventListener('mousemove', e => {
    if (!dragWidget || selectedWidgetIdx < 0) return;
    const page = currentPage();
    const rect = canvas.getBoundingClientRect();
    const mx = (e.clientX - rect.left) * (canvas.width / rect.width) / SCALE;
    const my = (e.clientY - rect.top) * (canvas.height / rect.height) / SCALE;
    const w = page.widgets[selectedWidgetIdx];
    w.x = Math.max(0, Math.min(DEVICE_W, Math.round(mx - dragWidget.offX)));
    w.y = Math.max(0, Math.min(DEVICE_H, Math.round(my - dragWidget.offY)));
    renderCanvas();
    renderWidgetProps();
  });
  window.addEventListener('mouseup', () => { dragWidget = null; });

  document.getElementById('addWidgetBtn').addEventListener('click', () => {
    const page = currentPage();
    if (!page) return;
    const type = document.getElementById('addWidgetType').value;
    const w = { type, x: 20, y: 20, color: 'white' };
    if (type === 'donutGauge') { w.r = 34; w.thickness = 7; w.bind = 'limit_5h'; w.label = 'GAUGE'; }
    if (type === 'bar') { w.w = 100; w.h = 12; w.bind = 'gpu_util'; }
    if (type === 'text') { w.label = 'Label'; w.font = 2; }
    page.widgets.push(w);
    selectedWidgetIdx = page.widgets.length - 1;
    renderCanvas();
    renderWidgetProps();
  });

  document.getElementById('deleteWidgetBtn').addEventListener('click', () => {
    const page = currentPage();
    if (!page || selectedWidgetIdx < 0) return;
    page.widgets.splice(selectedWidgetIdx, 1);
    selectedWidgetIdx = -1;
    renderCanvas();
    renderWidgetProps();
  });

  const BIND_OPTIONS = [
    'none', 'limit_5h', 'limit_7d', 'limit_opus', 'limit_sonnet', 'extra_pct',
    'gpu_util', 'gpu_vram', 'limit_opus_label', 'limit_sonnet_label',
    'extra_spend_label', 'ollama_model', 'ollama_model_clock_label',
    'gpu_temp', 'gpu_vram_label', 'data_status', 'wifi_rssi', 'uptime',
    'fetch_age', 'wifi_ip',
  ];

  function renderWidgetProps() {
    const box = document.getElementById('widgetProps');
    const delBtn = document.getElementById('deleteWidgetBtn');
    box.innerHTML = '';
    const page = currentPage();
    if (!page || selectedWidgetIdx < 0) { delBtn.disabled = true; return; }
    delBtn.disabled = false;
    const w = page.widgets[selectedWidgetIdx];

    const field = (labelText, input) => {
      const label = document.createElement('label');
      label.textContent = labelText;
      label.appendChild(input);
      box.appendChild(label);
    };

    const numInput = (key, val) => {
      const inp = document.createElement('input');
      inp.type = 'number';
      inp.value = val ?? 0;
      inp.addEventListener('input', () => { w[key] = parseInt(inp.value, 10) || 0; renderCanvas(); });
      return inp;
    };

    field('X', numInput('x', w.x));
    field('Y', numInput('y', w.y));
    if (w.type === 'donutGauge') {
      field('Radius', numInput('r', w.r));
      field('Thickness', numInput('thickness', w.thickness));
    }
    if (w.type === 'bar') {
      field('Width', numInput('w', w.w));
      field('Height', numInput('h', w.h));
    }

    const colorSel = document.createElement('select');
    Object.keys(COLOR_HEX).forEach(c => {
      const opt = document.createElement('option');
      opt.value = c; opt.textContent = c;
      if (w.color === c) opt.selected = true;
      colorSel.appendChild(opt);
    });
    colorSel.addEventListener('change', () => { w.color = colorSel.value; renderCanvas(); });
    field('Color', colorSel);

    if (w.type !== 'clock7seg') {
      const bindSel = document.createElement('select');
      BIND_OPTIONS.forEach(b => {
        const opt = document.createElement('option');
        opt.value = b === 'none' ? '' : b; opt.textContent = b;
        if ((w.bind || '') === opt.value) opt.selected = true;
        bindSel.appendChild(opt);
      });
      bindSel.addEventListener('change', () => { w.bind = bindSel.value; renderCanvas(); });
      field('Data bind', bindSel);
    }

    if (w.type === 'text' || w.type === 'donutGauge') {
      const labelInp = document.createElement('input');
      labelInp.type = 'text';
      labelInp.value = w.label || '';
      labelInp.addEventListener('input', () => { w.label = labelInp.value; renderCanvas(); });
      field('Label', labelInp);
    }
  }

  document.getElementById('saveLayoutBtn').addEventListener('click', async () => {
    const status = document.getElementById('layoutStatus');
    status.textContent = 'Saving...';
    const res = await fetch('/api/layout', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(layout),
    });
    status.textContent = res.ok ? 'Saved.' : 'Save failed — check widget bounds.';
  });

  // ============================================================
  // Buttons tab
  // ============================================================
  function actionOptionsHtml(pages) {
    let opts = ACTIONS.map(a => `<option value="${a.value}">${a.label}</option>`).join('');
    pages.forEach(p => { opts += `<option value="goto_page:${p.id}">Go to "${p.id}"</option>`; });
    return opts;
  }

  async function loadButtons() {
    const [btnRes, layoutRes] = await Promise.all([fetch('/api/buttons'), fetch('/api/layout')]);
    if (!btnRes.ok || !layoutRes.ok) return;
    const buttons = await btnRes.json();
    const currentLayout = await layoutRes.json();
    const optsHtml = actionOptionsHtml(currentLayout.pages || []);

    ['btn1_short', 'btn2_short', 'btn1_long', 'btn2_long'].forEach(key => {
      const sel = document.getElementById(key);
      sel.innerHTML = optsHtml;
      sel.value = buttons[key] || 'none';
    });
  }

  document.getElementById('saveButtonsBtn').addEventListener('click', async () => {
    const status = document.getElementById('buttonsStatus');
    const body = {};
    ['btn1_short', 'btn2_short', 'btn1_long', 'btn2_long'].forEach(key => {
      body[key] = document.getElementById(key).value;
    });
    status.textContent = 'Saving...';
    const res = await fetch('/api/buttons', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(body),
    });
    status.textContent = res.ok ? 'Saved.' : 'Save failed.';
  });
})();
