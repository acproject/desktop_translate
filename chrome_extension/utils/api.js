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

    const ocrBody = {
      model: config.ocrModel,
      messages: [
        {
          role: 'user',
          content: [
            { type: 'text', text: config.ocrPrompt },
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

      if (!cleanedOcrText.trim()) {
        return { success: false, originalText: '', translatedText: '', error: '未能识别到文字' };
      }

      const translateResult = await this.translate(cleanedOcrText, config);
      return {
        success: translateResult.success,
        originalText: cleanedOcrText,
        translatedText: translateResult.translatedText,
        error: translateResult.error
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

  static async summarize(text, config) {
    const url = this._buildUrl(config.translateApiHost, config.translateApiPort, '/v1/chat/completions');
    const prompt = config.summarizePrompt
      .replace('{target}', config.targetLanguage)
      .replace('{text}', text);

    const body = {
      model: config.translateModel,
      messages: [
        { role: 'user', content: prompt }
      ],
      temperature: 0.5,
      max_tokens: 2048
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
      const summaryText = this._extractMessageText(data);
      return { success: true, summary: summaryText };
    } catch (error) {
      clearTimeout(timeoutId);
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
}

if (typeof window !== 'undefined') {
  window.TranslateAPI = TranslateAPI;
}
