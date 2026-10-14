<script setup lang="ts">
import { computed, nextTick, onUnmounted, reactive, ref, watch } from 'vue'
import type { ApplicationDefinition } from '@shared/workspace'
import { documentationGuides, linkedChapter, loadChapter, resolveHelpUrl } from '../help'
import { renderChapter } from '../help/markdown'
import Icon from './Icon.vue'

const props = defineProps<{ applications: ApplicationDefinition[]; chapter: string }>()
const emit = defineEmits<{ 'update:chapter': [chapter: string] }>()
const query = ref('')
const collapsed = reactive(new Set<string>())
const guides = computed(() => documentationGuides(props.applications))
const active = computed(
  () =>
    guides.value
      .flatMap((guide) => guide.chapters)
      .find((chapter) => chapter.id === props.chapter) ?? guides.value[0]?.chapters[0]
)
const activeGuide = computed(() =>
  guides.value.find((guide) => guide.chapters.some((chapter) => chapter.id === active.value?.id))
)
const visibleGuides = computed(() => {
  const search = query.value.trim().toLowerCase()
  return guides.value
    .map((guide) => ({
      ...guide,
      chapters: guide.title.toLowerCase().includes(search)
        ? guide.chapters
        : guide.chapters.filter((chapter) => chapter.title.toLowerCase().includes(search))
    }))
    .filter((guide) => guide.chapters.length)
})
const html = ref('')
const loading = ref(false)
const error = ref('')
const scroll = ref<HTMLElement | null>(null)
const content = ref<HTMLElement | null>(null)
let request = 0
let pendingAnchor = ''

function toggleGuide(id: string): void {
  if (collapsed.has(id)) collapsed.delete(id)
  else collapsed.add(id)
}

function selectChapter(id: string): void {
  pendingAnchor = ''
  emit('update:chapter', id)
  if (id === active.value?.id && scroll.value) scroll.value.scrollTop = 0
}

function scrollToAnchor(anchor: string): void {
  const heading = [...(content.value?.querySelectorAll<HTMLElement>('[id]') ?? [])].find(
    (heading) => heading.id === anchor
  )
  heading?.scrollIntoView({ block: 'start' })
}

async function followLink(event: MouseEvent): Promise<void> {
  const target = event.target
  if (!(target instanceof Element)) return
  const link = target.closest<HTMLAnchorElement>('a[href]')
  if (!link || !active.value) return
  event.preventDefault()
  error.value = ''
  try {
    const href = link.getAttribute('href') ?? ''
    const destination = linkedChapter(guides.value, active.value.file, href)
    if (destination) {
      const guide = guides.value.find((guide) => guide.chapters.includes(destination.chapter))
      if (guide) collapsed.delete(guide.id)
      if (destination.chapter.id === active.value.id) scrollToAnchor(destination.anchor)
      else {
        pendingAnchor = destination.anchor
        query.value = ''
        emit('update:chapter', destination.chapter.id)
      }
    } else {
      const url = resolveHelpUrl(active.value.file, href)
      if (url.origin === 'https://documentation.local')
        error.value = 'This chapter is not available.'
      else await window.api.openDocumentationLink(url.href)
    }
  } catch {
    error.value = 'This link could not be opened.'
  }
}

watch(query, () => {
  if (query.value.trim()) collapsed.clear()
})
watch(
  () => active.value?.id,
  async () => {
    const current = ++request
    html.value = ''
    error.value = ''
    loading.value = true
    try {
      const chapter = active.value
      if (chapter) {
        const source = await loadChapter(chapter)
        if (request === current) html.value = renderChapter(source, chapter.file)
      }
    } catch {
      if (request === current) error.value = 'Unable to load this chapter.'
    } finally {
      if (request === current) {
        loading.value = false
        await nextTick()
        if (scroll.value) scroll.value.scrollTop = 0
        if (pendingAnchor) scrollToAnchor(pendingAnchor)
        pendingAnchor = ''
      }
    }
  },
  { immediate: true }
)
onUnmounted(() => {
  request += 1
})
</script>

<template>
  <section class="help-reader" data-testid="docs-page" aria-label="Docs">
    <aside class="documentation-tree">
      <div class="tree-heading">DOCUMENTATION</div>
      <label class="chapter-search"
        ><Icon name="search" :size="16" /><input
          v-model="query"
          aria-label="Find a chapter"
          placeholder="Find a chapter..."
          data-testid="help-search"
      /></label>
      <nav class="tree-scroll" aria-label="Documentation contents">
        <p v-if="!visibleGuides.length" class="tree-empty">No chapters found.</p>
        <ul class="guide-list">
          <li v-for="guide in visibleGuides" :key="guide.id" :data-guide-id="guide.id">
            <button
              class="guide-toggle"
              :aria-expanded="!collapsed.has(guide.id)"
              :aria-controls="'guide-' + guide.id"
              :data-testid="'help-guide-' + guide.id"
              @click="toggleGuide(guide.id)"
            >
              <Icon
                name="chevron"
                :size="13"
                :class="{ expanded: !collapsed.has(guide.id) }"
              /><Icon :name="guide.id === 'workspace' ? 'window' : 'apps'" :size="16" /><span>{{
                guide.title
              }}</span>
            </button>
            <ul v-if="!collapsed.has(guide.id)" :id="'guide-' + guide.id" class="chapter-list">
              <li v-for="entry in guide.chapters" :key="entry.id">
                <button
                  :class="['chapter-link', { selected: active?.id === entry.id }]"
                  :aria-current="active?.id === entry.id ? 'page' : undefined"
                  :data-chapter-id="entry.id"
                  @click="selectChapter(entry.id)"
                >
                  <Icon name="document" :size="15" /><span>{{ entry.title }}</span>
                </button>
              </li>
            </ul>
          </li>
        </ul>
      </nav>
    </aside>
    <div class="documentation-panel">
      <header class="document-path">
        <span>{{ activeGuide?.title }}</span
        ><Icon name="chevron" :size="13" /><strong>{{ active?.title }}</strong>
      </header>
      <div v-if="error" class="document-error" role="alert">{{ error }}</div>
      <div ref="scroll" class="document-scroll" data-testid="help-document-scroll">
        <p v-if="loading" class="document-loading" role="status">Loading documentation...</p>
        <!-- Raw HTML is disabled by the Markdown parser; links stay within the reader or open externally. -->
        <!-- eslint-disable vue/no-v-html -->
        <article
          v-else
          ref="content"
          class="markdown-document"
          data-testid="help-content"
          aria-label="Read-only documentation"
          @click="followLink"
          @auxclick.prevent="followLink"
          v-html="html"
        ></article>
        <!-- eslint-enable vue/no-v-html -->
      </div>
    </div>
  </section>
</template>

<style scoped>
.help-reader {
  display: flex;
  flex: 1;
  min-height: 0;
  min-width: 0;
}
.documentation-tree {
  width: 276px;
  flex-shrink: 0;
  display: flex;
  flex-direction: column;
  background: var(--surface-soft);
  border-right: 1px solid var(--border);
}
.tree-heading {
  padding: 17px 18px 12px;
  font-size: 12px;
  font-weight: 600;
  color: var(--muted);
  letter-spacing: 0.8px;
}
.chapter-search {
  display: flex;
  align-items: center;
  gap: 8px;
  margin: 0 12px 14px;
  padding: 7px 9px;
  border: 1px solid var(--border);
  border-radius: 5px;
  background: var(--surface);
  color: var(--muted);
}
.chapter-search input {
  width: 100%;
  min-width: 0;
  border: 0;
  outline: 0;
  background: transparent;
  color: var(--text);
  font-size: 13px;
}
.tree-scroll {
  flex: 1;
  min-height: 0;
  overflow: auto;
  padding-bottom: 18px;
}
.tree-empty {
  padding: 16px 18px;
  font-size: 13px;
  color: var(--muted);
}
.guide-list > li + li {
  margin-top: 12px;
}
.guide-toggle,
.chapter-link {
  display: flex;
  align-items: center;
  width: 100%;
  padding: 9px 14px;
  border: 0;
  background: transparent;
  text-align: left;
  color: var(--text);
  font-size: 13px;
  line-height: 1.4;
}
.guide-toggle {
  gap: 8px;
  font-weight: 500;
}
.guide-toggle > svg:first-child {
  transition: transform 0.12s;
}
.guide-toggle > svg.expanded {
  transform: rotate(90deg);
}
.guide-toggle > span,
.chapter-link > span {
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}
.chapter-link {
  gap: 9px;
  padding-left: 41px;
  font-weight: 400;
}
.chapter-link svg {
  color: var(--muted);
}
.guide-toggle:hover,
.chapter-link:hover {
  background: var(--surface);
}
.chapter-link.selected {
  background: var(--accent-soft);
  color: var(--accent);
}
.chapter-link.selected svg {
  color: var(--accent);
}
.documentation-panel {
  flex: 1;
  min-width: 0;
  min-height: 0;
  display: flex;
  flex-direction: column;
  background: var(--surface);
}
.document-path {
  display: flex;
  align-items: center;
  gap: 10px;
  height: 44px;
  flex-shrink: 0;
  padding: 0 28px;
  font-size: 13px;
  color: var(--muted);
  border-bottom: 1px solid var(--border);
}
.document-path strong {
  color: var(--text);
  font-weight: 500;
  overflow: hidden;
  white-space: nowrap;
  text-overflow: ellipsis;
}
.document-scroll {
  flex: 1;
  min-height: 0;
  overflow: auto;
  padding: 30px 40px 45px;
}
.document-loading {
  color: var(--muted);
  font-size: 13px;
}
.document-error {
  padding: 10px 28px;
  font-size: 13px;
  color: var(--text);
  background: var(--accent-soft);
}
.markdown-document {
  max-width: 850px;
  margin: 0 auto;
  font-size: 13px;
  line-height: 1.85;
  overflow-wrap: anywhere;
}
.markdown-document :deep(h1) {
  margin: 0 0 18px;
  font-size: 22px;
  line-height: 1.4;
  letter-spacing: -0.5px;
}
.markdown-document :deep(h2) {
  margin: 30px 0 10px;
  font-size: 16px;
  line-height: 1.5;
  scroll-margin-top: 20px;
}
.markdown-document :deep(h3) {
  margin: 23px 0 8px;
  font-size: 14px;
  scroll-margin-top: 20px;
}
.markdown-document :deep(p) {
  margin: 0 0 16px;
}
.markdown-document :deep(ul),
.markdown-document :deep(ol) {
  padding-left: 24px;
  margin: 0 0 18px;
}
.markdown-document :deep(ul) {
  list-style: disc;
}
.markdown-document :deep(li) {
  padding-left: 3px;
  margin-bottom: 6px;
}
.markdown-document :deep(a) {
  color: var(--accent);
  text-decoration: underline;
  text-underline-offset: 3px;
}
.markdown-document :deep(table) {
  border-collapse: collapse;
  width: 100%;
  margin: 16px 0 22px;
  font-size: 13px;
}
.markdown-document :deep(th),
.markdown-document :deep(td) {
  padding: 9px 12px;
  border: 1px solid var(--border);
  text-align: left;
}
.markdown-document :deep(th) {
  background: var(--surface-soft);
  font-weight: 600;
}
.markdown-document :deep(code) {
  font-family: 'Cascadia Code', Consolas, monospace;
  font-size: 13px;
  background: var(--surface-soft);
  border-radius: 4px;
  padding: 2px 5px;
}
.markdown-document :deep(pre) {
  overflow: auto;
  padding: 16px;
  margin: 0 0 20px;
  border: 1px solid var(--border);
  border-radius: 5px;
  background: var(--surface-soft);
  line-height: 1.7;
}
.markdown-document :deep(pre code) {
  padding: 0;
  background: transparent;
  white-space: pre;
}
.markdown-document :deep(blockquote) {
  border-left: 3px solid var(--accent);
  padding: 5px 16px;
  margin: 0 0 18px;
  color: var(--muted);
  background: var(--surface-soft);
}
.markdown-document :deep(img) {
  display: block;
  max-width: 100%;
  height: auto;
  margin: 18px 0;
}
@media (max-width: 1100px) {
  .documentation-tree {
    width: 245px;
  }
  .document-scroll {
    padding: 26px 28px 35px;
  }
  .document-path {
    padding: 0 22px;
  }
}
</style>
