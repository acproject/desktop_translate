class TranslateAPI {
  static _buildUrl(host, port, path) {
    let cleanHost = host.replace(/\/+$/, '');
    return `${cleanHost}:${port}${path}`;
  }

  static async translate(text, config) {
    const url = this._buildUrl(config.translateApiHost, config.translateApiPort, '/v1/chat/completions');
    const prompt = config.translatePrompt
      .replace('{target}', config.targetLanguage)
      .replace('{text}', text);

    const body = {
      model: config.translateModel,
      messages: [
        { role: 'user', content: prompt }
      ],
      temperature: 0.3,
      max_tokens: 4096
    };

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), config.apiTimeout * 1000);

    try {
      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          ...(config.translateApiKey ? { 'Authorization': `Bearer ${config.translateApiKey}` } : {})
        },
        body: JSON.stringify(body),
        signal: controller.signal
      });

      clearTimeout(timeoutId);

      if (!response.ok) {
        throw new Error(`HTTP ${response.status}: ${response.statusText}`);
      }

      const data = await response.json();
      const translatedText = this._extractMessageText(data);
      return { success: true, originalText: text, translatedText };
    } catch (error) {
      clearTimeout(timeoutId);
      if (error.name === 'AbortError') {
        return { success: false, originalText: text, translatedText: '', error: '请求超时，请检查翻译服务是否正常运行' };
      }
      const errMsg = error.message || '';
      if (errMsg.includes('Failed to fetch') || errMsg.includes('NetworkError') || errMsg.includes('Network request failed')) {
        return {
          success: false,
          originalText: text,
          translatedText: '',
          error: `无法连接翻译服务 (${url})，请确认：1) 服务已启动  2) 端口 ${config.translateApiPort} 正确  3) 在扩展设置中检查API地址`
        };
      }
      return { success: false, originalText: text, translatedText: '', error: errMsg };
    }
  }

  static async ocrAndTranslate(imageDataUrl, config) {
    const ocrUrl = this._buildUrl(config.ocrApiHost, config.ocrApiPort, '/v1/chat/completions');
    const ocrPrompt = this._buildStructuredOcrPrompt(config.ocrPrompt);

    const ocrBody = {
      model: config.ocrModel,
      messages: [
        {
          role: 'user',
          content: [
            { type: 'text', text: ocrPrompt },
            { type: 'image_url', image_url: { url: imageDataUrl } }
          ]
        }
      ],
      temperature: 0.1,
      max_tokens: 4096
    };

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), config.apiTimeout * 1000);

    try {
      const ocrResponse = await fetch(ocrUrl, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          ...(config.ocrApiKey ? { 'Authorization': `Bearer ${config.ocrApiKey}` } : {})
        },
        body: JSON.stringify(ocrBody),
        signal: controller.signal
      });

      clearTimeout(timeoutId);

      if (!ocrResponse.ok) {
        throw new Error(`OCR HTTP ${ocrResponse.status}: ${ocrResponse.statusText}`);
      }

      const ocrData = await ocrResponse.json();
      const ocrText = this._extractMessageText(ocrData);
      const cleanedOcrText = this._removeThinkBlocks(ocrText);
      let textBlocks = this._parseOcrTextBlocks(cleanedOcrText);

      if (textBlocks.length === 0 && !cleanedOcrText.trim()) {
        return { success: false, originalText: '', translatedText: '', error: '未能识别到文字' };
      }

      if (textBlocks.length === 0 || textBlocks.every(block => !block.bbox)) {
        const tiledBlocks = await this._ocrImageTiles(imageDataUrl, ocrUrl, config);
        if (tiledBlocks.length > 0) {
          textBlocks = tiledBlocks;
        }
      }

      const blocksForTranslation = textBlocks.length > 0
        ? textBlocks
        : [{ id: 1, text: cleanedOcrText, bbox: null }];
      const translatedBlocks = await this._translateTextBlocks(blocksForTranslation, config);
      const originalText = blocksForTranslation.map(block => block.text).join('\n');
      const translatedText = translatedBlocks.map(block => block.translatedText || '').filter(Boolean).join('\n');

      return {
        success: translatedBlocks.some(block => block.translatedText),
        originalText,
        translatedText,
        textBlocks: translatedBlocks,
        error: translatedBlocks.some(block => block.translatedText) ? undefined : '翻译结果为空'
      };
    } catch (error) {
      clearTimeout(timeoutId);
      if (error.name === 'AbortError') {
        return { success: false, originalText: '', translatedText: '', error: 'OCR请求超时，请检查OCR服务是否正常运行' };
      }
      const errMsg = error.message || '';
      if (errMsg.includes('Failed to fetch') || errMsg.includes('NetworkError') || errMsg.includes('Network request failed')) {
        return {
          success: false,
          originalText: '',
          translatedText: '',
          error: `无法连接OCR服务 (${ocrUrl})，请确认：1) 服务已启动  2) 端口 ${config.ocrApiPort} 正确  3) 在扩展设置中检查API地址`
        };
      }
      return { success: false, originalText: '', translatedText: '', error: errMsg };
    }
  }

  static _buildStructuredOcrPrompt(userPrompt) {
    return [
      userPrompt || '请识别图片中的所有文字及其位置。',
      '',
      '输出要求：',
      '1. 只返回 JSON 数组，不要 Markdown，不要解释。',
      '2. 先定位图片中的文字块、段落、表格单元或气泡等区域，再识别每个区域内的文字。',
      '3. 每个元素格式为 {"text":"识别文字","bbox":[x,y,width,height]}。',
      '4. bbox 必须使用原图像素坐标，x/y 为文字框左上角，width/height 为文字框大小。',
      '5. 按自然阅读顺序输出；如果没有文字，返回 []。'
    ].join('\n');
  }

  static async _ocrImageTiles(imageDataUrl, ocrUrl, config) {
    try {
      const image = await this._loadImageBitmap(imageDataUrl);
      const tiles = this._buildImageTiles(image.width, image.height);
      const blocks = [];

      for (const tile of tiles) {
        const tileDataUrl = await this._cropImageBitmapToDataUrl(image, tile);
        const prompt = [
          '这是从原图中裁剪出来的一个局部文字块。',
          '请只识别这个局部图片内清晰可见的文字，按自然阅读顺序输出纯文本。',
          '不要描述图片，不要输出 Markdown，不要解释。如果没有可读文字，返回空字符串。'
        ].join('\n');
        const blockText = this._cleanOcrPlainText(await this._requestOcrChatText(ocrUrl, config, prompt, tileDataUrl, 1024));
        if (!blockText) {
          continue;
        }
        if (blocks.some(block => this._isDuplicateOcrText(block.text, blockText))) {
          continue;
        }

        blocks.push({
          id: blocks.length + 1,
          text: blockText,
          bbox: {
            x: tile.x,
            y: tile.y,
            width: tile.width,
            height: tile.height
          }
        });
      }

      if (image.close) {
        image.close();
      }
      return blocks;
    } catch (e) {
      return [];
    }
  }

  static _isDuplicateOcrText(existingText, nextText) {
    const normalize = (value) => String(value || '').replace(/\s+/g, '').trim();
    const a = normalize(existingText);
    const b = normalize(nextText);
    if (!a || !b) {
      return false;
    }

    return a === b || (a.length >= 8 && b.length >= 8 && (a.includes(b) || b.includes(a)));
  }

  static _buildImageTiles(width, height) {
    const minSize = 80;
    if (width < minSize || height < minSize) {
      return [{ x: 0, y: 0, width, height }];
    }

    const columns = width / height > 1.8 ? 2 : 1;
    const targetTileHeight = 360;
    const rows = Math.max(1, Math.min(8, Math.ceil(height / targetTileHeight)));
    const overlap = Math.min(32, Math.floor(Math.min(width, height) * 0.06));
    const tiles = [];

    for (let row = 0; row < rows; row += 1) {
      const rawY = Math.floor((height * row) / rows);
      const rawY2 = Math.floor((height * (row + 1)) / rows);
      const y = Math.max(0, rawY - (row > 0 ? overlap : 0));
      const y2 = Math.min(height, rawY2 + (row < rows - 1 ? overlap : 0));

      for (let column = 0; column < columns; column += 1) {
        const rawX = Math.floor((width * column) / columns);
        const rawX2 = Math.floor((width * (column + 1)) / columns);
        const x = Math.max(0, rawX - (column > 0 ? overlap : 0));
        const x2 = Math.min(width, rawX2 + (column < columns - 1 ? overlap : 0));

        tiles.push({
          x,
          y,
          width: Math.max(1, x2 - x),
          height: Math.max(1, y2 - y)
        });
      }
    }

    return tiles;
  }

  static async _loadImageBitmap(imageDataUrl) {
    if (typeof createImageBitmap !== 'function') {
      throw new Error('createImageBitmap is unavailable');
    }

    const response = await fetch(imageDataUrl);
    const blob = await response.blob();
    return await createImageBitmap(blob);
  }

  static async _cropImageBitmapToDataUrl(image, tile) {
    if (typeof OffscreenCanvas !== 'undefined') {
      const canvas = new OffscreenCanvas(tile.width, tile.height);
      const ctx = canvas.getContext('2d');
      ctx.drawImage(image, tile.x, tile.y, tile.width, tile.height, 0, 0, tile.width, tile.height);
      const blob = await canvas.convertToBlob({ type: 'image/png' });
      return await this._blobToDataUrl(blob);
    }

    if (typeof document !== 'undefined') {
      const canvas = document.createElement('canvas');
      canvas.width = tile.width;
      canvas.height = tile.height;
      const ctx = canvas.getContext('2d');
      ctx.drawImage(image, tile.x, tile.y, tile.width, tile.height, 0, 0, tile.width, tile.height);
      return canvas.toDataURL('image/png');
    }

    throw new Error('Canvas is unavailable');
  }

  static async _blobToDataUrl(blob) {
    if (typeof FileReader !== 'undefined') {
      return await new Promise((resolve, reject) => {
        const reader = new FileReader();
        reader.onloadend = () => resolve(reader.result);
        reader.onerror = reject;
        reader.readAsDataURL(blob);
      });
    }

    const buffer = await blob.arrayBuffer();
    const bytes = new Uint8Array(buffer);
    let binary = '';
    const chunkSize = 0x8000;
    for (let offset = 0; offset < bytes.length; offset += chunkSize) {
      binary += String.fromCharCode(...bytes.subarray(offset, offset + chunkSize));
    }
    return `data:${blob.type || 'image/png'};base64,${btoa(binary)}`;
  }

  static async _requestOcrChatText(url, config, prompt, imageDataUrl, maxTokens) {
    const body = {
      model: config.ocrModel,
      messages: [
        {
          role: 'user',
          content: [
            { type: 'text', text: prompt },
            { type: 'image_url', image_url: { url: imageDataUrl } }
          ]
        }
      ],
      temperature: 0.1,
      max_tokens: maxTokens
    };

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), config.apiTimeout * 1000);

    try {
      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          ...(config.ocrApiKey ? { 'Authorization': `Bearer ${config.ocrApiKey}` } : {})
        },
        body: JSON.stringify(body),
        signal: controller.signal
      });

      clearTimeout(timeoutId);

      if (!response.ok) {
        const errorMessage = await this._readErrorMessage(response);
        throw new Error(`OCR HTTP ${response.status}: ${errorMessage || response.statusText}`);
      }

      const data = await response.json();
      return this._extractMessageText(data).trim();
    } catch (error) {
      clearTimeout(timeoutId);
      throw error;
    }
  }

  static _cleanOcrPlainText(text) {
    return this._removeThinkBlocks(String(text || ''))
      .replace(/```(?:text)?/gi, '```')
      .replace(/^```|```$/g, '')
      .replace(/^(no text found|no readable text|none|null|无文字|没有文字|未识别到文字)$/i, '')
      .trim();
  }

  static async _translateTextBlocks(blocks, config) {
    const nonEmptyBlocks = blocks.filter(block => block.text && block.text.trim());
    if (nonEmptyBlocks.length === 0) {
      return [];
    }

    const url = this._buildUrl(config.translateApiHost, config.translateApiPort, '/v1/chat/completions');
    const items = nonEmptyBlocks.map((block, index) => ({
      id: block.id ?? index + 1,
      text: block.text
    }));
    const prompt = [
      `请将以下 JSON 数组中每个 text 翻译为 ${config.targetLanguage}。`,
      '只返回 JSON 数组，不要 Markdown，不要解释。每项格式为 {"id":原id,"translatedText":"译文"}。',
      JSON.stringify(items)
    ].join('\n\n');

    try {
      const responseText = await this._requestChatText(url, config, prompt, 4096);
      const translatedItems = this._parseJsonPayload(responseText);
      if (Array.isArray(translatedItems)) {
        const translatedById = new Map();
        translatedItems.forEach((item) => {
          if (item && item.id !== undefined && item.translatedText !== undefined) {
            translatedById.set(String(item.id), String(item.translatedText));
          }
        });

        if (translatedById.size > 0) {
          return nonEmptyBlocks.map(block => ({
            ...block,
            translatedText: translatedById.get(String(block.id)) || ''
          }));
        }
      }
    } catch (e) {
      // Fall back to individual requests below; some local models are poor at JSON mode.
    }

    const translatedBlocks = [];
    for (const block of nonEmptyBlocks) {
      const result = await this.translate(block.text, config);
      translatedBlocks.push({
        ...block,
        translatedText: result.success ? result.translatedText : ''
      });
    }
    return translatedBlocks;
  }

  static _parseOcrTextBlocks(text) {
    const payload = this._parseJsonPayload(text);
    const items = Array.isArray(payload)
      ? (this._looksLikeOcrArrayBlock(payload) ? [payload] : payload)
      : payload?.items || payload?.blocks || payload?.textBlocks || payload?.result || payload?.data || payload?.ocr || [];

    if (!Array.isArray(items)) {
      return [];
    }

    return items
      .map((item, index) => this._normalizeOcrBlock(item, index))
      .filter(block => block && block.text);
  }

  static _normalizeOcrBlock(item, index) {
    if (Array.isArray(item)) {
      return this._normalizeOcrArrayBlock(item, index);
    }

    if (!item || typeof item !== 'object') {
      return null;
    }

    const textValue = item.text || item.content || item.value || item.words || item.rec_text || item.label || item.description || '';
    const text = String(textValue).trim();
    if (!text) {
      return null;
    }

    const bbox = this._normalizeBoundingBox(
      item.bbox || item.box || item.boundingBox || item.bounding_box || item.rect || item.position ||
      item.coordinate || item.coordinates || item.coords || item.points || item.vertices || item.polygon ||
      item.poly || item.quad || item.dt_poly || item.dt_polys || item
    );
    return {
      id: item.id ?? index + 1,
      text,
      bbox
    };
  }

  static _normalizeOcrArrayBlock(item, index) {
    const textIndex = item.findIndex(value => typeof value === 'string' && value.trim());
    const boxIndex = item.findIndex(value => value !== item[textIndex] && (Array.isArray(value) || (value && typeof value === 'object')));
    const text = textIndex >= 0 ? String(item[textIndex]).trim() : '';
    if (!text) {
      return null;
    }

    return {
      id: index + 1,
      text,
      bbox: boxIndex >= 0 ? this._normalizeBoundingBox(item[boxIndex]) : null
    };
  }

  static _looksLikeOcrArrayBlock(item) {
    return item.some(value => typeof value === 'string' && value.trim()) &&
      item.some(value => Array.isArray(value) || (value && typeof value === 'object'));
  }

  static _normalizeBoundingBox(box) {
    if (Array.isArray(box)) {
      const numericValues = box.map(value => Number(value));
      if (box.length >= 4 && numericValues.every(value => Number.isFinite(value))) {
        const [a, b, c, d] = numericValues;
        return { x: a, y: b, width: Math.max(1, c), height: Math.max(1, d) };
      }

      const points = box
        .filter(point => Array.isArray(point) && point.length >= 2)
        .map(point => ({ x: Number(point[0]), y: Number(point[1]) }))
        .filter(point => Number.isFinite(point.x) && Number.isFinite(point.y));
      return this._boxFromPoints(points);
    }

    if (box && typeof box === 'object') {
      if (Number.isFinite(Number(box.x)) && Number.isFinite(Number(box.y))) {
        const x = Number(box.x);
        const y = Number(box.y);
        const width = Number(box.width ?? box.w ?? (box.x2 !== undefined ? Number(box.x2) - x : 0));
        const height = Number(box.height ?? box.h ?? (box.y2 !== undefined ? Number(box.y2) - y : 0));
        if (width > 0 && height > 0) {
          return { x, y, width, height };
        }
      }

      const points = box.points || box.vertices || box.polygon;
      if (Array.isArray(points)) {
        return this._boxFromPoints(points.map(point => ({
          x: Number(point.x ?? point[0]),
          y: Number(point.y ?? point[1])
        })));
      }
    }

    return null;
  }

  static _boxFromPoints(points) {
    const validPoints = points.filter(point => Number.isFinite(point.x) && Number.isFinite(point.y));
    if (validPoints.length === 0) {
      return null;
    }

    const xs = validPoints.map(point => point.x);
    const ys = validPoints.map(point => point.y);
    const x = Math.min(...xs);
    const y = Math.min(...ys);
    const width = Math.max(...xs) - x;
    const height = Math.max(...ys) - y;
    if (width <= 0 || height <= 0) {
      return null;
    }

    return { x, y, width, height };
  }

  static _parseJsonPayload(text) {
    const cleanedText = this._removeThinkBlocks(String(text || ''))
      .replace(/```(?:json)?/gi, '```')
      .trim();
    const candidates = [
      cleanedText,
      this._extractJsonCandidate(cleanedText, '[', ']'),
      this._extractJsonCandidate(cleanedText, '{', '}')
    ].filter(Boolean);

    for (const candidate of candidates) {
      try {
        return JSON.parse(candidate.replace(/^```|```$/g, '').trim());
      } catch (e) {
        // Try the next candidate.
      }
    }

    return null;
  }

  static _extractJsonCandidate(text, startChar, endChar) {
    const start = text.indexOf(startChar);
    const end = text.lastIndexOf(endChar);
    if (start === -1 || end === -1 || end <= start) {
      return '';
    }
    return text.slice(start, end + 1);
  }

  static async summarize(text, config, onProgress = null) {
    const url = this._buildUrl(config.translateApiHost, config.translateApiPort, '/v1/chat/completions');
    const preparedText = this._prepareSummaryText(text);
    const chunks = this._splitSummaryText(preparedText, 2200);

    if (chunks.length === 0) {
      return { success: false, summary: '', error: '没有可用于总结的页面内容' };
    }

    try {
      this._emitProgress(onProgress, {
        stage: 'split',
        current: 0,
        total: chunks.length,
        message: chunks.length === 1
          ? '正在总结页面内容...'
          : `已拆分为 ${chunks.length} 段，准备开始分段总结...`
      });

      if (chunks.length === 1) {
        this._emitProgress(onProgress, {
          stage: 'chunk',
          current: 1,
          total: 1,
          message: '正在总结第 1/1 段...'
        });
        const prompt = this._buildChunkSummaryPrompt(chunks[0], config, 1, 1);
        const summaryText = await this._requestChatText(url, config, prompt, 1024);
        return { success: true, summary: summaryText };
      }

      const partialSummaries = [];
      for (let index = 0; index < chunks.length; index += 1) {
        this._emitProgress(onProgress, {
          stage: 'chunk',
          current: index + 1,
          total: chunks.length,
          message: `正在总结第 ${index + 1}/${chunks.length} 段...`
        });
        const prompt = this._buildChunkSummaryPrompt(chunks[index], config, index + 1, chunks.length);
        const partialSummary = await this._requestChatText(url, config, prompt, 768);
        if (partialSummary.trim()) {
          partialSummaries.push(`第${index + 1}段摘要：\n${partialSummary.trim()}`);
        }
      }

      if (partialSummaries.length === 0) {
        return { success: false, summary: '', error: '分段总结未返回有效结果' };
      }

      this._emitProgress(onProgress, {
        stage: 'final',
        current: partialSummaries.length,
        total: chunks.length,
        message: '正在生成最终摘要...'
      });
      const finalPrompt = this._buildFinalSummaryPrompt(partialSummaries, config);
      const finalSummary = await this._requestChatText(url, config, finalPrompt, 1024);
      return { success: true, summary: finalSummary };
    } catch (error) {
      if (error.name === 'AbortError') {
        return { success: false, summary: '', error: '请求超时，请检查翻译服务是否正常运行' };
      }
      const errMsg = error.message || '';
      if (errMsg.includes('Failed to fetch') || errMsg.includes('NetworkError') || errMsg.includes('Network request failed')) {
        return {
          success: false,
          summary: '',
          error: `无法连接翻译服务 (${url})，请确认服务已启动且端口正确`
        };
      }
      return { success: false, summary: '', error: errMsg };
    }
  }

  static async checkApiStatus(config) {
    const results = {
      translate: false,
      ocr: false
    };

    try {
      const translateUrl = this._buildUrl(config.translateApiHost, config.translateApiPort, '/v1/models');
      const controller1 = new AbortController();
      const tid1 = setTimeout(() => controller1.abort(), 5000);
      const translateResp = await fetch(translateUrl, {
        method: 'GET',
        signal: controller1.signal
      });
      clearTimeout(tid1);
      results.translate = translateResp.ok;
    } catch (e) {
      results.translate = false;
    }

    try {
      const ocrUrl = this._buildUrl(config.ocrApiHost, config.ocrApiPort, '/v1/models');
      const controller2 = new AbortController();
      const tid2 = setTimeout(() => controller2.abort(), 5000);
      const ocrResp = await fetch(ocrUrl, {
        method: 'GET',
        signal: controller2.signal
      });
      clearTimeout(tid2);
      results.ocr = ocrResp.ok;
    } catch (e) {
      results.ocr = false;
    }

    return results;
  }

  static _extractMessageText(data) {
    try {
      if (data.choices && data.choices.length > 0) {
        const choice = data.choices[0];
        if (choice.message) {
          if (typeof choice.message.content === 'string') {
            return choice.message.content;
          }
          if (Array.isArray(choice.message.content)) {
            return choice.message.content
              .filter(item => item.type === 'text')
              .map(item => item.text)
              .join('');
          }
        }
      }
      return '';
    } catch (e) {
      return '';
    }
  }

  static _removeThinkBlocks(text) {
    return text.replace(/<think>[\s\S]*?<\/think>/g, '').trim();
  }

  static _prepareSummaryText(text) {
    return (text || '')
      .replace(/\r/g, '\n')
      .replace(/[ \t]+/g, ' ')
      .replace(/\n{3,}/g, '\n\n')
      .trim()
      .slice(0, 20000);
  }

  static _splitSummaryText(text, maxChunkLength) {
    if (!text) {
      return [];
    }

    const normalized = text.replace(/\n{3,}/g, '\n\n').trim();
    if (!normalized) {
      return [];
    }

    const paragraphs = normalized
      .split(/\n{2,}/)
      .map(paragraph => paragraph.trim())
      .filter(Boolean);

    const chunks = [];
    let currentChunk = '';

    for (const paragraph of paragraphs) {
      if (paragraph.length > maxChunkLength) {
        if (currentChunk) {
          chunks.push(currentChunk);
          currentChunk = '';
        }

        const sentences = paragraph.split(/(?<=[。！？.!?])\s+/);
        let sentenceChunk = '';

        for (const sentence of sentences) {
          const trimmedSentence = sentence.trim();
          if (!trimmedSentence) {
            continue;
          }

          const candidate = sentenceChunk ? `${sentenceChunk} ${trimmedSentence}` : trimmedSentence;
          if (candidate.length <= maxChunkLength) {
            sentenceChunk = candidate;
            continue;
          }

          if (sentenceChunk) {
            chunks.push(sentenceChunk);
          }

          if (trimmedSentence.length <= maxChunkLength) {
            sentenceChunk = trimmedSentence;
            continue;
          }

          for (let offset = 0; offset < trimmedSentence.length; offset += maxChunkLength) {
            chunks.push(trimmedSentence.slice(offset, offset + maxChunkLength));
          }
          sentenceChunk = '';
        }

        if (sentenceChunk) {
          chunks.push(sentenceChunk);
        }
        continue;
      }

      const candidate = currentChunk ? `${currentChunk}\n\n${paragraph}` : paragraph;
      if (candidate.length <= maxChunkLength) {
        currentChunk = candidate;
      } else {
        if (currentChunk) {
          chunks.push(currentChunk);
        }
        currentChunk = paragraph;
      }
    }

    if (currentChunk) {
      chunks.push(currentChunk);
    }

    return chunks;
  }

  static _buildChunkSummaryPrompt(chunkText, config, index, total) {
    const basePrompt = config.summarizePrompt
      .replace('{target}', config.targetLanguage)
      .replace('{text}', chunkText);

    if (total === 1) {
      return basePrompt;
    }

    return `${basePrompt}\n\n补充要求：这是长网页的第 ${index}/${total} 段，请只总结当前片段的关键信息，避免猜测和重复。`;
  }

  static _buildFinalSummaryPrompt(partialSummaries, config) {
    return [
      `请使用${config.targetLanguage}整合以下同一网页的分段摘要，输出一份最终摘要。`,
      '要求：',
      '1. 去除重复信息',
      '2. 保留核心观点、事实、结论和关键数据',
      '3. 结构清晰，语言简洁',
      '4. 不要提及“第几段摘要”之类的过程描述',
      '',
      partialSummaries.join('\n\n')
    ].join('\n');
  }

  static async _requestChatText(url, config, prompt, maxTokens) {
    const body = {
      model: config.translateModel,
      messages: [
        { role: 'user', content: prompt }
      ],
      temperature: 0.5,
      max_tokens: maxTokens
    };

    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), config.apiTimeout * 1000);

    try {
      const response = await fetch(url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          ...(config.translateApiKey ? { 'Authorization': `Bearer ${config.translateApiKey}` } : {})
        },
        body: JSON.stringify(body),
        signal: controller.signal
      });

      clearTimeout(timeoutId);

      if (!response.ok) {
        const errorMessage = await this._readErrorMessage(response);
        throw new Error(`HTTP ${response.status}: ${errorMessage || response.statusText}`);
      }

      const data = await response.json();
      return this._extractMessageText(data).trim();
    } catch (error) {
      clearTimeout(timeoutId);
      throw error;
    }
  }

  static async _readErrorMessage(response) {
    try {
      const data = await response.json();
      return data?.error?.message || data?.message || JSON.stringify(data);
    } catch (e) {
      try {
        return await response.text();
      } catch (ignored) {
        return '';
      }
    }
  }

  static _emitProgress(onProgress, payload) {
    if (typeof onProgress === 'function') {
      onProgress(payload);
    }
  }
}

if (typeof window !== 'undefined') {
  window.TranslateAPI = TranslateAPI;
}
