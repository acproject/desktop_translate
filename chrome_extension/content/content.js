(function () {
  if (window.__dtInitialized) return;
  window.__dtInitialized = true;

  window.textTranslator = new TextTranslator();
  window.imageTranslator = new ImageTranslator();
  window.gestureHandler = new GestureHandler();

  window.textTranslator.init();
  window.imageTranslator.init();
  window.gestureHandler.init();

  chrome.runtime.onMessage.addListener((request, _sender, sendResponse) => {
    switch (request.type) {
      case 'translate-text':
        window.textTranslator.translateSelection().then(() => sendResponse({ success: true }));
        return true;

      case 'translate-selection':
        window.textTranslator.translateSelection().then(() => sendResponse({ success: true }));
        return true;

      case 'translate-page':
        window.textTranslator.translatePage().then(() => sendResponse({ success: true }));
        return true;

      case 'translate-image':
        if (request.srcUrl) {
          window.imageTranslator.translateImageBySrc(request.srcUrl).then(() => sendResponse({ success: true }));
        }
        return true;

      case 'summarize-page':
        window.gestureHandler._summarizePage();
        sendResponse({ success: true });
        return false;

      case 'summarize-progress':
        if (window.gestureHandler) {
          window.gestureHandler._updateSummaryProgress(request.message);
        }
        sendResponse({ success: true });
        return false;

      case 'ping':
        sendResponse({ success: true, version: '1.0.0' });
        return false;
    }
  });

  document.addEventListener('dblclick', async () => {
    const config = await new Promise((resolve) => {
      chrome.storage.local.get('config', (result) => {
        resolve({ ...DEFAULT_CONFIG, ...(result.config || {}) });
      });
    });

    if (!config.enableTextTranslate) return;

    const selection = window.getSelection();
    if (selection && selection.toString().trim()) {
      window.textTranslator.translateSelection();
    }
  });
})();
