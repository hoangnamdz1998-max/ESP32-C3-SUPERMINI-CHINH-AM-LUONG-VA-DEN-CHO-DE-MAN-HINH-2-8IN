#ifndef WEBPAGE_H
#define WEBPAGE_H

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
  <title>Desk LED & Media Controller</title>
  <style>
    :root {
      --bg: #121216;
      --card: #1c1c24;
      --card-border: #2c2c38;
      --accent: #ff6b00;
      --accent-glow: rgba(255, 107, 0, 0.4);
      --text: #f0f0f5;
      --text-sub: #9090a0;
      --success: #10b981;
      --danger: #ef4444;
    }
    * { box-sizing: border-box; margin: 0; padding: 0; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif; -webkit-tap-highlight-color: transparent; }
    body { background-color: var(--bg); color: var(--text); padding-bottom: 40px; user-select: none; }
    
    header {
      background: rgba(28, 28, 36, 0.9);
      backdrop-filter: blur(14px);
      position: sticky; top: 0; z-index: 100;
      border-bottom: 1px solid var(--card-border);
      padding: 12px 18px;
      display: flex; align-items: center; justify-content: space-between;
    }
    .brand { display: flex; align-items: center; gap: 10px; font-weight: 700; font-size: 1.1rem; }
    .brand-icon { color: var(--accent); font-size: 1.3rem; }
    
    .power-btn {
      width: 44px; height: 44px; border-radius: 50%;
      background: #252530; border: 2px solid #3a3a4c; color: #888;
      display: flex; align-items: center; justify-content: center;
      font-size: 1.35rem; cursor: pointer; transition: all 0.25s ease;
    }
    .power-btn.active {
      background: var(--accent); border-color: var(--accent); color: #fff;
      box-shadow: 0 0 16px var(--accent-glow);
    }

    .container { max-width: 520px; margin: 0 auto; padding: 14px 16px; }
    .card {
      background: var(--card); border: 1px solid var(--card-border);
      border-radius: 16px; padding: 16px; margin-bottom: 14px;
      box-shadow: 0 4px 12px rgba(0,0,0,0.25);
    }
    .card-title {
      font-size: 0.88rem; font-weight: 700; color: var(--text-sub);
      text-transform: uppercase; letter-spacing: 0.6px; margin-bottom: 14px;
      display: flex; justify-content: space-between; align-items: center;
    }

    .slider-wrap { margin-bottom: 14px; }
    .slider-wrap:last-child { margin-bottom: 0; }
    .slider-label { display: flex; justify-content: space-between; font-size: 0.92rem; margin-bottom: 8px; }
    .slider-label .val { font-weight: 700; color: var(--accent); }
    input[type=range] {
      width: 100%; height: 8px; border-radius: 5px; background: #282836;
      outline: none; -webkit-appearance: none;
    }
    input[type=range]::-webkit-slider-thumb {
      -webkit-appearance: none; width: 22px; height: 22px; border-radius: 50%;
      background: #fff; border: 3px solid var(--accent);
      box-shadow: 0 0 10px rgba(0,0,0,0.6); cursor: pointer;
    }

    /* Color picker section */
    .color-section { display: flex; flex-direction: column; align-items: center; gap: 14px; }
    #colorCanvas { border-radius: 50%; cursor: crosshair; touch-action: none; box-shadow: 0 0 24px rgba(0,0,0,0.5); }
    .color-preview-bar {
      display: flex; align-items: center; gap: 12px; width: 100%;
      background: #16161e; padding: 8px 12px; border-radius: 10px; border: 1px solid var(--card-border);
    }
    .color-preview-box {
      width: 38px; height: 38px; border-radius: 8px; border: 2px solid #fff;
      box-shadow: 0 0 8px rgba(0,0,0,0.5); flex-shrink: 0;
    }
    .color-preview-hex { font-family: monospace; font-size: 1rem; font-weight: 700; color: #fff; flex: 1; }

    .quick-colors { display: grid; grid-template-columns: repeat(6, 1fr); gap: 10px; width: 100%; }
    .color-swatch {
      height: 38px; border-radius: 10px; cursor: pointer;
      border: 2px solid rgba(255,255,255,0.15); transition: transform 0.15s, box-shadow 0.15s;
    }
    .color-swatch:active { transform: scale(0.9); }

    /* Effects grid */
    .effects-grid { display: grid; grid-template-columns: repeat(2, 1fr); gap: 10px; }
    .effect-btn {
      background: #242430; border: 1px solid var(--card-border); color: var(--text);
      padding: 13px 10px; border-radius: 12px; font-size: 0.88rem; font-weight: 600;
      text-align: center; cursor: pointer; transition: all 0.2s ease;
      display: flex; align-items: center; justify-content: center; gap: 6px;
    }
    .effect-btn.active {
      background: var(--accent); color: #fff; border-color: var(--accent);
      box-shadow: 0 0 14px var(--accent-glow);
    }
    .effect-btn.featured {
      grid-column: span 2;
      background: linear-gradient(135deg, #ff6b00 0%, #ff8800 100%);
      color: #fff; border-color: #ff9922; font-size: 0.95rem; font-weight: 700;
      box-shadow: 0 4px 14px var(--accent-glow);
    }
    .effect-btn.featured.active {
      border-color: #fff; box-shadow: 0 0 20px rgba(255,107,0,0.8);
    }

    /* Settings section */
    .form-group { margin-bottom: 12px; }
    .form-group label { display: block; font-size: 0.82rem; color: var(--text-sub); margin-bottom: 6px; }
    .form-row { display: flex; gap: 10px; }
    .form-input {
      flex: 1; background: #16161e; border: 1px solid var(--card-border);
      border-radius: 10px; color: #fff; padding: 11px 14px; font-size: 0.95rem; outline: none;
    }
    .form-btn {
      background: var(--accent); border: none; border-radius: 10px;
      color: #fff; font-size: 0.9rem; font-weight: 700; padding: 11px 18px;
      cursor: pointer; box-shadow: 0 2px 10px var(--accent-glow);
      transition: transform 0.15s; white-space: nowrap;
    }
    .form-btn:active { transform: scale(0.96); }

    .toast {
      position: fixed; bottom: 18px; left: 50%; transform: translateX(-50%) translateY(100px);
      background: #252532; border: 1px solid var(--success); color: #fff;
      padding: 10px 22px; border-radius: 25px; font-size: 0.88rem; font-weight: 600;
      box-shadow: 0 8px 24px rgba(0,0,0,0.5); opacity: 0; transition: all 0.3s cubic-bezier(0.18, 0.89, 0.32, 1.28);
      z-index: 200; pointer-events: none;
    }
    .toast.show { transform: translateX(-50%) translateY(0); opacity: 1; }

    .badge-pin {
      display: inline-block; background: #2a2a38; padding: 3px 8px; border-radius: 6px;
      font-size: 0.78rem; color: var(--accent); font-weight: 700;
    }
  </style>
</head>
<body>

  <!-- Header -->
  <header>
    <div class="brand">
      <span class="brand-icon">✦</span>
      <span>Đế Màn Hình 2.8" & LED</span>
    </div>
    <div class="power-btn active" id="powerBtn" onclick="togglePower()">⏻</div>
  </header>

  <div class="container">

    <!-- MASTER CONTROLS -->
    <div class="card">
      <div class="slider-wrap">
        <div class="slider-label"><span>Độ sáng LED</span><span class="val" id="briVal">100%</span></div>
        <input type="range" id="briSlider" min="0" max="255" value="255" oninput="onBriChange(this.value)">
      </div>
      <div class="slider-wrap">
        <div class="slider-label"><span>Tốc độ hiệu ứng</span><span class="val" id="spdVal">50%</span></div>
        <input type="range" id="spdSlider" min="5" max="100" value="50" oninput="onSpdChange(this.value)">
      </div>
    </div>

    <!-- EFFECTS CARD -->
    <div class="card">
      <div class="card-title">
        <span>Chọn Hiệu Ứng LED</span>
        <span class="badge-pin">Chân LED: GPIO 1</span>
      </div>
      <div class="effects-grid" id="effectsGrid">
        <div class="effect-btn featured active" onclick="setEffect(1)">
          🌟 Chạy từ giữa ra 2 đầu (Center Out)
        </div>
        <div class="effect-btn" onclick="setEffect(2)">🔄 Từ 2 đầu vào giữa</div>
        <div class="effect-btn" onclick="setEffect(3)">🌈 Cầu vồng cuộn</div>
        <div class="effect-btn" onclick="setEffect(0)">💡 Sáng tĩnh 1 màu</div>
        <div class="effect-btn" onclick="setEffect(4)">🌬️ Thở nhịp (Breathe)</div>
        <div class="effect-btn" onclick="setEffect(5)">⚡ Đuổi màu (Chase)</div>
        <div class="effect-btn" onclick="setEffect(6)">💫 Quét màu (Wipe)</div>
        <div class="effect-btn" onclick="setEffect(7)">✨ Sao lấp lánh</div>
        <div class="effect-btn" onclick="setEffect(8)">☄️ Sao băng (Meteor)</div>
        <div class="effect-btn" onclick="setEffect(9)">🔥 Ngọn lửa ấm</div>
        <div class="effect-btn" onclick="setEffect(10)">⚡ Chớp nháy (Strobe)</div>
      </div>
      <div style="margin-top: 12px;">
        <button class="form-btn" id="rainbowDirBtn" onclick="toggleRainbowDir()" style="width: 100%; background: #2f3542; border: 1px solid var(--card-border); padding: 12px; font-size: 0.9rem;">
          🔄 Chiều chạy Cầu vồng: Thuận (➡️)
        </button>
      </div>
    </div>

    <!-- COLOR PICKER CARD -->
    <div class="card">
      <div class="card-title"><span>Chọn Màu Sắc</span></div>
      <div class="color-section">
        <canvas id="colorCanvas" width="240" height="240"></canvas>
        <div class="color-preview-bar">
          <div class="color-preview-box" id="previewBox" style="background:#ff6b00"></div>
          <div class="color-preview-hex" id="previewHex">#FF6B00</div>
        </div>
        <div class="quick-colors">
          <div class="color-swatch" style="background:#ff0000" onclick="setColorHex('#ff0000')"></div>
          <div class="color-swatch" style="background:#ff6b00" onclick="setColorHex('#ff6b00')"></div>
          <div class="color-swatch" style="background:#ffd000" onclick="setColorHex('#ffd000')"></div>
          <div class="color-swatch" style="background:#10b981" onclick="setColorHex('#10b981')"></div>
          <div class="color-swatch" style="background:#06b6d4" onclick="setColorHex('#06b6d4')"></div>
          <div class="color-swatch" style="background:#3b82f6" onclick="setColorHex('#3b82f6')"></div>
          <div class="color-swatch" style="background:#8b5cf6" onclick="setColorHex('#8b5cf6')"></div>
          <div class="color-swatch" style="background:#ec4899" onclick="setColorHex('#ec4899')"></div>
          <div class="color-swatch" style="background:#ffffff" onclick="setColorHex('#ffffff')"></div>
          <div class="color-swatch" style="background:#ffe6b3" onclick="setColorHex('#ffe6b3')"></div>
          <div class="color-swatch" style="background:#ffa852" onclick="setColorHex('#ffa852')"></div>
          <div class="color-swatch" style="background:#000000" onclick="setColorHex('#000000')"></div>
        </div>
      </div>
    </div>

    <!-- SETTINGS CARD -->
    <div class="card">
      <div class="card-title"><span>Cài Đặt Phần Cứng</span></div>
      <div class="form-group">
        <label>Số lượng bóng LED trên dây (LED Count)</label>
        <div class="form-row">
          <input type="number" id="ledCountInput" class="form-input" min="1" max="300" value="60">
          <button class="form-btn" onclick="saveLedCount()">Lưu số LED</button>
        </div>
      </div>
    </div>

  </div>

  <div class="toast" id="toast">Đã lưu cài đặt!</div>

  <script>
    let state = { power: 1, bri: 255, spd: 50, count: 60, fx: 1, r: 255, g: 107, b: 0, rb_rev: 0 };
    let briTimeout = null;
    let spdTimeout = null;

    function showToast(msg) {
      const t = document.getElementById('toast');
      t.innerText = msg;
      t.classList.add('show');
      setTimeout(() => t.classList.remove('show'), 1800);
    }

    function togglePower() {
      state.power = state.power ? 0 : 1;
      updatePowerUI();
      fetch('/api/power?val=' + state.power);
    }

    function updatePowerUI() {
      const btn = document.getElementById('powerBtn');
      if (state.power) {
        btn.classList.add('active');
      } else {
        btn.classList.remove('active');
      }
    }

    function onBriChange(val) {
      document.getElementById('briVal').innerText = Math.round((val / 255) * 100) + '%';
      state.bri = parseInt(val);
      clearTimeout(briTimeout);
      briTimeout = setTimeout(() => {
        fetch('/api/bri?val=' + val);
      }, 60);
    }

    function onSpdChange(val) {
      document.getElementById('spdVal').innerText = val + '%';
      state.spd = parseInt(val);
      clearTimeout(spdTimeout);
      spdTimeout = setTimeout(() => {
        fetch('/api/spd?val=' + val);
      }, 60);
    }

    function toggleRainbowDir() {
      fetch('/api/rainbow_dir')
        .then(r => r.text())
        .then(val => {
          state.rb_rev = (val === '1' ? 1 : 0);
          updateRainbowDirUI();
          showToast(state.rb_rev ? 'Cầu vồng: Chiều Nghịch (⬅️)' : 'Cầu vồng: Chiều Thuận (➡️)');
        });
    }

    function updateRainbowDirUI() {
      const btn = document.getElementById('rainbowDirBtn');
      if (btn) {
        if (state.rb_rev) {
          btn.innerHTML = '🔄 Chiều Cầu vồng: Nghịch (⬅️)';
          btn.style.borderColor = 'var(--accent)';
          btn.style.color = '#fff';
        } else {
          btn.innerHTML = '🔄 Chiều Cầu vồng: Thuận (➡️)';
          btn.style.borderColor = 'var(--card-border)';
          btn.style.color = 'var(--text)';
        }
      }
    }

    function setEffect(fxIdx) {
      state.fx = fxIdx;
      updateEffectUI();
      fetch('/api/fx?val=' + fxIdx);
      showToast('Đã chọn hiệu ứng #' + fxIdx);
    }

    function updateEffectUI() {
      const btns = document.querySelectorAll('.effect-btn');
      btns.forEach((btn, idx) => {
        const onclickAttr = btn.getAttribute('onclick') || '';
        const match = onclickAttr.match(/setEffect\((\d+)\)/);
        if (match && parseInt(match[1]) === state.fx) {
          btn.classList.add('active');
        } else {
          btn.classList.remove('active');
        }
      });
    }

    function saveLedCount() {
      const count = parseInt(document.getElementById('ledCountInput').value);
      if (isNaN(count) || count < 1 || count > 300) {
        alert('Vui lòng nhập số LED từ 1 đến 300');
        return;
      }
      state.count = count;
      fetch('/api/ledcount?val=' + count)
        .then(() => showToast('Đã cập nhật: ' + count + ' LED!'));
    }

    // Color Canvas Wheel
    const canvas = document.getElementById('colorCanvas');
    const ctx = canvas.getContext('2d');
    const radius = canvas.width / 2;

    function drawColorWheel() {
      const image = ctx.createImageData(canvas.width, canvas.height);
      const data = image.data;
      for (let x = -radius; x < radius; x++) {
        for (let y = -radius; y < radius; y++) {
          const [r, phi] = xy2polar(x, y);
          if (r <= radius) {
            const deg = rad2deg(phi);
            const [red, green, blue] = hsv2rgb(deg, r / radius, 1.0);
            const index = ((y + radius) * canvas.width + (x + radius)) * 4;
            data[index] = red;
            data[index + 1] = green;
            data[index + 2] = blue;
            data[index + 3] = 255;
          }
        }
      }
      ctx.putImageData(image, 0, 0);
    }

    function xy2polar(x, y) {
      const r = Math.sqrt(x * x + y * y);
      const phi = Math.atan2(y, x);
      return [r, phi];
    }

    function rad2deg(rad) {
      return ((rad + Math.PI) / (2 * Math.PI)) * 360;
    }

    function hsv2rgb(h, s, v) {
      let f = (n, k = (n + h / 60) % 6) => v - v * s * Math.max(Math.min(k, 4 - k, 1), 0);
      return [Math.round(f(5) * 255), Math.round(f(3) * 255), Math.round(f(1) * 255)];
    }

    function handleCanvasEvent(e) {
      e.preventDefault();
      const rect = canvas.getBoundingClientRect();
      const clientX = e.touches ? e.touches[0].clientX : e.clientX;
      const clientY = e.touches ? e.touches[0].clientY : e.clientY;
      const x = clientX - rect.left - radius;
      const y = clientY - rect.top - radius;
      const [r, phi] = xy2polar(x, y);
      if (r <= radius) {
        const deg = rad2deg(phi);
        const [red, green, blue] = hsv2rgb(deg, r / radius, 1.0);
        sendColorRGB(red, green, blue);
      }
    }

    canvas.addEventListener('mousedown', (e) => {
      handleCanvasEvent(e);
      window.addEventListener('mousemove', handleCanvasEvent);
      window.addEventListener('mouseup', () => {
        window.removeEventListener('mousemove', handleCanvasEvent);
      }, { once: true });
    });

    canvas.addEventListener('touchstart', handleCanvasEvent, { passive: false });
    canvas.addEventListener('touchmove', handleCanvasEvent, { passive: false });

    let colorTimeout = null;
    function sendColorRGB(r, g, b) {
      state.r = r; state.g = g; state.b = b;
      updateColorPreview(r, g, b);
      clearTimeout(colorTimeout);
      colorTimeout = setTimeout(() => {
        fetch(`/api/color?r=${r}&g=${g}&b=${b}`);
      }, 50);
    }

    function setColorHex(hex) {
      const r = parseInt(hex.slice(1, 3), 16);
      const g = parseInt(hex.slice(3, 5), 16);
      const b = parseInt(hex.slice(5, 7), 16);
      sendColorRGB(r, g, b);
    }

    function updateColorPreview(r, g, b) {
      const hex = '#' + ((1 << 24) + (r << 16) + (g << 8) + b).toString(16).slice(1).toUpperCase();
      document.getElementById('previewBox').style.backgroundColor = `rgb(${r},${g},${b})`;
      document.getElementById('previewHex').innerText = hex;
    }

    // Sync state on load
    function loadInitialState() {
      drawColorWheel();
      fetch('/api/state')
        .then(res => res.json())
        .then(data => {
          state = data;
          document.getElementById('briSlider').value = state.bri;
          document.getElementById('briVal').innerText = Math.round((state.bri / 255) * 100) + '%';
          document.getElementById('spdSlider').value = state.spd;
          document.getElementById('spdVal').innerText = state.spd + '%';
          document.getElementById('ledCountInput').value = state.count;
          updatePowerUI();
          updateEffectUI();
          updateColorPreview(state.r, state.g, state.b);
        })
        .catch(err => console.log('Init state load failed', err));
    }

    window.onload = loadInitialState;
  </script>
</body>
</html>
)rawliteral";

#endif
