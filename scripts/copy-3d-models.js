import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const root = path.resolve(__dirname, '..');
const projectsDir = path.join(root, 'projects');
const outDir = path.join(root, 'public', 'models');

if (!fs.existsSync(projectsDir)) {
  process.exit(0);
}

const slugs = fs.readdirSync(projectsDir, { withFileTypes: true })
  .filter((d) => d.isDirectory())
  .map((d) => d.name);

for (const slug of slugs) {
  const modelDir = path.join(projectsDir, slug, '3d-models');
  if (!fs.existsSync(modelDir) || !fs.statSync(modelDir).isDirectory()) continue;

  const dest = path.join(outDir, slug);
  fs.mkdirSync(dest, { recursive: true });

  const files = fs.readdirSync(modelDir, { withFileTypes: true })
    .filter((e) => e.isFile())
    .map((e) => e.name);

  for (const name of files) {
    const src = path.join(modelDir, name);
    const dst = path.join(dest, name);
    fs.copyFileSync(src, dst);
  }
}
