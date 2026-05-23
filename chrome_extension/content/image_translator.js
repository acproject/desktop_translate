class ImageTranslator {
  constructor() {
    this.overlay = null;
    this.hoveredImage = null;
    this.imageTranslateButton = null;
    this.activeImageOverlays = new Map();
  }

  init() {
    this._createTranslateButton();
    document.addEventListener('mouseover', (e) => this._onMouseOver(e));
    document.addEventListener('mouseout', (e) => this._onMouseOut(e));
  }

  _createTranslateButton() {
    this.imageTranslateButton = document.createElement('div');
    this.imageTranslateButton.className = 'dt-image-translate-btn';
    this.imageTranslateButton.innerHTML = '🌐 翻译图片';
    this.imageTranslateButton.addEventListener('click', (e) => {
      e.preventDefault();
      e.stopPropagation();
      if (this.hoveredImage) {
        this._translateImage(this.hoveredImage);
      }
    });
    this.imageTranslateButton.style.display = 'none';
    document.body.appendChild(this.imageTranslateButton);
  }

  _onMouseOver(e) {
    const img = e.target;
    if (img.tagName !== 'IMG') return;
    if (img.width < 50 || img.height < 30) return;

    this.hoveredImage = img;
    const rect = img.getBoundingClientRect();
    this.imageTranslateButton.style.display = 'block';
    this.imageTranslateButton.style.left = (rect.left + window.scrollX) + 'px';
    this.imageTranslateButton.style.top = (rect.top + window.scrollY) + 'px';
  }

  _onMouseOut(e) {
    const img = e.target;
    if (img.tagName !== 'IMG') return;

    if (e.relatedTarget === this.imageTranslateButton) return;

    this.hoveredImage = null;
    this.imageTranslateButton.style.display = 'none';
  }

  async translateImageBySrc(srcUrl) {
    const imgElement = this._findImageBySrc(srcUrl);
    if (imgElement) {
      await this._translateImage(imgElement);
      return;
    }

    this._showImageLoading(null);
    try {
      const imageDataUrl = await this._imageUrlToBase64(srcUrl);
      const result = await this._sendOcrTranslateRequest(imageDataUrl);
      this._hideImageLoading();

      if (result.success) {
        this._showImageTooltip('未找到页面中的图片元素，无法按坐标覆盖显示', 'warning');
      } else {
        this._showImageTooltip(result.error || '图片翻译失败', 'error');
      }
    } catch (error) {
      this._hideImageLoading();
      this._showImageTooltip('无法获取图片数据: ' + error.message, 'error');
    }
  }

  async _translateImage(imgElement) {
    this.imageTranslateButton.style.display = 'none';
    const rect = imgElement.getBoundingClientRect();
    this._showImageLoading(rect);

    try {
      const imageDataUrl = await this._imageElementToBase64(imgElement);
      const result = await this._sendOcrTranslateRequest(imageDataUrl);
      this._hideImageLoading();

      if (result.success) {
        this._showImageTranslationOverlay(imgElement, result);
      } else {
        this._showImageTooltip(result.error || '图片翻译失败', 'error');
      }
    } catch (error) {
      this._hideImageLoading();
      this._showImageTooltip('无法获取图片数据: ' + error.message, 'error');
    }
  }

  _imageElementToBase64(imgElement) {
    return new Promise((resolve, reject) => {
      try {
        const canvas = document.createElement('canvas');
        const ctx = canvas.getContext('2d');

        canvas.width = imgElement.naturalWidth || imgElement.width;
        canvas.height = imgElement.naturalHeight || imgElement.height;

        ctx.drawImage(imgElement, 0, 0);
        const dataUrl = canvas.toDataURL('image/png');
        resolve(dataUrl);
      } catch (error) {
        if (error.name === 'SecurityError') {
          this._fetchImageAsBase64(imgElement.src).then(resolve).catch(reject);
        } else {
          reject(error);
        }
      }
    });
  }

  async _imageUrlToBase64(url) {
    try {
      const response = await fetch(url);
      const blob = await response.blob();
      return new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onloadend = () => resolve(reader.result);
        reader.onerror = reject;
        reader.readAsDataURL(blob);
      });
    } catch (e) {
      return await this._fetchImageAsBase64(url);
    }
  }

  _fetchImageAsBase64(url) {
    return new Promise((resolve, reject) => {
      const img = new Image();
      img.crossOrigin = 'anonymous';
      img.onload = () => {
        const canvas = document.createElement('canvas');
        const ctx = canvas.getContext('2d');
        canvas.width = img.naturalWidth;
        canvas.height = img.naturalHeight;
        ctx.drawImage(img, 0, 0);
        resolve(canvas.toDataURL('image/png'));
      };
      img.onerror = () => reject(new Error('无法加载图片'));
      img.src = url;
    });
  }

  _showImageLoading(rect) {
    const existing = document.getElementById('dt-image-loading');
    if (existing) existing.remove();

    const loading = document.createElement('div');
    loading.id = 'dt-image-loading';
    loading.className = 'dt-loading';
    loading.innerHTML = '<div class="dt-spinner"></div><span>OCR识别中...</span>';

    if (rect) {
      loading.style.left = (rect.left + window.scrollX + rect.width / 2 - 50) + 'px';
      loading.style.top = (rect.bottom + window.scrollY + 8) + 'px';
    } else {
      loading.style.left = '50%';
      loading.style.top = '50%';
      loading.style.transform = 'translate(-50%, -50%)';
    }

    document.body.appendChild(loading);
  }

  _hideImageLoading() {
    const loading = document.getElementById('dt-image-loading');
    if (loading) loading.remove();
  }

  _showImageTranslationOverlay(imgElement, result) {
    this._removeImageOverlay(imgElement);

    const textBlocks = (Array.isArray(result.textBlocks) ? result.textBlocks : [])
      .filter(block => block?.bbox && (block.translatedText || '').trim());

    if (textBlocks.length === 0) {
      this._showImageTooltip('OCR未返回文字坐标，无法直接覆盖到图片文字位置', 'warning');
      return;
    }

    const overlay = document.createElement('div');
    overlay.className = 'dt-image-translation-overlay';
    overlay.dataset.dtImageOverlay = 'true';

    const textLayer = document.createElement('div');
    textLayer.className = 'dt-image-translation-layer';
    overlay.appendChild(textLayer);

    const toolbar = document.createElement('div');
    toolbar.className = 'dt-image-translation-toolbar';
    toolbar.innerHTML = `
      <button class="dt-image-overlay-action" data-action="copy" title="复制翻译">📋</button>
      <button class="dt-image-overlay-action" data-action="close" title="关闭">✕</button>
    `;
    overlay.appendChild(toolbar);

    document.body.appendChild(overlay);

    const updatePosition = () => {
      if (!document.body.contains(overlay) || !document.body.contains(imgElement)) {
        this._removeImageOverlay(imgElement);
        return;
      }

      const rect = imgElement.getBoundingClientRect();
      overlay.style.left = `${rect.left + window.scrollX}px`;
      overlay.style.top = `${rect.top + window.scrollY}px`;
      overlay.style.width = `${rect.width}px`;
      overlay.style.height = `${rect.height}px`;

      this._renderImageTextBlocks(textLayer, textBlocks, imgElement, rect);
    };

    const scheduleUpdate = () => requestAnimationFrame(updatePosition);
    const entry = { overlay, updatePosition: scheduleUpdate };
    this.activeImageOverlays.set(imgElement, entry);

    toolbar.querySelector('[data-action="copy"]').addEventListener('click', () => {
      navigator.clipboard.writeText(result.translatedText || '').then(() => {
        this._showImageTooltip('已复制到剪贴板', 'success');
      });
    });

    toolbar.querySelector('[data-action="close"]').addEventListener('click', () => {
      this._removeImageOverlay(imgElement);
    });

    window.addEventListener('scroll', scheduleUpdate, true);
    window.addEventListener('resize', scheduleUpdate);
    entry.cleanup = () => {
      window.removeEventListener('scroll', scheduleUpdate, true);
      window.removeEventListener('resize', scheduleUpdate);
    };

    updatePosition();
  }

  _renderImageTextBlocks(textLayer, textBlocks, imgElement, rect) {
    textLayer.replaceChildren();

    const naturalWidth = imgElement.naturalWidth || rect.width;
    const naturalHeight = imgElement.naturalHeight || rect.height;
    const scaleX = rect.width / naturalWidth;
    const scaleY = rect.height / naturalHeight;

    textBlocks.forEach((block) => {
      const translatedText = (block.translatedText || '').trim();
      if (!translatedText) return;

      const blockEl = document.createElement('div');
      blockEl.className = 'dt-image-translated-block';
      blockEl.textContent = translatedText;
      blockEl.title = block.text || '';

      const left = Math.max(0, block.bbox.x * scaleX);
      const top = Math.max(0, block.bbox.y * scaleY);
      if (left >= rect.width || top >= rect.height) return;

      const width = Math.max(12, Math.min(block.bbox.width * scaleX, rect.width - left));
      const height = Math.max(10, Math.min(block.bbox.height * scaleY, rect.height - top));
      const textStyle = this._fitTextStyle(translatedText, width, height);

      blockEl.style.left = `${left}px`;
      blockEl.style.top = `${top}px`;
      blockEl.style.width = `${width}px`;
      blockEl.style.height = `${height}px`;
      blockEl.style.fontSize = `${textStyle.fontSize}px`;
      blockEl.style.lineHeight = String(textStyle.lineHeight);

      textLayer.appendChild(blockEl);
    });
  }

  _fitTextStyle(text, width, height) {
    const paddingX = width >= 28 ? 8 : 4;
    const paddingY = height >= 18 ? 4 : 2;
    const availableWidth = Math.max(4, width - paddingX);
    const availableHeight = Math.max(4, height - paddingY);
    const lineHeight = 1.12;
    const maxFontSize = Math.max(8, Math.min(34, Math.floor(availableHeight * 0.95), Math.floor(width * 0.45)));
    const minFontSize = 6;

    for (let fontSize = maxFontSize; fontSize >= minFontSize; fontSize -= 1) {
      const metrics = this._measureWrappedText(text, fontSize, availableWidth, lineHeight);
      if (metrics.width <= availableWidth && metrics.height <= availableHeight) {
        return { fontSize, lineHeight };
      }
    }

    return { fontSize: minFontSize, lineHeight };
  }

  _measureWrappedText(text, fontSize, maxWidth, lineHeight) {
    if (!this.measureCanvas) {
      this.measureCanvas = document.createElement('canvas');
    }

    const ctx = this.measureCanvas.getContext('2d');
    ctx.font = `650 ${fontSize}px -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif`;

    const lines = this._wrapTextForWidth(ctx, text, maxWidth);
    const widestLine = lines.reduce((maxWidthSoFar, line) => {
      return Math.max(maxWidthSoFar, ctx.measureText(line).width);
    }, 0);

    return {
      width: widestLine,
      height: lines.length * fontSize * lineHeight
    };
  }

  _wrapTextForWidth(ctx, text, maxWidth) {
    const tokens = this._tokenizeTextForWrap(text);
    const lines = [];
    let currentLine = '';

    tokens.forEach((token) => {
      const candidate = currentLine ? `${currentLine}${token}` : token.trimStart();
      if (candidate && ctx.measureText(candidate).width <= maxWidth) {
        currentLine = candidate;
        return;
      }

      if (currentLine) {
        lines.push(currentLine);
        currentLine = '';
      }

      if (ctx.measureText(token).width <= maxWidth) {
        currentLine = token.trimStart();
        return;
      }

      for (const char of token) {
        const charCandidate = currentLine + char;
        if (charCandidate && ctx.measureText(charCandidate).width <= maxWidth) {
          currentLine = charCandidate;
        } else {
          if (currentLine) {
            lines.push(currentLine);
          }
          currentLine = char;
        }
      }
    });

    if (currentLine) {
      lines.push(currentLine);
    }

    return lines.length > 0 ? lines : [''];
  }

  _tokenizeTextForWrap(text) {
    const normalizedText = String(text || '').replace(/\s+/g, ' ').trim();
    return normalizedText.match(/[\u4e00-\u9fff\u3040-\u30ff\uac00-\ud7af]|[^\s\u4e00-\u9fff\u3040-\u30ff\uac00-\ud7af]+|\s+/g) || [];
  }

  _removeImageOverlay(imgElement) {
    const entry = this.activeImageOverlays.get(imgElement);
    if (!entry) return;

    if (entry.cleanup) {
      entry.cleanup();
    }
    entry.overlay.remove();
    this.activeImageOverlays.delete(imgElement);
  }

  _findImageBySrc(srcUrl) {
    if (!srcUrl) return null;

    const normalize = (url) => {
      try {
        return new URL(url, window.location.href).href;
      } catch (e) {
        return url;
      }
    };

    const targetUrl = normalize(srcUrl);
    return Array.from(document.images).find((img) => {
      return normalize(img.currentSrc || img.src) === targetUrl || normalize(img.src) === targetUrl;
    }) || null;
  }

  _showImageResultPopup(_rect, result) {
    const existing = document.getElementById('dt-image-result');
    if (existing) existing.remove();

    const popup = document.createElement('div');
    popup.id = 'dt-image-result';
    popup.className = 'dt-popup';
    popup.innerHTML = `
      <div class="dt-popup-header">
        <span class="dt-popup-title">图片翻译结果</span>
        <div class="dt-popup-actions">
          <button class="dt-btn dt-btn-icon" data-action="copy" title="复制翻译">📋</button>
          <button class="dt-btn dt-btn-icon" data-action="close" title="关闭">✕</button>
        </div>
      </div>
      <div class="dt-popup-body">
        <div class="dt-section-label">识别文字</div>
        <div class="dt-original-text">${this._escapeHtml(result.originalText)}</div>
        <div class="dt-divider"></div>
        <div class="dt-section-label">翻译结果</div>
        <div class="dt-translated-text">${this._escapeHtml(result.translatedText)}</div>
      </div>
    `;

    const centerLeft = window.scrollX + (window.innerWidth / 2);
    const centerTop = window.scrollY + (window.innerHeight / 2);
    popup.style.left = `${centerLeft}px`;
    popup.style.top = `${centerTop}px`;
    popup.style.transform = 'translate(-50%, -50%)';

    document.body.appendChild(popup);
    this._makePopupDraggable(popup);

    popup.querySelector('[data-action="copy"]').addEventListener('click', () => {
      navigator.clipboard.writeText(result.translatedText).then(() => {
        this._showImageTooltip('已复制到剪贴板', 'success');
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

  _showImageTooltip(message, type = 'info') {
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

  _sendOcrTranslateRequest(imageDataUrl) {
    return new Promise((resolve) => {
      chrome.runtime.sendMessage({ type: 'ocr-translate', imageDataUrl }, (response) => {
        resolve(response || { success: false, error: '无法连接到OCR服务' });
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

window.ImageTranslator = ImageTranslator;
