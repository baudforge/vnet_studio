<script setup lang="ts">
withDefaults(defineProps<{ name: string; size?: number }>(), { size: 20 })

const paths: Record<string, string[]> = {
  home: ['M3 10.5 12 3l9 7.5', 'M5 9v11h5v-6h4v6h5V9'],
  apps: ['M3 3h7v7H3z', 'M14 3h7v7h-7z', 'M3 14h7v7H3z', 'M14 14h7v7h-7z'],
  logs: ['M4 4h16v16H4z', 'm7 8 2 2-2 2', 'M12 12h5', 'M7 16h10'],
  help: [
    'M9.2 8.5a3 3 0 0 1 5.6 1.5c0 2-2.8 2-2.8 4',
    'M12 17h.01',
    'M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0'
  ],
  settings: [
    'm9 3 6 0 .6 2.5 2.1 1.2 2.4-.7 2.4 4.1-1.8 1.9v2.1l1.8 1.9-2.4 4.1-2.4-.7-2.1 1.2L15 23H9l-.6-2.4-2.1-1.2-2.4.7-2.4-4.1 1.8-1.9V12l-1.8-1.9L3.9 6l2.4.7 2.1-1.2z',
    'M15 13a3 3 0 1 1-6 0 3 3 0 0 1 6 0'
  ],
  activity: ['M3 12h4l3-7 4 14 3-7h4'],
  arrow: ['M5 12h14', 'm13 6 6 6-6 6'],
  play: ['m9 5 11 7-11 7z'],
  pause: ['M8 5v14', 'M16 5v14'],
  plus: ['M12 5v14', 'M5 12h14'],
  search: ['M17 10a7 7 0 1 1-14 0 7 7 0 0 1 14 0', 'm15 15 6 6'],
  close: ['m6 6 12 12', 'm6 18 12-12'],
  external: ['M14 3h7v7', 'm21 3-11 11', 'M10 3H4v17h17v-7'],
  dock: ['M3 4h18v16H3z', 'M3 9h18', 'M12 12v5', 'm9 14 3 3 3-3'],
  chevron: ['m9 5 7 7-7 7'],
  clock: ['M21 12a9 9 0 1 1-18 0 9 9 0 0 1 18 0', 'M12 7v5l3 2'],
  sun: [
    'M16 12a4 4 0 1 1-8 0 4 4 0 0 1 8 0',
    'M12 2v2m0 16v2M2 12h2m16 0h2M5 5l1.5 1.5m11 11L19 19M5 19l1.5-1.5m11-11L19 5'
  ],
  moon: ['M20 14a8.5 8.5 0 0 1-10-10A8.5 8.5 0 1 0 20 14z'],
  monitor: ['M3 4h18v13H3z', 'M12 17v4', 'M8 21h8'],
  window: ['M3 4h18v16H3z', 'M3 9h18', 'M6 6.5h.01m3 0h.01'],
  document: ['M6 3h8l4 4v14H6z', 'M14 3v5h4', 'M9 12h6m-6 4h6'],
  minus: ['M5 12h14'],
  maximize: ['M5 5h14v14H5z'],
  check: ['m5 12 4 4L19 6'],
  reset: ['M3 9a9 9 0 1 1 0 6', 'M3 3v6h6'],
  shield: ['M12 3 4 6v6c0 5 8 9 8 9s8-4 8-9V6z', 'm8 12 3 3 5-6']
}
</script>

<template>
  <svg
    :width="size"
    :height="size"
    viewBox="0 0 24 24"
    fill="none"
    stroke="currentColor"
    stroke-width="1.7"
    stroke-linecap="round"
    stroke-linejoin="round"
    aria-hidden="true"
  >
    <path v-for="(path, index) in paths[name] ?? paths.window" :key="index" :d="path" />
  </svg>
</template>
