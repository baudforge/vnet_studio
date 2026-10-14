import type { ApplicationDefinition } from '@shared/workspace'

interface ChapterEntry {
  id: string
  title: string
  file: string
}
interface GuideManifest {
  chapters: ChapterEntry[]
}
export interface HelpChapter {
  id: string
  title: string
  file: string
}
export interface HelpGuide {
  id: string
  title: string
  chapters: HelpChapter[]
}

const manifests = import.meta.glob<GuideManifest>('./**/index.json', {
  eager: true,
  import: 'default'
})
const documents = import.meta.glob<string>('./**/*.md', { query: '?raw', import: 'default' })
export const documentImages = import.meta.glob<string>('./**/*.{png,jpg,jpeg,gif,webp,svg}', {
  eager: true,
  query: '?url',
  import: 'default'
})

function guide(id: string, title: string, directory: string): HelpGuide {
  const entries = manifests[directory + 'index.json']?.chapters ?? [
    { id: 'overview', title: 'Overview', file: 'overview.md' }
  ]
  return {
    id,
    title,
    chapters: entries.map((entry) => ({
      id: id + '/' + entry.id,
      title: entry.title,
      file: directory + entry.file
    }))
  }
}

export function documentationGuides(applications: ApplicationDefinition[]): HelpGuide[] {
  return [
    guide('workspace', 'Main program', './workspace/'),
    ...applications.map((application) =>
      guide(
        'application-' + application.id,
        application.name,
        './applications/' + application.id + '/'
      )
    )
  ]
}

export async function loadChapter(chapter: HelpChapter): Promise<string> {
  const load = documents[chapter.file]
  return load
    ? await load()
    : '# Overview\n\nThe user manual for this application has not been added yet.'
}

export function resolveHelpUrl(file: string, link: string): URL {
  return new URL(link, new URL(file.replace(/^\.\//, ''), 'https://documentation.local/'))
}

export function linkedChapter(
  guides: HelpGuide[],
  file: string,
  link: string
): { chapter: HelpChapter; anchor: string } | undefined {
  const url = resolveHelpUrl(file, link)
  if (url.origin !== 'https://documentation.local') return undefined
  const chapter = guides
    .flatMap((guide) => guide.chapters)
    .find((chapter) => chapter.file === '.' + decodeURIComponent(url.pathname))
  return chapter ? { chapter, anchor: decodeURIComponent(url.hash.slice(1)) } : undefined
}
