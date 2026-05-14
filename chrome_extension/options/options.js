document.addEventListener('DOMContentLoaded', async () => {
  const config = await Storage.getConfig();
  loadConfigToForm(config);
  initTabs();
  loadHistory();

  if (window.location.hash === '#history') {
    switchTab('history');
  }

  document.getElementById('btn-save').addEventListener('click', saveConfig);
  document.getElementById('btn-reset').addEventListener('click', resetConfig);
  document.getElementById('btn-test-translate').addEventListener('click', () => testApi('translate'));
  document.getElementById('btn-test-ocr').addEventListener('click', () => testApi('ocr'));
  document.getElementById('btn-clear-history').addEventListener('click', clearHistory);
});

function loadConfigToForm(config) {
  document.getElementById('translate-api-host').value = config.translateApiHost;
  document.getElementById('translate-api-port').value = config.translateApiPort;
  document.getElementById('translate-api-key').value = config.translateApiKey;
  document.getElementById('translate-model').value = config.translateModel;
  document.getElementById('api-timeout').value = config.apiTimeout;

  document.getElementById('ocr-api-host').value = config.ocrApiHost;
  document.getElementById('ocr-api-port').value = config.ocrApiPort;
  document.getElementById('ocr-api-key').value = config.ocrApiKey;
  document.getElementById('ocr-model').value = config.ocrModel;

  document.getElementById('source-language').value = config.sourceLanguage;
  document.getElementById('target-language').value = config.targetLanguage;

  document.getElementById('opt-enable-text-translate').checked = config.enableTextTranslate;
  document.getElementById('opt-enable-image-translate').checked = config.enableImageTranslate;
  document.getElementById('opt-enable-context-menu').checked = config.enableContextMenu;

  document.getElementById('translate-prompt').value = config.translatePrompt;
  document.getElementById('ocr-prompt').value = config.ocrPrompt;
  document.getElementById('summarize-prompt').value = config.summarizePrompt;

  document.getElementById('opt-enable-gesture').checked = config.enableGesture;
  document.getElementById('gesture-button').value = config.gestureButton;
  document.getElementById('gesture-threshold').value = config.gestureThreshold;
}

function getConfigFromForm() {
  return {
    translateApiHost: document.getElementById('translate-api-host').value,
    translateApiPort: parseInt(document.getElementById('translate-api-port').value) || 8110,
    translateApiKey: document.getElementById('translate-api-key').value,
    translateModel: document.getElementById('translate-model').value,
    apiTimeout: parseInt(document.getElementById('api-timeout').value) || 180,

    ocrApiHost: document.getElementById('ocr-api-host').value,
    ocrApiPort: parseInt(document.getElementById('ocr-api-port').value) || 8111,
    ocrApiKey: document.getElementById('ocr-api-key').value,
    ocrModel: document.getElementById('ocr-model').value,

    sourceLanguage: document.getElementById('source-language').value,
    targetLanguage: document.getElementById('target-language').value,

    enableTextTranslate: document.getElementById('opt-enable-text-translate').checked,
    enableImageTranslate: document.getElementById('opt-enable-image-translate').checked,
    enableContextMenu: document.getElementById('opt-enable-context-menu').checked,

    translatePrompt: document.getElementById('translate-prompt').value,
    ocrPrompt: document.getElementById('ocr-prompt').value,
    summarizePrompt: document.getElementById('summarize-prompt').value,

    enableGesture: document.getElementById('opt-enable-gesture').checked,
    gestureButton: document.getElementById('gesture-button').value,
    gestureThreshold: parseInt(document.getElementById('gesture-threshold').value) || 30,
  };
}

async function saveConfig() {
  const config = getConfigFromForm();
  await Storage.saveConfig(config);

  chrome.runtime.sendMessage({
    type: 'update-context-menu',
    enabled: config.enableContextMenu
  });

  const status = document.getElementById('save-status');
  status.textContent = '✓ 已保存';
  status.classList.add('show');
  setTimeout(() => status.classList.remove('show'), 2000);
}

async function resetConfig() {
  if (!confirm('确定要恢复默认设置吗？')) return;
  await Storage.saveConfig(DEFAULT_CONFIG);
  loadConfigToForm(DEFAULT_CONFIG);

  const status = document.getElementById('save-status');
  status.textContent = '✓ 已恢复默认';
  status.classList.add('show');
  setTimeout(() => status.classList.remove('show'), 2000);
}

async function testApi(type) {
  const btn = document.getElementById(`btn-test-${type}`);
  const originalText = btn.textContent;
  btn.textContent = '测试中...';
  btn.disabled = true;

  const config = getConfigFromForm();
  let url;

  if (type === 'translate') {
    url = `${config.translateApiHost}:${config.translateApiPort}/v1/models`;
  } else {
    url = `${config.ocrApiHost}:${config.ocrApiPort}/v1/models`;
  }

  try {
    const response = await fetch(url, {
      method: 'GET',
      signal: AbortSignal.timeout(5000)
    });

    if (response.ok) {
      btn.textContent = '✓ 连接成功';
      btn.className = 'btn-test success';
    } else {
      btn.textContent = `✗ HTTP ${response.status}`;
      btn.className = 'btn-test error';
    }
  } catch (error) {
    btn.textContent = '✗ 连接失败';
    btn.className = 'btn-test error';
  }

  setTimeout(() => {
    btn.textContent = originalText;
    btn.className = 'btn-test';
    btn.disabled = false;
  }, 2000);
}

function initTabs() {
  document.querySelectorAll('.nav-tab').forEach(tab => {
    tab.addEventListener('click', () => {
      switchTab(tab.dataset.tab);
    });
  });
}

function switchTab(tabName) {
  document.querySelectorAll('.nav-tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.tab-panel').forEach(p => p.classList.remove('active'));

  document.querySelector(`.nav-tab[data-tab="${tabName}"]`).classList.add('active');
  document.getElementById(`tab-${tabName}`).classList.add('active');

  if (tabName === 'history') {
    loadHistory();
  }
}

async function loadHistory() {
  const history = await Storage.getHistory();
  const list = document.getElementById('history-list');
  const count = document.getElementById('history-count');

  count.textContent = `${history.length} 条记录`;

  if (history.length === 0) {
    list.innerHTML = '<div class="history-empty">暂无翻译历史</div>';
    return;
  }

  list.innerHTML = history.map(item => {
    const typeLabels = {
      'translate': '文字翻译',
      'ocr-translate': '图片翻译',
      'summarize': '页面摘要'
    };

    const time = new Date(item.timestamp).toLocaleString('zh-CN');

    return `
      <div class="history-item">
        <span class="history-type ${item.type}">${typeLabels[item.type] || item.type}</span>
        <div class="history-original">${escapeHtml(item.originalText)}</div>
        <div class="history-translated">${escapeHtml(item.translatedText)}</div>
        <div class="history-time">${time}</div>
      </div>
    `;
  }).join('');
}

async function clearHistory() {
  if (!confirm('确定要清空所有翻译历史吗？')) return;
  await Storage.clearHistory();
  loadHistory();
}

function escapeHtml(text) {
  const div = document.createElement('div');
  div.textContent = text || '';
  return div.innerHTML;
}
