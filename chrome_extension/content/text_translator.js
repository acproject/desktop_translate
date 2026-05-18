class TextTranslator {
  constructor() {
    this.translatingElements = new WeakMap();
    this.originalTexts = new WeakMap();
    this.originalPageTexts = new WeakMap();
    this.pageTranslationCache = new Map();
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

  _showResultPopup(rect, originalText, translatedText) {
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

    const scrollX = window.scrollX;
    const scrollY = window.scrollY;
    let left = rect.left + scrollX + rect.width / 2 - 180;
    let top = rect.bottom + scrollY + 8;

    if (left < 10) left = 10;
    if (left + 360 > window.innerWidth + scrollX) left = window.innerWidth + scrollX - 370;
    if (top + 200 > window.innerHeight + scrollY) {
      top = rect.top + scrollY - 208;
    }

    popup.style.left = left + 'px';
    popup.style.top = top + 'px';

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

  _escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML.replace(/\n/g, '<br>');
  }
}

window.TextTranslator = TextTranslator;
