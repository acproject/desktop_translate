importScripts(
  chrome.runtime.getURL('utils/storage.js'),
  chrome.runtime.getURL('utils/api.js')
);

chrome.runtime.onInstalled.addListener(async () => {
  const config = await Storage.getConfig();
  if (config.enableContextMenu) {
    createContextMenus();
  }
});

function createContextMenus() {
  chrome.contextMenus.removeAll(() => {
    chrome.contextMenus.create({
      id: 'translate-selection',
      title: '翻译选中的文字',
      contexts: ['selection']
    });

    chrome.contextMenus.create({
      id: 'translate-image',
      title: '翻译图片中的文字',
      contexts: ['image']
    });

    chrome.contextMenus.create({
      id: 'summarize-page',
      title: '总结页面内容',
      contexts: ['page']
    });

    chrome.contextMenus.create({
      id: 'translate-page',
      title: '翻译整个页面',
      contexts: ['page']
    });
  });
}

chrome.contextMenus.onClicked.addListener(async (info, tab) => {
  const config = await Storage.getConfig();

  switch (info.menuItemId) {
    case 'translate-selection':
      chrome.tabs.sendMessage(tab.id, {
        type: 'translate-text',
        text: info.selectionText
      });
      break;

    case 'translate-image':
      chrome.tabs.sendMessage(tab.id, {
        type: 'translate-image',
        srcUrl: info.srcUrl
      });
      break;

    case 'summarize-page':
      chrome.tabs.sendMessage(tab.id, {
        type: 'summarize-page'
      });
      break;

    case 'translate-page':
      chrome.tabs.sendMessage(tab.id, {
        type: 'translate-page'
      });
      break;
  }
});

chrome.commands.onCommand.addListener(async (command) => {
  const [tab] = await chrome.tabs.query({ active: true, currentWindow: true });
  if (!tab) return;

  switch (command) {
    case 'translate-selection':
      chrome.tabs.sendMessage(tab.id, { type: 'translate-selection' });
      break;
    case 'translate-page':
      chrome.tabs.sendMessage(tab.id, { type: 'translate-page' });
      break;
    case 'summarize-page':
      chrome.tabs.sendMessage(tab.id, { type: 'summarize-page' });
      break;
  }
});

chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
  if (request.type === 'translate') {
    handleTranslate(request.text).then(sendResponse);
    return true;
  }

  if (request.type === 'ocr-translate') {
    handleOcrTranslate(request.imageDataUrl).then(sendResponse);
    return true;
  }

  if (request.type === 'summarize') {
    handleSummarize(request.text).then(sendResponse);
    return true;
  }

  if (request.type === 'check-api-status') {
    handleCheckApiStatus().then(sendResponse);
    return true;
  }

  if (request.type === 'update-context-menu') {
    if (request.enabled) {
      createContextMenus();
    } else {
      chrome.contextMenus.removeAll();
    }
    sendResponse({ success: true });
    return false;
  }
});

async function handleTranslate(text) {
  const config = await Storage.getConfig();
  const result = await TranslateAPI.translate(text, config);
  if (result.success) {
    await Storage.addHistory({
      type: 'translate',
      originalText: result.originalText,
      translatedText: result.translatedText
    });
  }
  return result;
}

async function handleOcrTranslate(imageDataUrl) {
  const config = await Storage.getConfig();
  const result = await TranslateAPI.ocrAndTranslate(imageDataUrl, config);
  if (result.success) {
    await Storage.addHistory({
      type: 'ocr-translate',
      originalText: result.originalText,
      translatedText: result.translatedText
    });
  }
  return result;
}

async function handleSummarize(text) {
  const config = await Storage.getConfig();
  const result = await TranslateAPI.summarize(text, config);
  if (result.success) {
    await Storage.addHistory({
      type: 'summarize',
      originalText: text.substring(0, 200),
      translatedText: result.summary
    });
  }
  return result;
}

async function handleCheckApiStatus() {
  const config = await Storage.getConfig();
  return await TranslateAPI.checkApiStatus(config);
}
