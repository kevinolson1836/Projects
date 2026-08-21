import { next, rewrite } from '@vercel/edge';

export default function middleware(request: Request) {
  const url = new URL(request.url);

  if (url.hostname === 'wezterm.kevinolson.org' && url.pathname === '/') {
    return rewrite(new URL('/wezterm.lua', request.url));
  }

  return next();
}
