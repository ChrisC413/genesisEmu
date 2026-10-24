# 05 · Multiple Projects and Backlogs

Priority 2. One monitor shows agents across several projects, each project's backlog (milestones and
stories with their statuses), and which agent is working on which story.

## Project = repository

Every project is a git repository tied to a GitHub repository, **private by default**. It declares
itself with a committed registration file at its root, **`.agent-project.json`**:

```json
{
  "name": "genesisEmu",
  "github": {"repo": "ChrisC413/genesisEmu", "visibility": "private"},
  "status_dir": ".agent-status",
  "stories": {"root": "stories", "milestone_glob": "*", "story_glob": "*-[0-9][0-9]-*.md"},
  "issues": {"mode": "off"}
}
```

| Key | Default | Meaning |
|-----|---------|---------|
| `name` | folder name | Display name, and the key used in the registry |
| `github.repo` | — | `owner/name`. Without it, PR, CI and Issues features are off for the project. |
| `github.visibility` | `private` | `private`, `internal` or `public`. The tool never changes a repository's visibility. The value is a declaration that features check, for example the Issues sync warns before writing story text into a public repository. |
| `status_dir` | `.agent-status` | Relative to the **main checkout** of the repository, so all worktrees share it |
| `stories.root` | `stories` | Folder that holds the milestone folders |
| `stories.milestone_glob` / `story_glob` | `*` / `*-[0-9][0-9]-*.md` | Which folders are milestones and which files are stories |
| `issues.mode` | `off` | `off`, `link` or `push` ([06](06-github-issues-sync.md)) |

Repositories without the file still work with these defaults, which is how the monitor behaved before
this spec.

## Registry (per machine, not committed)

`$AGENT_MONITOR_HOME/projects.json` (default `~/.agent-monitor/projects.json`) lists the local checkouts
to show together:

```json
{"projects": [{"path": "C:/Users/chris/Desktop/projects/genesisEmu"},
              {"path": "C:/Users/chris/Desktop/projects/agent-monitor"}]}
```

Planned commands (AM2-02): `monitor.py projects add <path>`, `projects list`, and `projects remove <name>`.
Running `--project <name|path>` selects one project, and `--all` selects every registered project.

## Story format

The format is the one genesisEmu already uses: the first line is `# <ID> · <title>`, followed by
`**Status:** Draft|Ready|In progress|Done` and `**Depends on:** <IDs or —>`. A milestone folder's
`README.md` first line is `# <ID> · <title>`. Files that don't match, such as `_TEMPLATE.md` and notes,
are ignored.

## Views

- **`monitor.py backlog [--project P | --all] [--milestone M] [--json]`** (built):
  - one line per milestone with status counts;
  - the stories that aren't done, each with `<- agent` when an active agent's latest event is on that
    story;
  - `(reported done by agent)` when an agent emitted `done` but the file still says otherwise. That
    usually means the story's status line is updated on the agent's branch and not yet on `main`.
- **Cross-project live view** (AM2-02): the agent table gains a PROJECT column, and all registered
  projects' status directories are tailed together. Agent names are unique per project, and rows are
  keyed by `(project, agent)`.
- **Current work summary** (MQ9, AM2-03): one line per agent that says what the agent is targeting
  now: the story title, the agent's own latest `message`, and its branch's latest commit subject. It
  doesn't parse acceptance criteria.

## Source of truth

The story files on the default branch (`main`) of the main checkout are the backlog. Agents change
story status on their branches, so the backlog view can lag until merge, and the "reported done by"
hint bridges that gap. Reading story files from each agent's branch (`git show <branch>:<path>`) is a
possible later refinement (OQ-5).
