import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');
const projectsDir = path.join(root, 'projects');
const outDir = path.join(root, 'public', 'images');

const imageExt = ['.png', '.jpg', '.jpeg', '.gif', '.webp', '.svg'];

if (!fs.existsSync(projectsDir)) {
  process.exit(0);
}

const slugs = fs.readdirSync(projectsDir, { withFileTypes: true })
  .filter((d) => d.isDirectory())
  .map((d) => d.name);

for (const slug of slugs) {
  const imageDir = path.join(projectsDir, slug, 'images');
  if (!fs.existsSync(imageDir) || !fs.statSync(imageDir).isDirectory()) continue;

  const dest = path.join(outDir, slug);
  fs.mkdirSync(dest, { recursive: true });

  const files = fs.readdirSync(imageDir, { withFileTypes: true })
    .filter((e) => e.isFile())
    .map((e) => e.name)
    .filter((name) => imageExt.includes(path.extname(name).toLowerCase()));

  for (const name of files) {
    const src = path.join(imageDir, name);
    const dst = path.join(dest, name);
    fs.copyFileSync(src, dst);
  }
}
