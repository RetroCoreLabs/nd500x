# Documentation site

The site at https://retrocorelabs.github.io/nd500x/ is built by MkDocs +
Material from the Markdown files git tracks. Nothing on the site is typed by
hand twice: the page tree, the instruction index and the navigation are
written by `tools/gen_docs_site.py` from `git ls-files` and
`docs/instructions/instructions.json`, following the policy in that script's
header (what is on the site, in which section, and what stays off).

## Build locally

From the repository root:

```bash
python3 -m venv ~/venvs/nd500x-site
~/venvs/nd500x-site/bin/pip install -r docs-site/requirements.txt
python3 tools/gen_docs_site.py                 # writes build-site/ (gitignored)
~/venvs/nd500x-site/bin/mkdocs build --strict -f build-site/mkdocs.yml -d ~/nd500x-site-out
# or:  ~/venvs/nd500x-site/bin/mkdocs serve -f build-site/mkdocs.yml
```

The build must end with zero warnings. `--strict` turns a broken link into a
failure.

## Consistency gate

```bash
python3 tools/gen_docs_site.py --check
```

Fails if a tracked `.md` file is not covered by the policy (add it to a section
or to the off-site list in the script) or if a per-instruction page named in
`instructions.json` has no file under `docs/instructions/asm/`. CI runs this
before every build.

## Regenerating the figures

```bash
python3 tools/gen_isa_figures.py               # writes docs/figures/*.svg
```

Run it after `docs/instructions/instructions.json` changes; the figures are
committed because they are also read on github.com.

## Files

| File | Purpose |
|---|---|
| `docs-site/mkdocs.yml` | theme, extensions, validation; the committed base config |
| `docs-site/requirements.txt` | pinned Python packages |
| `tools/gen_docs_site.py` | policy + generator; writes `build-site/docs/` and `build-site/mkdocs.yml` (`INHERIT: ../docs-site/mkdocs.yml` + `nav:`) |
| `tools/gen_isa_figures.py` | the four instruction-set figures in `docs/figures/` |
| `.github/workflows/docs.yml` | builds and deploys on every push to `main` |
