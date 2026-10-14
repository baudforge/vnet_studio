# VNet Studio Desktop

An application workspace built with Electron, Vue, and TypeScript.

The sidebar contains Home, Logs, Apps, and Docs, in that order. Use Ctrl / Command + 1 through 4 to open them. Launch applications from Apps into Home tabs. Detach an instance using the editor toolbar or View menu, then use the Dock to Home title-bar button to return it without resetting its session.

## Recommended IDE Setup

- [VSCode](https://code.visualstudio.com/) + [ESLint](https://marketplace.visualstudio.com/items?itemName=dbaeumer.vscode-eslint) + [Prettier](https://marketplace.visualstudio.com/items?itemName=esbenp.prettier-vscode) + [Volar](https://marketplace.visualstudio.com/items?itemName=Vue.volar)

## Project Setup

### Install

```bash
$ npm install
```

### Development

```bash
$ npm run dev
```

### Validation

```bash
npm run lint
npm run test:logs
npm run test:smoke
```

### Docs

User manuals live in `src/renderer/src/help/workspace` and `src/renderer/src/help/applications/<application-id>`. Each guide has an `index.json` that orders its Markdown chapters. Registered applications automatically get a root in the Docs tree.

Vite includes the chapter content in the renderer build and loads it on demand. Electron Builder ships the renderer files inside the application package, so Docs works offline. Add local images alongside the chapters to include them in the same build.

### Build

```bash
# For windows
$ npm run build:win

# For macOS
$ npm run build:mac

# For Linux
$ npm run build:linux
```
