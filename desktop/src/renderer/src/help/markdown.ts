import MarkdownIt from 'markdown-it'
import { documentImages, resolveHelpUrl } from './index'

const markdown = new MarkdownIt({ html: false, linkify: false, typographer: false })

export function renderChapter(source: string, file: string): string {
  const tokens = markdown.parse(source, {})
  const headings = new Map<string, number>()
  for (let index = 0; index < tokens.length; index++) {
    const token = tokens[index]
    if (token.type === 'heading_open') {
      const title = tokens[index + 1]?.content ?? ''
      const slug =
        title
          .toLowerCase()
          .replace(/[^\p{L}\p{N}]+/gu, '-')
          .replace(/^-|-$/g, '') || 'section'
      const count = (headings.get(slug) ?? 0) + 1
      headings.set(slug, count)
      token.attrSet('id', count === 1 ? slug : slug + '-' + count)
    }
    for (const child of token.children ?? []) {
      if (child.type !== 'image') continue
      const url = resolveHelpUrl(file, String(child.attrGet('src') ?? ''))
      const source =
        url.origin === 'https://documentation.local'
          ? documentImages['.' + decodeURIComponent(url.pathname)]
          : undefined
      if (source) child.attrSet('src', source)
      else {
        child.type = 'text'
        child.tag = ''
        child.children = null
        child.attrs = null
      }
    }
  }
  return markdown.renderer.render(tokens, markdown.options, {})
}
