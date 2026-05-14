class TranslateAPI {
  static async translate(text, config) {
    const url = `${config.translateApiHost}:${config.translateApiPort}/v1/chat/completions`;
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
        return { success: false, originalText: text, translatedText: '', error: '请求超时' };
      }
      return { success: false, originalText: text, translatedText: '', error: error.message };
    }
  }

  static async ocrAndTranslate(imageDataUrl, config) {
    const ocrUrl = `${config.ocrApiHost}:${config.ocrApiPort}/v1/chat/completions`;

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
        return { success: false, originalText: '', translatedText: '', error: 'OCR请求超时' };
      }
      return { success: false, originalText: '', translatedText: '', error: error.message };
    }
  }

  static async summarize(text, config) {
    const url = `${config.translateApiHost}:${config.translateApiPort}/v1/chat/completions`;
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
        return { success: false, summary: '', error: '请求超时' };
      }
      return { success: false, summary: '', error: error.message };
    }
  }

  static async checkApiStatus(config) {
    const results = {
      translate: false,
      ocr: false
    };

    try {
      const translateUrl = `${config.translateApiHost}:${config.translateApiPort}/v1/models`;
      const translateResp = await fetch(translateUrl, {
        method: 'GET',
        signal: AbortSignal.timeout(5000)
      });
      results.translate = translateResp.ok;
    } catch (e) {
      results.translate = false;
    }

    try {
      const ocrUrl = `${config.ocrApiHost}:${config.ocrApiPort}/v1/models`;
      const ocrResp = await fetch(ocrUrl, {
        method: 'GET',
        signal: AbortSignal.timeout(5000)
      });
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
