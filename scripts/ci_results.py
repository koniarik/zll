#!/usr/bin/env python3
"""Fetch and display CI pipeline results from GitHub Actions.

Usage:
    python scripts/ci_results.py [--ref <branch/sha>] [--workflow <name>] [--logs]

Requires the `gh` CLI to be authenticated, or a GITHUB_TOKEN env var.
"""

import argparse
import json
import os
import subprocess
import sys
import urllib.error
import urllib.request

REPO = "koniarik/zll"
WORKFLOWS = ["test.yml", "clang-tidy.yml"]

STATUS_ICON = {
    "success": "✓",
    "failure": "✗",
    "cancelled": "⊘",
    "skipped": "—",
    "in_progress": "⟳",
    "queued": "…",
    "waiting": "…",
    None: "?",
}


def _token() -> str:
    token = os.environ.get("GITHUB_TOKEN", "")
    if token:
        return token
    try:
        result = subprocess.run(
            ["gh", "auth", "token"], capture_output=True, text=True, check=True
        )
        return result.stdout.strip()
    except (subprocess.CalledProcessError, FileNotFoundError):
        print("error: no GITHUB_TOKEN and 'gh auth token' failed", file=sys.stderr)
        sys.exit(1)


def _api(path: str, token: str) -> dict:
    url = f"https://api.github.com{path}"
    req = urllib.request.Request(
        url,
        headers={
            "Authorization": f"Bearer {token}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )
    try:
        with urllib.request.urlopen(req) as resp:
            return json.loads(resp.read())
    except urllib.error.HTTPError as e:
        body = e.read().decode()
        print(f"error: GitHub API {url} → {e.code}: {body}", file=sys.stderr)
        sys.exit(1)


def get_latest_run(workflow: str, ref: str | None, token: str) -> dict | None:
    path = f"/repos/{REPO}/actions/workflows/{workflow}/runs?per_page=5"
    if ref:
        path += f"&branch={urllib.parse.quote(ref)}"
    data = _api(path, token)
    runs = data.get("workflow_runs", [])
    return runs[0] if runs else None


def get_jobs(run_id: int, token: str) -> list[dict]:
    data = _api(f"/repos/{REPO}/actions/runs/{run_id}/jobs?per_page=100", token)
    return data.get("jobs", [])


def fetch_log(job_id: int, token: str) -> str:
    """Fetch the full log for a job and return its text content.

    GitHub API returns a 302 redirect to a blob-storage URL. The blob URL
    must be fetched without the Authorization header or it will be rejected.
    """
    api_url = f"https://api.github.com/repos/{REPO}/actions/jobs/{job_id}/logs"
    req = urllib.request.Request(
        api_url,
        headers={
            "Authorization": f"Bearer {token}",
            "Accept": "application/vnd.github+json",
            "X-GitHub-Api-Version": "2022-11-28",
        },
    )

    # Intercept the redirect so we can strip auth before following it.
    class _NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, req, fp, code, msg, headers, newurl):
            return None

    opener = urllib.request.build_opener(_NoRedirect)
    try:
        with opener.open(req) as resp:
            return resp.read().decode(errors="replace")
    except urllib.error.HTTPError as e:
        if e.code in (301, 302, 303, 307, 308):
            blob_url = e.headers.get("Location", "")
            if not blob_url:
                return "(log unavailable: no redirect location)"
            try:
                with urllib.request.urlopen(blob_url) as resp:
                    return resp.read().decode(errors="replace")
            except urllib.error.HTTPError as e2:
                return f"(log unavailable: blob fetch failed {e2.code})"
        return f"(log unavailable: {e.code})"


def print_run(workflow: str, run: dict, jobs: list[dict], token: str):
    icon = STATUS_ICON.get(run.get("conclusion") or run.get("status"))
    print(f"\n{'─'*60}")
    print(f"{icon}  {workflow}  #{run['run_number']}  {run['status']}", end="")
    if run.get("conclusion"):
        print(f" / {run['conclusion']}", end="")
    print(f"\n   branch: {run['head_branch']}  sha: {run['head_sha'][:10]}")
    print(f"   {run['html_url']}")

    if not jobs:
        print("   (no jobs found)")
        return

    for job in jobs:
        conclusion = job.get("conclusion") or job.get("status")
        icon = STATUS_ICON.get(conclusion)
        print(f"   {icon}  {job['name']}")
        if conclusion == "failure":
            log = fetch_log(job["id"], token)
            print()
            for line in log.splitlines()[-60:]:
                print(f"       {line}")
            print()


def main():
    import urllib.parse  # needed inside get_latest_run

    parser = argparse.ArgumentParser(description="Show GitHub Actions CI results")
    parser.add_argument("--ref", metavar="BRANCH", help="Branch or SHA to filter by")
    parser.add_argument(
        "--workflow",
        metavar="FILE",
        help=f"Workflow file name (default: all). One of: {', '.join(WORKFLOWS)}",
    )
    args = parser.parse_args()

    token = _token()
    workflows = [args.workflow] if args.workflow else WORKFLOWS

    any_failure = False
    for wf in workflows:
        run = get_latest_run(wf, args.ref, token)
        if run is None:
            print(f"\n{wf}: no runs found")
            continue
        jobs = get_jobs(run["id"], token)
        print_run(wf, run, jobs, token)
        if run.get("conclusion") == "failure":
            any_failure = True

    sys.exit(1 if any_failure else 0)


if __name__ == "__main__":
    main()
