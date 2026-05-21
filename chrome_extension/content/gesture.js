class GestureHandler {
  constructor() {
    this.isEnabled = false;
    this.isDrawing = false;
    this.points = [];
    this.canvas = null;
    this.ctx = null;
    this.startX = 0;
    this.startY = 0;
    this.gestureButton = 2;
    this.threshold = 30;
  }

  async init() {
    const config = await this._getConfig();
    this.isEnabled = config.enableGesture;
    this.gestureButton = config.gestureButton === 'middle' ? 1 : 2;
    this.threshold = config.gestureThreshold || 30;

    if (this.isEnabled) {
      this._bindEvents();
    }
  }

  enable() {
    this.isEnabled = true;
    this._bindEvents();
  }

  disable() {
    this.isEnabled = false;
    this._unbindEvents();
  }

  _bindEvents() {
    if (this._bound) return;
    this._bound = true;

    this._onMouseDown = (e) => this._handleMouseDown(e);
    this._onMouseMove = (e) => this._handleMouseMove(e);
    this._onMouseUp = (e) => this._handleMouseUp(e);
    this._onContextMenu = (e) => this._handleContextMenu(e);

    document.addEventListener('mousedown', this._onMouseDown, true);
    document.addEventListener('mousemove', this._onMouseMove, true);
    document.addEventListener('mouseup', this._onMouseUp, true);
    document.addEventListener('contextmenu', this._onContextMenu, true);
  }

  _unbindEvents() {
    if (!this._bound) return;
    this._bound = false;

    document.removeEventListener('mousedown', this._onMouseDown, true);
    document.removeEventListener('mousemove', this._onMouseMove, true);
    document.removeEventListener('mouseup', this._onMouseUp, true);
    document.removeEventListener('contextmenu', this._onContextMenu, true);
  }

  _handleMouseDown(e) {
    if (!this.isEnabled) return;
    if (e.button !== this.gestureButton) return;

    this.isDrawing = true;
    this.startX = e.clientX;
    this.startY = e.clientY;
    this.points = [{ x: e.clientX, y: e.clientY }];

    this._createCanvas();
  }

  _handleMouseMove(e) {
    if (!this.isDrawing) return;

    this.points.push({ x: e.clientX, y: e.clientY });
    this._drawGesture();
  }

  _handleMouseUp(e) {
    if (!this.isDrawing) return;
    if (e.button !== this.gestureButton) return;

    this.isDrawing = false;
    this._removeCanvas();

    const gesture = this._recognizeGesture();
    if (gesture) {
      this._executeGesture(gesture);
    }

    this.points = [];
  }

  _handleContextMenu(e) {
    if (this.isDrawing || (this.isEnabled && this.points.length > 3)) {
      e.preventDefault();
    }
  }

  _createCanvas() {
    if (this.canvas) return;

    this.canvas = document.createElement('canvas');
    this.canvas.className = 'dt-gesture-canvas';
    this.canvas.width = window.innerWidth;
    this.canvas.height = window.innerHeight;
    this.canvas.style.cssText = `
      position: fixed;
      top: 0;
      left: 0;
      width: 100%;
      height: 100%;
      z-index: 2147483646;
      pointer-events: none;
    `;

    this.ctx = this.canvas.getContext('2d');
    document.body.appendChild(this.canvas);
  }

  _drawGesture() {
    if (!this.ctx || this.points.length < 2) return;

    this.ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

    this.ctx.beginPath();
    this.ctx.strokeStyle = 'rgba(74, 144, 217, 0.8)';
    this.ctx.lineWidth = 3;
    this.ctx.lineCap = 'round';
    this.ctx.lineJoin = 'round';

    const lastPoints = this.points.slice(-50);
    this.ctx.moveTo(lastPoints[0].x, lastPoints[0].y);
    for (let i = 1; i < lastPoints.length; i++) {
      this.ctx.lineTo(lastPoints[i].x, lastPoints[i].y);
    }
    this.ctx.stroke();

    this.ctx.beginPath();
    this.ctx.fillStyle = 'rgba(74, 144, 217, 1)';
    this.ctx.arc(this.startX, this.startY, 6, 0, Math.PI * 2);
    this.ctx.fill();

    this.ctx.font = '14px sans-serif';
    this.ctx.fillStyle = 'rgba(74, 144, 217, 0.9)';
    this.ctx.fillText('↑ 翻译选中 | ↓ 总结页面 | ← 翻译页面 | → OCR图片', this.startX + 12, this.startY - 12);
  }

  _removeCanvas() {
    if (this.canvas) {
      this.canvas.remove();
      this.canvas = null;
      this.ctx = null;
    }
  }

  _recognizeGesture() {
    if (this.points.length < 5) return null;

    const start = this.points[0];
    const end = this.points[this.points.length - 1];
    const dx = end.x - start.x;
    const dy = end.y - start.y;
    const distance = Math.sqrt(dx * dx + dy * dy);

    if (distance < this.threshold) return null;

    const angle = Math.atan2(dy, dx) * 180 / Math.PI;

    if (angle >= -45 && angle < 45) return 'right';
    if (angle >= 45 && angle < 135) return 'down';
    if (angle >= -135 && angle < -45) return 'up';
    if (angle >= 135 || angle < -135) return 'left';

    return null;
  }

  async _executeGesture(gesture) {
    switch (gesture) {
      case 'up':
        if (window.textTranslator) {
          await window.textTranslator.translateSelection();
        }
        break;
      case 'down':
        this._summarizePage();
        break;
      case 'left':
        if (window.textTranslator) {
          await window.textTranslator.translatePage();
        }
        break;
      case 'right':
        this._translateHoveredImage();
        break;
    }
  }

  _summarizePage() {
    const pageText = window.textTranslator?.getPageTextForSummary?.() || this._collectSummarizableText();
    if (!pageText) {
      this._showGestureTooltip('页面没有可总结的内容', 'warning');
      return;
    }

    this._showGestureLoading('正在准备页面摘要...');

    chrome.runtime.sendMessage({ type: 'summarize', text: pageText }, (response) => {
      this._hideGestureLoading();
      if (response && response.success) {
        this._showSummaryPopup(response.summary);
      } else {
        this._showGestureTooltip(response?.error || '总结失败', 'error');
      }
    });
  }

  _collectSummarizableText() {
    const maxLength = 20000;
    const walker = document.createTreeWalker(
      document.body,
      NodeFilter.SHOW_TEXT,
      {
        acceptNode: (node) => {
          if (!node || !node.parentElement) {
            return NodeFilter.FILTER_REJECT;
          }

          const parent = node.parentElement;
          const tagName = parent.tagName;
          if (['SCRIPT', 'STYLE', 'NOSCRIPT', 'TEXTAREA', 'INPUT', 'OPTION'].includes(tagName)) {
            return NodeFilter.FILTER_REJECT;
          }

          if (parent.closest('#dt-result-popup, #dt-page-result, #dt-page-loading, #dt-summary-popup, #dt-gesture-loading, .dt-popup, .dt-tooltip')) {
            return NodeFilter.FILTER_REJECT;
          }

          const text = (node.nodeValue || '').replace(/\s+/g, ' ').trim();
          if (text.length < 8) {
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
      const text = node.nodeValue.replace(/\s+/g, ' ').trim();
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

  _translateHoveredImage() {
    const images = document.querySelectorAll('img:hover');
    if (images.length > 0 && window.imageTranslator) {
      window.imageTranslator._translateImage(images[0]);
    } else {
      this._showGestureTooltip('请将鼠标悬停在图片上再使用此手势', 'warning');
    }
  }

  _showGestureLoading(message = '处理中...') {
    const existing = document.getElementById('dt-gesture-loading');
    if (existing) existing.remove();

    const loading = document.createElement('div');
    loading.id = 'dt-gesture-loading';
    loading.className = 'dt-loading';
    loading.style.cssText = `
      position: fixed;
      top: 50%;
      left: 50%;
      transform: translate(-50%, -50%);
    `;
    loading.innerHTML = `<div class="dt-spinner"></div><span>${this._escapeHtml(message)}</span>`;

    document.body.appendChild(loading);
  }

  _updateSummaryProgress(message) {
    this._updateGestureLoading(message || '处理中...');
  }

  _updateGestureLoading(message) {
    const loading = document.getElementById('dt-gesture-loading');
    const label = loading ? loading.querySelector('span') : null;
    if (label) {
      label.textContent = message;
    }
  }

  _hideGestureLoading() {
    const loading = document.getElementById('dt-gesture-loading');
    if (loading) loading.remove();
  }

  _showSummaryPopup(summaryText) {
    const existing = document.getElementById('dt-summary-popup');
    if (existing) existing.remove();

    const popup = document.createElement('div');
    popup.id = 'dt-summary-popup';
    popup.className = 'dt-popup';
    popup.innerHTML = `
      <div class="dt-popup-header">
        <span class="dt-popup-title">📄 页面摘要</span>
        <div class="dt-popup-actions">
          <button class="dt-btn dt-btn-icon" data-action="copy" title="复制">📋</button>
          <button class="dt-btn dt-btn-icon" data-action="close" title="关闭">✕</button>
        </div>
      </div>
      <div class="dt-popup-body">
        <div class="dt-translated-text">${this._escapeHtml(summaryText)}</div>
      </div>
    `;

    popup.style.left = '50%';
    popup.style.top = '50%';
    popup.style.transform = 'translate(-50%, -50%)';

    document.body.appendChild(popup);
    this._makePopupDraggable(popup);

    popup.querySelector('[data-action="copy"]').addEventListener('click', () => {
      navigator.clipboard.writeText(summaryText).then(() => {
        this._showGestureTooltip('已复制到剪贴板', 'success');
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

  _showGestureTooltip(message, type = 'info') {
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

  _getConfig() {
    return new Promise((resolve) => {
      chrome.storage.local.get('config', (result) => {
        resolve({ ...DEFAULT_CONFIG, ...(result.config || {}) });
      });
    });
  }

  _makePopupDraggable(popup) {
    const header = popup.querySelector('.dt-popup-header');
    if (!header) return;

    let startX = 0;
    let startY = 0;
    let initialLeft = 0;
    let initialTop = 0;
    let isDragging = false;

    const onMouseMove = (event) => {
      if (!isDragging) return;

      const nextLeft = initialLeft + (event.clientX - startX);
      const nextTop = initialTop + (event.clientY - startY);

      popup.style.left = `${Math.max(8, nextLeft)}px`;
      popup.style.top = `${Math.max(8, nextTop)}px`;
      popup.style.transform = 'none';
    };

    const onMouseUp = () => {
      if (!isDragging) return;
      isDragging = false;
      document.removeEventListener('mousemove', onMouseMove);
      document.removeEventListener('mouseup', onMouseUp);
      document.body.classList.remove('dt-dragging');
    };

    header.addEventListener('mousedown', (event) => {
      if (event.target.closest('.dt-popup-actions')) {
        return;
      }

      isDragging = true;
      startX = event.clientX;
      startY = event.clientY;

      const rect = popup.getBoundingClientRect();
      initialLeft = rect.left + window.scrollX;
      initialTop = rect.top + window.scrollY;

      popup.style.left = `${initialLeft}px`;
      popup.style.top = `${initialTop}px`;
      popup.style.transform = 'none';

      document.addEventListener('mousemove', onMouseMove);
      document.addEventListener('mouseup', onMouseUp);
      document.body.classList.add('dt-dragging');
      event.preventDefault();
    });
  }

  _escapeHtml(text) {
    const div = document.createElement('div');
    div.textContent = text;
    return div.innerHTML.replace(/\n/g, '<br>');
  }
}

window.GestureHandler = GestureHandler;
