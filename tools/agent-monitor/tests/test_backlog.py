import json
import os
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

from agent_monitor.backlog import attach_agents, load_backlog, render_backlog  # noqa: E402
from agent_monitor.emit import emit  # noqa: E402
from agent_monitor.issues import body_sha, plan, render_body, split_marker  # noqa: E402
from agent_monitor.project import load_project  # noqa: E402
from agent_monitor.state import Monitor  # noqa: E402
from agent_monitor.tail import DirTailer  # noqa: E402

STORY = """# {id} · {title}

**Milestone:** [M00 – Foundation](README.md)
**Status:** {status}
**Depends on:** {deps}
**Spec refs:** x

## Story

As a developer...
"""


class _Fixture(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.root = Path(self.tmp.name)
        ms = self.root / "stories" / "M00-foundation"
        ms.mkdir(parents=True)
        (ms / "README.md").write_text("# M00 · Foundation\n", encoding="utf-8")
        for sid, title, status, deps in (("M00-01", "Build", "Done", "—"),
                                         ("M00-02", "CI", "In progress", "M00-01"),
                                         ("M00-03", "Config", "Draft", "M00-01, M00-02")):
            (ms / f"{sid}-x.md").write_text(STORY.format(id=sid, title=title, status=status, deps=deps),
                                            encoding="utf-8")
        (ms / "notes.md").write_text("# not a story\n", encoding="utf-8")
        (self.root / "stories" / "_TEMPLATE.md").write_text("# M<NN>-<NN> · <title>\n", encoding="utf-8")
        (self.root / ".agent-project.json").write_text(json.dumps(
            {"name": "demo", "github": {"repo": "me/demo"}, "status_dir": "status"}), encoding="utf-8")
        self.project = load_project(self.root)

    def tearDown(self):
        self.tmp.cleanup()


class BacklogTests(_Fixture):
    def test_registration_defaults_private(self):
        p = self.project
        self.assertEqual((p.name, p.repo, p.visibility, p.issues_mode, p.registered),
                         ("demo", "me/demo", "private", "off", True))
        self.assertEqual(p.status_dir.name, "status")

    def test_registration_validation(self):
        (self.root / ".agent-project.json").write_text(json.dumps({"github": {"visibility": "secret"}}),
                                                     encoding="utf-8")
        with self.assertRaises(ValueError):
            load_project(self.root)

    def test_unregistered_project_uses_folder_name(self):
        other = self.root / "plain"
        other.mkdir()
        p = load_project(other)
        self.assertEqual((p.name, p.registered, p.repo), ("plain", False, ""))

    def test_parse_milestones_and_stories(self):
        ms = load_backlog(self.project)
        self.assertEqual([(m.id, m.title) for m in ms], [("M00", "Foundation")])
        st = ms[0].stories
        self.assertEqual([(s.id, s.status) for s in st], [("M00-01", "Done"), ("M00-02", "In progress"),
                                                          ("M00-03", "Draft")])
        self.assertEqual(st[0].depends, [])
        self.assertEqual(st[2].depends, ["M00-01", "M00-02"])
        self.assertEqual(ms[0].counts(), {"Draft": 1, "Ready": 0, "In progress": 1, "Done": 1})

    def test_agents_attached_to_stories(self):
        sd = self.root / "status"
        emit("m00", "M00-02", "progress", "workflow yaml", status_dir=sd)
        emit("helper", "M00-03", "done", "config done", status_dir=sd)
        mon = Monitor()
        for ln in DirTailer(sd).poll():
            mon.ingest_line(ln.text, ln.source, ln.lineno)
        ms = load_backlog(self.project)
        attach_agents(ms, mon)
        by = {s.id: s for s in ms[0].stories}
        self.assertEqual(by["M00-02"].agents, ["m00"])
        self.assertEqual(by["M00-03"].agents, [])  # finished agents aren't "on" it
        self.assertEqual(by["M00-03"].reported_done_by, ["helper"])
        text = "\n".join(render_backlog(self.project, ms))
        self.assertIn("M00-02   In progress  CI  <- m00", text)
        self.assertIn("(reported done by helper)", text)
        self.assertNotIn("M00-01   Done", text)  # done stories are summarized, not listed
        text.encode("ascii")


class IssuesPlanTests(_Fixture):
    def read(self, path):
        return Path(path).read_text(encoding="utf-8")

    def issue_for(self, story, number, **over):
        iss = {"number": number, "title": f"{story.id} · {story.title}",
               "body": render_body(story, self.project, self.read(story.path)), "state": "OPEN",
               "labels": [{"name": "story"}, {"name": "status:" + story.status.lower().replace(" ", "-")}],
               "milestone": {"title": "M00 · Foundation"}}
        iss.update(over)
        return iss

    def test_empty_repo_plans_milestone_and_creates(self):
        acts = plan(self.project, load_backlog(self.project), [], [])
        kinds = [(a.kind, a.target) for a in acts]
        self.assertEqual(kinds, [("create-milestone", "M00"), ("create", "M00-01"), ("create", "M00-02"),
                                 ("create", "M00-03")])
        self.assertIn("then close (Done)", acts[1].detail)

    def test_in_sync_is_ok_and_changes_are_detected(self):
        ms = load_backlog(self.project)
        s1, s2, s3 = ms[0].stories
        issues = [self.issue_for(s1, 1, state="CLOSED"), self.issue_for(s2, 2),
                  self.issue_for(s3, 3, title="old title", labels=[{"name": "story"}, {"name": "status:ready"},
                                                                   {"name": "help wanted"}])]
        acts = {a.target: a for a in plan(self.project, ms, issues, ["M00 · Foundation"])}
        self.assertEqual(acts["M00-01"].kind, "ok")
        self.assertEqual(acts["M00-02"].kind, "ok")
        self.assertEqual(acts["M00-03"].kind, "update")
        self.assertIn("title", acts["M00-03"].detail)
        self.assertIn("status:draft", acts["M00-03"].detail)
        self.assertNotIn("help wanted", acts["M00-03"].detail)  # unmanaged labels are left alone

    def test_close_reopen_and_body_change(self):
        ms = load_backlog(self.project)
        s1, s2, _ = ms[0].stories
        stale = self.issue_for(s2, 2)
        content, _, _ = split_marker(stale["body"])
        old = content.replace("As a developer", "As an old developer")
        stale["body"] = old + f"\n\n<!-- agent-monitor:story=M00-02 sha={body_sha(old)} -->"
        acts = plan(self.project, ms, [self.issue_for(s1, 1), stale], ["M00 · Foundation"])
        got = {(a.kind, a.target) for a in acts}
        self.assertIn(("close", "M00-01"), got)
        self.assertIn(("update", "M00-02"), got)

    def test_edit_on_github_is_a_conflict_not_overwritten(self):
        ms = load_backlog(self.project)
        s2 = ms[0].stories[1]
        iss = self.issue_for(s2, 2)
        iss["body"] = iss["body"].replace("As a developer", "As a developer (edited on GitHub)")
        acts = {a.target: a for a in plan(self.project, ms, [iss], [])}
        self.assertEqual(acts["M00-02"].kind, "conflict")

    def test_orphan_issue_reported(self):
        ms = load_backlog(self.project)
        body = "x\n\n<!-- agent-monitor:story=M00-99 sha=" + "0" * 40 + " -->"
        acts = plan(self.project, ms, [{"number": 9, "title": "gone", "body": body, "state": "OPEN"}], [])
        self.assertIn(("orphan", "#9"), {(a.kind, a.target) for a in acts})


if __name__ == "__main__":
    unittest.main()
