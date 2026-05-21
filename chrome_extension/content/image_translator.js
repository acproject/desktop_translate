class ImageTranslator {
  constructor() {
    this.overlay = null;
    this.hoveredImage = null;
    this.imageTranslateButton = null;
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
    this._showImageLoading(null);
    try {
      const imageDataUrl = await this._imageUrlToBase64(srcUrl);
      const result = await this._sendOcrTranslateRequest(imageDataUrl);
      this._hideImageLoading();

      if (result.success) {
        this._showImageResultPopup(null, result);
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
        this._showImageResultPopup(rect, result);
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
