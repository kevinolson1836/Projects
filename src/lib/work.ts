import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import yaml from 'js-yaml';

export interface WorkItem {
  label: string;
  href: string;
  external?: boolean;
}

export const workItems: WorkItem[] = [
  { label: 'Notion', href: 'https://notion.so', external: true },
  { label: 'GitHub', href: 'https://github.com', external: true },
  { label: 'Figma', href: 'https://figma.com', external: true },
  { label: 'Slack', href: 'https://slack.com', external: true },
];

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const WORK_ROOT = path.resolve(__dirname, '../../work');

export interface Work {
  slug: string;
  title: string;
  status: string;
  progress: number;
  started: string;
  tags: string[];
  notes: string;
  models: string[];
}

function getWorkDirs(): string[] {
  if (!fs.existsSync(WORK_ROOT)) {
    return [];
  }
  return fs.readdirSync(WORK_ROOT, { withFileTypes: true })
    .filter((d) => d.isDirectory())
    .map((d) => d.name);
}

export function loadWork(): Work[] {
  const dirs = getWorkDirs();
  const work: Work[] = [];

  for (const dir of dirs) {
    const workPath = path.join(WORK_ROOT, dir);
    const yamlPath = path.join(workPath, 'work.yaml');
    const notesPath = path.join(workPath, 'notes.md');
    const modelsDir = path.join(workPath , '3d-models');

    if (!fs.existsSync(yamlPath)) continue;

    const yamlContent = fs.readFileSync(yamlPath, 'utf-8');
    const data = yaml.load(yamlContent) as Record<string, unknown>;

    const notes = fs.existsSync(notesPath)
      ? fs.readFileSync(notesPath, 'utf-8')
      : '';

    const models =
      fs.existsSync(modelsDir) && fs.statSync(modelsDir).isDirectory()
        ? fs
            .readdirSync(modelsDir, { withFileTypes: true })
            .filter((entry) => entry.isFile())
            .map((entry) => entry.name)
        : [];

    work.push({
      slug: (data.slug as string) ?? dir,
      title: (data.title as string) ?? dir,
      status: (data.status as string) ?? 'unknown',
      progress: (data.progress as number) ?? 0,
      started: (data.started as string) ?? '',
      tags: Array.isArray(data.tags) ? (data.tags as string[]) : [],
      notes,
      models,
    });
  }

  return work.sort((a, b) => a.title.localeCompare(b.title));
}

export function getWorkBySlug(slug: string): Work | undefined {
  return loadWork().find((w) => w.slug === slug);
}
