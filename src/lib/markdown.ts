import { marked } from 'marked';
import hljs from 'highlight.js';

marked.setOptions({
  highlight(code: string, lang: string) {
    if (lang && hljs.getLanguage(lang)) {
      return hljs.highlight(code, { language: lang }).value;
    }
    return hljs.highlightAuto(code).value;
  },
    gfm: true,
    breaks: true
});

/**
 * Render markdown to HTML.
 * @param md - Markdown source
 * @param imageBaseUrl - Optional base URL for relative image src (e.g. "/images/chessboard/").
 *   In content use ![](./photo.png); images go in public/images/{project-slug}/
 */
export function renderMarkdown(md: string, imageBaseUrl?: string): string {
  if (!imageBaseUrl) {
    return marked.parse(md) as string;
  }

  const base = imageBaseUrl.replace(/\/?$/, '/');
  const escapeAttr = (s: string) => s.replace(/&/g, '&amp;').replace(/"/g, '&quot;');

  const renderer = new marked.Renderer();

  renderer.image = function (token: marked.Tokens.Image) {
    const href = token.href ?? '';

    const resolved =
      href.startsWith('.') || (!href.startsWith('/') && !href.startsWith('http'))
        ? base + href.replace(/^\.\//, '')
        : href;

    const title = token.title ? ` title="${escapeAttr(token.title)}"` : '';
    const alt = escapeAttr(token.text ?? '');

    return `<img src="${escapeAttr(resolved)}" alt="${alt}"${title}>`;
  };

  return marked.parse(md, { renderer }) as string;
}