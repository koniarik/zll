# CI Pipeline Failures

When a GitHub Actions pipeline failure is mentioned or needs investigation, use
`scripts/ci_results.py` to fetch the results:

```sh
python3 scripts/ci_results.py
```

This prints the status of every job in the latest runs of `test.yml` and
`clang-tidy.yml`, and automatically shows the tail of the log for any failed
job. Use `--ref <branch>` to filter by branch, or `--workflow <file>` to
inspect a single workflow.

Authentication uses `GITHUB_TOKEN` env var or `gh auth login` as a fallback.
