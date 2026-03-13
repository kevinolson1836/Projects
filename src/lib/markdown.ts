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

const MODEL_EXT = /\.(stl|obj|glb|gltf)$/i;

/**
 * Render markdown to HTML.
 * @param md - Markdown source
 * @param imageBaseUrl - Optional base URL for relative image src (e.g. "/images/chessboard/").
 *   In content use ![](./photo.png); images go in public/images/{project-slug}/
 * @param projectSlug - Optional project slug; links to 3d-models/* are rendered as embedded viewers.
 */
export function renderMarkdown(md: string, imageBaseUrl?: string, projectSlug?: string): string {
  const base = imageBaseUrl?.replace(/\/?$/, '/') ?? '';
  const escapeAttr = (s: string) => s.replace(/&/g, '&amp;').replace(/"/g, '&quot;');

  const renderer = new marked.Renderer();

  if (base) {
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
  }

  if (projectSlug) {
    renderer.link = function (token: marked.Tokens.Link) {
      const href = (token.href ?? '').replace(/^\.\//, '');
      const isModel =
        (href.startsWith('3d-models/') || href === '3d-models') && MODEL_EXT.test(href);
      if (isModel) {
        const filename = href.includes('/') ? href.replace(/^3d-models\/?/, '') : '';
        if (!filename) return `<a href="${escapeAttr(token.href ?? '')}">${token.text}</a>`;
        const modelUrl = `/models/${escapeAttr(projectSlug)}/${encodeURIComponent(filename)}`;
        return `<div class="embed-3d" data-src="${escapeAttr(modelUrl)}" data-label="${escapeAttr(token.text ?? filename)}"></div>`;
      }
      const title = token.title ? ` title="${escapeAttr(token.title)}"` : '';
      return `<a href="${escapeAttr(token.href ?? '')}"${title}>${token.text}</a>`;
    };
  }

  return marked.parse(md, { renderer }) as string;
}