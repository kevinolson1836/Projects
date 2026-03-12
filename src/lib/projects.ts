import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import yaml from 'js-yaml';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const PROJECTS_ROOT = path.resolve(__dirname, '../../projects');

export interface Project {
  slug: string;
  title: string;
  status: string;
  progress: number;
  started: string;
  tags: string[];
  notes: string;
}

function getProjectDirs(): string[] {
  if (!fs.existsSync(PROJECTS_ROOT)) {
    return [];
  }
  return fs.readdirSync(PROJECTS_ROOT, { withFileTypes: true })
    .filter((d) => d.isDirectory())
    .map((d) => d.name);
}

export function loadProjects(): Project[] {
  const dirs = getProjectDirs();
  const projects: Project[] = [];

  for (const dir of dirs) {
    const projectPath = path.join(PROJECTS_ROOT, dir);
    const yamlPath = path.join(projectPath, 'project.yaml');
    const notesPath = path.join(projectPath, 'notes.md');

    if (!fs.existsSync(yamlPath)) continue;

    const yamlContent = fs.readFileSync(yamlPath, 'utf-8');
    const data = yaml.load(yamlContent) as Record<string, unknown>;

    const notes = fs.existsSync(notesPath)
      ? fs.readFileSync(notesPath, 'utf-8')
      : '';

    projects.push({
      slug: (data.slug as string) ?? dir,
      title: (data.title as string) ?? dir,
      status: (data.status as string) ?? 'unknown',
      progress: (data.progress as number) ?? 0,
      started: (data.started as string) ?? '',
      tags: Array.isArray(data.tags) ? (data.tags as string[]) : [],
      notes,
    });
  }

  return projects.sort((a, b) => a.title.localeCompare(b.title));
}

export function getProjectBySlug(slug: string): Project | undefined {
  return loadProjects().find((p) => p.slug === slug);
}
