# Kevin's Lab

A VSCode-style site for browsing engineering projects and build logs. Built with **Astro**.

## Requirements

- **Node.js** >= 18.14.1 (recommended: 20.x or 22.x)
- npm

## Setup

```bash
npm install
```

## Development

```bash
npm run dev
```

Open [http://localhost:4321](http://localhost:4321).

## Build

```bash
npm run build
```

Output in `dist/`.

## Project structure

- **Dashboard** (`/`) – stats and active projects
- **Project pages** (`/projects/[slug]`) – build logs (Markdown) with code highlighting

Add projects by creating folders under `projects/`:

```
projects/
  your-project/
    project.yaml   # slug, title, status, progress, started, tags
    notes.md       # Markdown build log
```

### Embedding images

Images in `notes.md` are supported. Put image files in **`public/images/{project-slug}/`** and reference them with a relative path in Markdown:

```markdown
![PCB layout](./pcb-v1.png)
![Schematic](/images/chessboard/schematic.png)
```

- **Relative path** `./photo.png` is resolved to `/images/{project-slug}/photo.png` (so the file must be in `public/images/{project-slug}/photo.png`).
- **Absolute path** `/images/...` works as-is.

### Work sidebar

The activity bar has two views: **Explorer** (project files) and **Work** (work-related links). Edit `src/lib/work.ts` to add or change Work items (e.g. Notion, GitHub, Figma). Use `external: true` and a full URL for links that open in a new tab.

## Tech

- Astro (static)
- marked + highlight.js (Markdown + syntax highlighting)
- js-yaml (project metadata)
