document.addEventListener('DOMContentLoaded', async () => {
  const config = await Storage.getConfig();

  document.getElementById('source-lang').value = config.sourceLanguage;
  document.getElementById('target-lang').value = config.targetLanguage;
  document.getElementById('toggle-text-translate').checked = config.enableTextTranslate;
  document.getElementById('toggle-image-translate').checked = config.enableImageTranslate;
  document.getElementById('toggle-video-subtitle-translate').checked = config.enableVideoSubtitleTranslate;
  document.getElementById('toggle-bilingual-subtitles').checked = config.showBilingualSubtitles;
  document.getElementById('toggle-gesture').checked = config.enableGesture;
  document.getElementById('toggle-context-menu').checked = config.enableContextMenu;

  checkApiStatus();

  document.getElementById('btn-translate-selection').addEventListener('click', async () => {
    const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
    chrome.tabs.sendMessage(tab.id, { type: 'translate-selection' });
    window.close();
  });

  document.getElementById('btn-translate-page').addEventListener('click', async () => {
    const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
    chrome.tabs.sendMessage(tab.id, { type: 'translate-page' });
    window.close();
  });

  document.getElementById('btn-summarize').addEventListener('click', async () => {
    const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
    chrome.tabs.sendMessage(tab.id, { type: 'summarize-page' });
    window.close();
  });

  document.getElementById('source-lang').addEventListener('change', async (e) => {
    const cfg = await Storage.getConfig();
    cfg.sourceLanguage = e.target.value;
    await Storage.saveConfig(cfg);
  });

  document.getElementById('target-lang').addEventListener('change', async (e) => {
    const cfg = await Storage.getConfig();
    cfg.targetLanguage = e.target.value;
    await Storage.saveConfig(cfg);
  });

  document.getElementById('toggle-text-translate').addEventListener('change', async (e) => {
    const cfg = await Storage.getConfig();
    cfg.enableTextTranslate = e.target.checked;
    await Storage.saveConfig(cfg);
  });

  document.getElementById('toggle-image-translate').addEventListener('change', async (e) => {
    const cfg = await Storage.getConfig();
    cfg.enableImageTranslate = e.target.checked;
    await Storage.saveConfig(cfg);
  });

  document.getElementById('toggle-video-subtitle-translate').addEventListener('change', async (e) => {
    const cfg = await Storage.getConfig();
    cfg.enableVideoSubtitleTranslate = e.target.checked;
    await Storage.saveConfig(cfg);
  });

  document.getElementById('toggle-bilingual-subtitles').addEventListener('change', async (e) => {
    const cfg = await Storage.getConfig();
    cfg.showBilingualSubtitles = e.target.checked;
    await Storage.saveConfig(cfg);
  });

  document.getElementById('toggle-gesture').addEventListener('change', async (e) => {
    const cfg = await Storage.getConfig();
    cfg.enableGesture = e.target.checked;
    await Storage.saveConfig(cfg);
    const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
    chrome.tabs.sendMessage(tab.id, {
      type: e.target.checked ? 'enable-gesture' : 'disable-gesture'
    });
  });

  document.getElementById('toggle-context-menu').addEventListener('change', async (e) => {
    const cfg = await Storage.getConfig();
    cfg.enableContextMenu = e.target.checked;
    await Storage.saveConfig(cfg);
    chrome.runtime.sendMessage({
      type: 'update-context-menu',
      enabled: e.target.checked
    });
  });

  document.getElementById('btn-settings').addEventListener('click', () => {
    chrome.runtime.openOptionsPage();
  });

  document.getElementById('btn-history').addEventListener('click', () => {
    chrome.tabs.create({ url: chrome.runtime.getURL('options/options.html#history') });
  });
});

async function checkApiStatus() {
  const translateDot = document.getElementById('translate-status');
  const ocrDot = document.getElementById('ocr-status');

  translateDot.className = 'status-dot';
  ocrDot.className = 'status-dot';

  chrome.runtime.sendMessage({ type: 'check-api-status' }, (response) => {
    if (response) {
      translateDot.className = `status-dot ${response.translate ? 'online' : 'offline'}`;
      ocrDot.className = `status-dot ${response.ocr ? 'online' : 'offline'}`;
    } else {
      translateDot.className = 'status-dot offline';
      ocrDot.className = 'status-dot offline';
    }
  });
}
