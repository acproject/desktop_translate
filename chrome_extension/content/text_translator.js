class TextTranslator {
  constructor() {
    this.translatingElements = new WeakMap();
    this.originalTexts = new WeakMap();
    this.originalPageTexts = new WeakMap();
    this.pageTranslationCache = new Map();
    this.subtitleObserver = null;
    this.subtitleScanTimer = null;
    this.subtitleTranslationCache = new Map();
    this.pendingSubtitleTranslations = new Map();
    this.subtitleConfig = {
      enableVideoSubtitleTranslate: true,
      showBilingualSubtitles: true
    };
    this.hasSubtitleStorageListener = false;
  }

  async init() {
    const config = await this._getConfig();
    this._applySubtitleConfig(config);

    if (!this.hasSubtitleStorageListener && chrome?.storage?.onChanged) {
      chrome.storage.onChanged.addListener((changes, areaName) => {
        if (areaName !== 'local' || !changes.config) {
          return;
        }

        const nextConfig = { ...DEFAULT_CONFIG, ...(changes.config.newValue || {}) };
        this._applySubtitleConfig(nextConfig);
      });
      this.hasSubtitleStorageListener = true;
    }
  }

  async translateSelection() {
    const selection = window.getSelection();
    if (!selection || selection.isCollapsed || !selection.toString().trim()) {
      this._showTooltip('请先选中需要翻译的文字', 'warning');
      return;
    }

    const text = selection.toString().trim();
    const range = selection.getRangeAt(0);
    const rect = range.getBoundingClientRect();

    this._showLoading(rect);
    const result = await this._sendTranslateRequest(text);
    this._hideLoading();

    if (result.success) {
      this._showResultPopup(rect, result.originalText, result.translatedText);
    } else {
      this._showTooltip(result.error || '翻译失败', 'error');
    }
  }

  async translateText(text) {
    return await this._sendTranslateRequest(text);
  }

  async translatePage() {
    const textNodes = this._collectPageTextNodes();
    if (textNodes.length === 0) {
      this._showTooltip('页面没有可翻译的内容', 'warning');
      return;
    }

    this._showPageLoading();
    let translatedCount = 0;
    let successCount = 0;

    for (const node of textNodes) {
      const originalText = node.nodeValue;
      const normalizedText = originalText.trim();
      if (!normalizedText) {
        translatedCount += 1;
        continue;
      }

      this._updatePageLoading(`正在翻译页面内容... ${translatedCount + 1}/${textNodes.length}`);

      let result = this.pageTranslationCache.get(normalizedText);
      if (!result) {
        result = await this._sendTranslateRequest(normalizedText);
        this.pageTranslationCache.set(normalizedText, result);
      }

      if (result.success && result.translatedText.trim()) {
        if (!this.originalPageTexts.has(node)) {
          this.originalPageTexts.set(node, originalText);
        }
        node.nodeValue = originalText.replace(normalizedText, result.translatedText.trim());
        successCount += 1;
      }

      translatedCount += 1;
    }

    this._hidePageLoading();

    if (successCount > 0) {
      this._showTooltip(`已直接替换 ${successCount} 处页面文本`, 'success');
      return;
    }

    this._showTooltip('页面翻译失败', 'error');
  }

  async translateElement(element) {
    if (this.translatingElements.has(element)) return;

    const originalText = element.innerText;
    if (!originalText.trim()) return;

    this.originalTexts.set(element, originalText);
    this.translatingElements.set(element, true);

    const originalBg = element.style.backgroundColor;
    element.style.backgroundColor = 'rgba(74, 144, 217, 0.1)';

    const result = await this._sendTranslateRequest(originalText);

    if (result.success) {
      element.innerText = result.translatedText;
      element.setAttribute('data-translated', 'true');
      element.setAttribute('data-original', originalText);
      element.style.backgroundColor = 'rgba(74, 144, 217, 0.05)';
    } else {
      element.style.backgroundColor = originalBg;
      this._showTooltip(result.error || '翻译失败', 'error');
    }

    this.translatingElements.delete(element);
  }

  getPageTextForSummary(maxLength = 20000) {
    const walker = document.createTreeWalker(
      document.body,
      NodeFilter.SHOW_TEXT,
      {
        acceptNode: (node) => {
          if (!this._isSummaryCandidateNode(node)) {
            return NodeFilter.FILTER_REJECT;
          }

          const text = this._getTextNodeContentForSummary(node);
          if (!text || text.length < 8) {
            return NodeFilter.FILTER_REJECT;
          }

          return NodeFilter.FILTER_ACCEPT;
        }
      }
    );

    const segments = [];
    let totalLength = 0;
    let node = walker.nextNode();

    while (node && totalLength < maxLength) {
      const text = this._getTextNodeContentForSummary(node);
      if (text) {
        const remaining = maxLength - totalLength;
        const segment = text.slice(0, remaining);
        segments.push(segment);
        totalLength += segment.length + 1;
      }
      node = walker.nextNode();
    }

    return segments.join('\n').trim();
  }

  restoreElement(element) {
    if (element.getAttribute('data-translated') === 'true') {
      const original = element.getAttribute('data-original');
      if (original) {
        element.innerText = original;
        element.removeAttribute('data-translated');
        element.removeAttribute('data-original');
        element.style.backgroundColor = '';
      }
    }
  }

  _showResultPopup(_rect, originalText, translatedText) {
    const existing = document.getElementById('dt-result-popup');
    if (existing) existing.remove();

    const popup = document.createElement('div');
    popup.id = 'dt-result-popup';
    popup.className = 'dt-popup';
    popup.innerHTML = `
      <div class="dt-popup-header">
        <span class="dt-popup-title">翻译结果</span>
        <div class="dt-popup-actions">
          <button class="dt-btn dt-btn-icon" data-action="copy" title="复制">📋</button>
          <button class="dt-btn dt-btn-icon" data-action="close" title="关闭">✕</button>
        </div>
      </div>
      <div class="dt-popup-body">
        <div class="dt-original-text">${this._escapeHtml(originalText)}</div>
        <div class="dt-divider"></div>
        <div class="dt-translated-text">${this._escapeHtml(translatedText)}</div>
      </div>
    `;

    const centerLeft = window.scrollX + (window.innerWidth / 2);
    const centerTop = window.scrollY + (window.innerHeight / 2);
    popup.style.left = `${centerLeft}px`;
    popup.style.top = `${centerTop}px`;
    popup.style.transform = 'translate(-50%, -50%)';

    document.body.appendChild(popup);

    popup.querySelector('[data-action="copy"]').addEventListener('click', () => {
      navigator.clipboard.writeText(translatedText).then(() => {
        this._showTooltip('已复制到剪贴板', 'success');
      });
    });

    popup.querySelector('[data-action="close"]').addEventListener('click', () => {
      popup.remove();
    });

    const handleClickOutside = (e) => {
      if (!popup.contains(e.target)) {
        popup.remove();
        document.removeEventListener('mousedown', handleClickOutside);
      }
    };
    setTimeout(() => document.addEventListener('mousedown', handleClickOutside), 100);
  }

  _showPageResult(translatedText) {
    const existing = document.getElementById('dt-page-result');
    if (existing) existing.remove();

    const overlay = document.createElement('div');
    overlay.id = 'dt-page-result';
    overlay.className = 'dt-page-overlay';
    overlay.innerHTML = `
      <div class="dt-page-panel">
        <div class="dt-popup-header">
          <span class="dt-popup-title">页面翻译结果</span>
          <div class="dt-popup-actions">
            <button class="dt-btn dt-btn-icon" data-action="copy" title="复制全部">📋</button>
            <button class="dt-btn dt-btn-icon" data-action="close" title="关闭">✕</button>
          </div>
        </div>
        <div class="dt-page-body">${this._escapeHtml(translatedText)}</div>
      </div>
    `;

    document.body.appendChild(overlay);

    overlay.querySelector('[data-action="copy"]').addEventListener('click', () => {
      navigator.clipboard.writeText(translatedText).then(() => {
        this._showTooltip('已复制到剪贴板', 'success');
      });
    });

    overlay.querySelector('[data-action="close"]').addEventListener('click', () => {
      overlay.remove();
    });

    overlay.addEventListener('click', (e) => {
      if (e.target === overlay) overlay.remove();
    });
  }

  _showLoading(rect) {
    const existing = document.getElementById('dt-loading');
    if (existing) existing.remove();

    const loading = document.createElement('div');
    loading.id = 'dt-loading';
    loading.className = 'dt-loading';
    loading.innerHTML = '<div class="dt-spinner"></div><span>翻译中...</span>';

    const scrollX = window.scrollX;
    const scrollY = window.scrollY;
    loading.style.left = (rect.left + scrollX + rect.width / 2 - 50) + 'px';
    loading.style.top = (rect.bottom + scrollY + 8) + 'px';

    document.body.appendChild(loading);
  }

  _hideLoading() {
    const loading = document.getElementById('dt-loading');
    if (loading) loading.remove();
  }

  _showPageLoading() {
    const existing = document.getElementById('dt-page-loading');
    if (existing) existing.remove();

    const overlay = document.createElement('div');
    overlay.id = 'dt-page-loading';
    overlay.className = 'dt-page-overlay';
    overlay.innerHTML = `
      <div class="dt-page-panel dt-loading-panel">
        <div class="dt-spinner-large"></div>
        <span>正在翻译页面内容...</span>
      </div>
    `;

    document.body.appendChild(overlay);
  }

  _updatePageLoading(message) {
    const overlay = document.getElementById('dt-page-loading');
    const label = overlay ? overlay.querySelector('span') : null;
    if (label) {
      label.textContent = message;
    }
  }

  _hidePageLoading() {
    const overlay = document.getElementById('dt-page-loading');
    if (overlay) overlay.remove();
  }

  _showTooltip(message, type = 'info') {
    const existing = document.querySelector('.dt-tooltip');
    if (existing) existing.remove();

    const tooltip = document.createElement('div');
    tooltip.className = `dt-tooltip dt-tooltip-${type}`;
    tooltip.textContent = message;

    document.body.appendChild(tooltip);

    setTimeout(() => {
      tooltip.classList.add('dt-tooltip-fade-out');
      setTimeout(() => tooltip.remove(), 300);
    }, 2000);
  }

  _sendTranslateRequest(text) {
    return new Promise((resolve) => {
      chrome.runtime.sendMessage({ type: 'translate', text }, (response) => {
        resolve(response || { success: false, error: '无法连接到翻译服务' });
      });
    });
  }

  _initSubtitleTranslation() {
    if (!this._isSupportedSubtitleHost() || this.subtitleObserver) {
      return;
    }

    this.subtitleObserver = new MutationObserver(() => {
      this._scheduleSubtitleScan();
    });

    this.subtitleObserver.observe(document.body, {
      childList: true,
      subtree: true,
      characterData: true
    });

    this._scheduleSubtitleScan();
  }

  _scheduleSubtitleScan() {
    if (this.subtitleScanTimer) {
      clearTimeout(this.subtitleScanTimer);
    }

    this.subtitleScanTimer = setTimeout(() => {
      this.subtitleScanTimer = null;
      this._scanAndTranslateSubtitles();
    }, 120);
  }

  async _scanAndTranslateSubtitles() {
    if (!this.subtitleConfig.enableVideoSubtitleTranslate) {
      this._restoreTranslatedSubtitles();
      return;
    }

    const provider = this._getActiveSubtitleProvider();
    if (!provider) {
      this._restoreTranslatedSubtitles();
      return;
    }

    const subtitleElements = this._collectSubtitleElements(provider);
    for (const element of subtitleElements) {
      await this._translateSubtitleElement(element);
    }
  }

  _collectSubtitleElements(provider) {
    for (const selector of provider.selectors) {
      const elements = Array.from(document.querySelectorAll(selector))
        .filter((element) => this._isVisibleSubtitleElement(element));
      if (elements.length > 0) {
        return elements;
      }
    }

    return [];
  }

  _isVisibleSubtitleElement(element) {
    if (!element || !element.isConnected) {
      return false;
    }

    const rect = element.getBoundingClientRect();
    return rect.width > 0 && rect.height > 0;
  }

  async _translateSubtitleElement(element) {
    const originalText = this._getSubtitleSourceText(element);
    if (!originalText) {
      return;
    }

    const desiredMode = this.subtitleConfig.showBilingualSubtitles ? 'bilingual' : 'translated';
    if (
      element.dataset.dtSubtitleOriginalText === originalText &&
      element.dataset.dtSubtitleTranslatedText &&
      element.dataset.dtSubtitleRenderMode === desiredMode
    ) {
      return;
    }

    const cachedTranslation = this.subtitleTranslationCache.get(originalText);
    if (cachedTranslation) {
      this._applySubtitleTranslation(element, originalText, cachedTranslation);
      return;
    }

    let pendingRequest = this.pendingSubtitleTranslations.get(originalText);
    if (!pendingRequest) {
      pendingRequest = this._sendTranslateRequest(originalText)
        .finally(() => this.pendingSubtitleTranslations.delete(originalText));
      this.pendingSubtitleTranslations.set(originalText, pendingRequest);
    }

    const result = await pendingRequest;
    if (result?.success && result.translatedText?.trim()) {
      const translatedText = this._normalizeSubtitleText(result.translatedText);
      this.subtitleTranslationCache.set(originalText, translatedText);
      this._applySubtitleTranslation(element, originalText, translatedText);
    }
  }

  _applySubtitleTranslation(element, originalText, translatedText) {
    if (!translatedText || !element.isConnected) {
      return;
    }

    element.dataset.dtSubtitleOriginalText = originalText;
    element.dataset.dtSubtitleTranslatedText = translatedText;
    element.dataset.dtSubtitleRenderMode = this.subtitleConfig.showBilingualSubtitles ? 'bilingual' : 'translated';
    element.classList.add('dt-subtitle-host');

    if (this.subtitleConfig.showBilingualSubtitles) {
      element.innerHTML = `
        <span class="dt-subtitle-bilingual">
          <span class="dt-subtitle-original">${this._escapeHtml(originalText)}</span>
          <span class="dt-subtitle-translation">${this._escapeHtml(translatedText)}</span>
        </span>
      `;
    } else {
      element.textContent = translatedText;
    }
  }

  _normalizeSubtitleText(text) {
    return (text || '').replace(/\s+/g, ' ').trim();
  }

  _getSubtitleSourceText(element) {
    const currentText = this._normalizeSubtitleText(element.textContent || '');
    const originalText = this._normalizeSubtitleText(element.dataset.dtSubtitleOriginalText || '');
    const translatedText = this._normalizeSubtitleText(element.dataset.dtSubtitleTranslatedText || '');

    if (!originalText) {
      return currentText;
    }

    const bilingualRenderedText = this._normalizeSubtitleText(`${originalText} ${translatedText}`);
    if (
      currentText === originalText ||
      currentText === translatedText ||
      currentText === bilingualRenderedText ||
      !!element.querySelector('.dt-subtitle-bilingual')
    ) {
      return originalText;
    }

    delete element.dataset.dtSubtitleOriginalText;
    delete element.dataset.dtSubtitleTranslatedText;
    delete element.dataset.dtSubtitleRenderMode;
    element.classList.remove('dt-subtitle-host');
    return currentText;
  }

  _applySubtitleConfig(config) {
    this.subtitleConfig.enableVideoSubtitleTranslate = config.enableVideoSubtitleTranslate !== false;
    this.subtitleConfig.showBilingualSubtitles = config.showBilingualSubtitles !== false;

    if (!this._isSupportedSubtitleHost()) {
      return;
    }

    if (this.subtitleConfig.enableVideoSubtitleTranslate) {
      this._initSubtitleTranslation();
      this._scheduleSubtitleScan();
    } else {
      this._teardownSubtitleTranslation();
      this._restoreTranslatedSubtitles();
    }
  }

  _teardownSubtitleTranslation() {
    if (this.subtitleObserver) {
      this.subtitleObserver.disconnect();
      this.subtitleObserver = null;
    }

    if (this.subtitleScanTimer) {
      clearTimeout(this.subtitleScanTimer);
      this.subtitleScanTimer = null;
    }
  }

  _restoreTranslatedSubtitles() {
    document.querySelectorAll('[data-dt-subtitle-original-text]').forEach((element) => {
      const originalText = element.dataset.dtSubtitleOriginalText || '';
      if (originalText) {
        element.textContent = originalText;
      }

      delete element.dataset.dtSubtitleOriginalText;
      delete element.dataset.dtSubtitleTranslatedText;
      delete element.dataset.dtSubtitleRenderMode;
      element.classList.remove('dt-subtitle-host');
    });
  }

  _getActiveSubtitleProvider() {
    if (this._isYouTubeVideoPage()) {
      return {
        name: 'youtube',
        selectors: [
          '.ytp-caption-window-container .caption-visual-line',
          '.ytp-caption-window-container .ytp-caption-segment'
        ]
      };
    }

    if (this._isBilibiliVideoPage()) {
      return {
        name: 'bilibili',
        selectors: [
          '.bpx-player-subtitle-panel-text',
          '.bilibili-player-video-subtitle-content .bilibili-player-video-subtitle-item-text',
          '.bilibili-player-video-subtitle-content .subtitle-item-text'
        ]
      };
    }

    return null;
  }

  _isSupportedSubtitleHost() {
    return this._isYouTubeSite() || this._isBilibiliSite();
  }

  _isYouTubeSite() {
    const host = window.location.hostname;
    return host === 'www.youtube.com' || host === 'youtube.com' || host.endsWith('.youtube.com');
  }

  _isYouTubeVideoPage() {
    if (!this._isYouTubeSite()) {
      return false;
    }

    const path = window.location.pathname;
    return path === '/watch' || path.startsWith('/shorts/') || path.startsWith('/live/');
  }

  _isBilibiliSite() {
    const host = window.location.hostname;
    return host === 'www.bilibili.com' || host === 'bilibili.com' || host.endsWith('.bilibili.com');
  }

  _isBilibiliVideoPage() {
    if (!this._isBilibiliSite()) {
      return false;
    }

    const path = window.location.pathname;
    return path.startsWith('/video/') || path.startsWith('/bangumi/play/') || path.startsWith('/medialist/play/');
  }

  _getConfig() {
    if (typeof Storage !== 'undefined' && typeof Storage.getConfig === 'function') {
      return Storage.getConfig();
    }

    return new Promise((resolve) => {
      chrome.storage.local.get('config', (result) => {
        resolve({ ...DEFAULT_CONFIG, ...(result.config || {}) });
      });
    });
  }

  _collectPageTextNodes() {
    const walker = document.createTreeWalker(
      document.body,
      NodeFilter.SHOW_TEXT,
      {
        acceptNode: (node) => {
          if (!node || !node.parentElement) {
            return NodeFilter.FILTER_REJECT;
          }

          if (this.originalPageTexts.has(node)) {
            return NodeFilter.FILTER_REJECT;
          }

          const parent = node.parentElement;
          const tagName = parent.tagName;
          if (['SCRIPT', 'STYLE', 'NOSCRIPT', 'TEXTAREA', 'INPUT', 'OPTION'].includes(tagName)) {
            return NodeFilter.FILTER_REJECT;
          }

          if (parent.closest('[contenteditable="true"], #dt-result-popup, #dt-page-result, #dt-page-loading, .dt-popup, .dt-tooltip')) {
            return NodeFilter.FILTER_REJECT;
          }

          const text = node.nodeValue;
          if (!text || !text.trim()) {
            return NodeFilter.FILTER_REJECT;
          }

          if (text.trim().length < 2) {
            return NodeFilter.FILTER_REJECT;
          }

          return NodeFilter.FILTER_ACCEPT;
        }
      }
    );

    const nodes = [];
    let currentNode = walker.nextNode();
    while (currentNode) {
      nodes.push(currentNode);
      currentNode = walker.nextNode();
    }

    return nodes;
  }

  _getTextNodeContentForSummary(node) {
    const sourceText = this.originalPageTexts.get(node) || node.nodeValue || '';
    return sourceText.replace(/\s+/g, ' ').trim();
  }

  _isSummaryCandidateNode(node) {
    if (!node || !node.parentElement) {
      return false;
    }

    const parent = node.parentElement;
    const tagName = parent.tagName;
    if (['SCRIPT', 'STYLE', 'NOSCRIPT', 'TEXTAREA', 'INPUT', 'OPTION'].includes(tagName)) {
      return false;
    }

    if (parent.closest('[contenteditable="true"], #dt-result-popup, #dt-page-result, #dt-page-loading, #dt-summary-popup, #dt-gesture-loading, .dt-popup, .dt-tooltip')) {
      return false;
    }

    return true;
  }

  _escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML.replace(/\n/g, '<br>');
  }
}

window.TextTranslator = TextTranslator;
