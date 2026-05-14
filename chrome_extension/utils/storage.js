const DEFAULT_CONFIG = {
  translateApiHost: 'http://127.0.0.1',
  translateApiPort: 8110,
  translateApiKey: '',
  translateModel: 'HY-MT1.5-1.8B-Q8_0',
  ocrApiHost: 'http://127.0.0.1',
  ocrApiPort: 8111,
  ocrApiKey: '',
  ocrModel: 'PaddleOCR-VL-1.5-GGUF',
  sourceLanguage: 'auto',
  targetLanguage: 'zh',
  apiTimeout: 180,
  enableTextTranslate: true,
  enableImageTranslate: true,
  enableGesture: true,
  enableContextMenu: true,
  gestureButton: 'right',
  gestureThreshold: 30,
  translatePrompt: '请将以下文本翻译为{target}，只返回翻译结果：\n\n{text}',
  ocrPrompt: '请识别图片中的所有文字，只输出识别到的文字内容：',
  summarizePrompt: '请用{target}总结以下网页内容，提取关键信息：\n\n{text}'
};

class Storage {
  static async getConfig() {
    return new Promise((resolve) => {
      chrome.storage.local.get('config', (result) => {
        resolve({ ...DEFAULT_CONFIG, ...(result.config || {}) });
      });
    });
  }

  static async saveConfig(config) {
    return new Promise((resolve) => {
      chrome.storage.local.set({ config: { ...DEFAULT_CONFIG, ...config } }, resolve);
    });
  }

  static async getHistory() {
    return new Promise((resolve) => {
      chrome.storage.local.get('history', (result) => {
        resolve(result.history || []);
      });
    });
  }

  static async addHistory(item) {
    const history = await this.getHistory();
    history.unshift({
      ...item,
      timestamp: Date.now()
    });
    if (history.length > 100) {
      history.length = 100;
    }
    return new Promise((resolve) => {
      chrome.storage.local.set({ history }, resolve);
    });
  }

  static async clearHistory() {
    return new Promise((resolve) => {
      chrome.storage.local.set({ history: [] }, resolve);
    });
  }
}

if (typeof window !== 'undefined') {
  window.Storage = Storage;
  window.DEFAULT_CONFIG = DEFAULT_CONFIG;
}
