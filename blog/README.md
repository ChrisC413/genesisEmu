# blog/: "How genesisEmu is built"

A static site with one article per completed story, published to GitHub Pages at
<https://chrisc413.github.io/genesisEmu/> by `.github/workflows/pages.yml` whenever `blog/**` changes on `main`.

```
blog/
  index.html          Series landing page: articles grouped by milestone, with a one-line blurb each
  posts/              One page per story: <story-id>-<slug>.html, e.g. M01-03-effective-address-modes.html
  assets/style.css    The only stylesheet (light and dark via prefers-color-scheme, phone-width layout)
```

Plain HTML and CSS: no build step, no JavaScript, no framework. Fonts come from Google Fonts and fall back to
system fonts. Diagrams are inline SVG using the `dg-*` classes in `style.css`, so they follow the colour theme.

Preview locally with `python -m http.server -d blog 8000` and open <http://localhost:8000/>.

## Rules for content

- Never include ROM bytes, screenshots or frames, audio, or any other asset from a commercial game. Draw your own
  diagrams. Naming the games and describing their behaviour is fine.
- Take every figure (test counts, timings, hashes, percentages) from the story file, the tests, the commit history
  or the pull request. If a criterion is unverified, say so.
- Code excerpts are copied verbatim from the repository (whitespace aside). Use `// ...` on its own line to mark
  an elision. Escape `<`, `>` and `&` inside `<pre>`.

## Adding an article for a new story

1. Copy the last article in `posts/` to `posts/<story-id>-<slug>.html`.
2. Update `<title>`, the `description` and `og:` meta tags, the kicker (milestone and "Story N of M"), `<h1>`,
   the subtitle, the reading time (words / 230, rounded up), the story link in the byline, the body and the
   "Where to look in the repo" list.
3. Fix the navigation: the new page's "Previous" points at the old last article and has no "Next"; add a
   "Next" link to the old last article. If the milestone gained a story, update "Story N of M" in that
   milestone's other articles.
4. Add an entry to the milestone's list in `index.html` (and a new `<section class="milestone">` plus a `.toc`
   link for a new milestone). Update the article count and reading time in the hero.
5. Check that every link resolves (all links are relative), view the page at phone width and in dark mode,
   and run `python tools/check_line_endings.py`.

Code links point at `main`. Articles for stories whose code is still on an unmerged branch will show 404s for
those links until the branch merges.
