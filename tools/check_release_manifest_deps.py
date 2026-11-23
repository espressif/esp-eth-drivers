#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 Espressif Systems (Shanghai) CO LTD
# SPDX-License-Identifier: Apache-2.0

"""Suggest manifest updates for dependency ranges that exclude a release bump."""

import argparse
import json
import os
import subprocess
import sys

from collections.abc import Iterator
from dataclasses import dataclass
from html import escape
from pathlib import Path

import gitlab
import yaml

from idf_component_tools.semver import SimpleSpec
from idf_component_tools.semver import Version

RELEASE_MANIFEST = '.release-please-manifest.json'
COMMENT_MARKER = '<!-- release-manifest-check -->'
TOKEN_VARIABLE = 'ETH_CI_TOKEN'  # noqa: S105 - environment variable name
SKIP_DIRS = {'.git', '.cache', 'build', 'managed_components'}


@dataclass(frozen=True)
class Finding:
    manifest: str
    dependency: str
    location: str
    constraint: str
    released_version: str


def released_versions(root: Path, base_ref: str) -> dict[str, str]:
    """Read only new or increased versions in the release-please manifest."""
    result = subprocess.run(  # noqa: S603 - arguments passed directly to git
        ['git', 'show', f'{base_ref}:{RELEASE_MANIFEST}'],  # noqa: S607
        cwd=root, check=True, capture_output=True, text=True,
    )
    base = json.loads(result.stdout)
    head = json.loads((root / RELEASE_MANIFEST).read_text(encoding='utf-8'))
    return {
        component: version
        for component, version in sorted(head.items())
        if component not in base or Version(version) > Version(base[component])
    }


def version_specs(entry: object) -> Iterator[tuple[str, str]]:
    """Include conditional versions without assuming a particular IDF/Kconfig setup."""
    if isinstance(entry, (str, int, float)) and not isinstance(entry, bool):
        yield 'version', str(entry)
    elif isinstance(entry, dict):
        # Git versions are refs, and local dependencies do not use the registry.
        # override_path is deliberately included: published consumers use the range.
        if 'git' in entry or 'path' in entry:
            return
        if entry.get('registry_url', 'https://components.espressif.com').rstrip('/') != 'https://components.espressif.com':
            return
        if 'version' in entry:
            yield 'version', str(entry['version'])
        for key in ('rules', 'matches'):
            for index, rule in enumerate(entry.get(key, [])):
                if isinstance(rule, dict) and 'version' in rule:
                    location = f'{key}[{index}].version (if: {rule.get("if", "always")})'
                    yield location, str(rule['version'])


def find_excluded_versions(root: Path, releases: dict[str, str]) -> list[Finding]:
    findings: list[Finding] = []
    if not releases:
        return findings
    for manifest in sorted(root.rglob('idf_component.yml')):
        relative = manifest.relative_to(root)
        if SKIP_DIRS.intersection(relative.parts):
            continue
        data = yaml.safe_load(manifest.read_text(encoding='utf-8')) or {}
        for dependency, entry in data.get('dependencies', {}).items():
            component = dependency.removeprefix('espressif/')
            if component not in releases:
                continue
            for location, constraint in version_specs(entry):
                try:
                    accepts = SimpleSpec(constraint).match(Version(releases[component]))
                except ValueError as exc:
                    raise ValueError(f'{relative}: {dependency} {location}: {exc}') from exc
                if not accepts:
                    findings.append(Finding(relative.as_posix(), dependency, location, constraint, releases[component]))
    return findings


def markdown_code(value: str) -> str:
    # Keep YAML conditions containing pipes or backticks inside one table cell.
    value = escape(value).replace('|', '&#124;').replace('`', '&#96;').replace('\n', ' ')
    return f'<code>{value}</code>'


def format_comment(findings: list[Finding]) -> str:
    lines = [COMMENT_MARKER, '### Release dependency suggestions', '']
    if findings:
        lines += [
            'These manifest constraints exclude versions being released by this MR:', '',
            '| Manifest | Dependency | Location / condition | Current constraint | Release version |',
            '| --- | --- | --- | --- | --- |',
        ]
        for item in findings:
            cells = (item.manifest, item.dependency, item.location, item.constraint, item.released_version)
            lines.append('| ' + ' | '.join(markdown_code(cell) for cell in cells) + ' |')
        lines += [
            '', 'Consider updating the affected constraints if these consumers should use the new release.',
            'Conditional constraints are listed for review regardless of the current IDF/Kconfig configuration.',
            'Local override paths can hide these constraints during repository builds.', '',
            'This is advisory only. The maintainer decides whether to update each constraint and release the consuming component.',
        ]
    else:
        lines.append('No manifest constraints exclude the component versions being released by this MR. Previous suggestions are resolved.')
    return '\n'.join(lines) + '\n'


def update_comment(client: gitlab.Gitlab, project: str, iid: str, body: str, has_findings: bool) -> None:
    mr = client.projects.get(project, lazy=True).mergerequests.get(iid)
    # An older pipeline must not overwrite the report for a newer MR revision.
    source_sha = os.environ.get('CI_MERGE_REQUEST_SOURCE_BRANCH_SHA') or os.environ.get('CI_COMMIT_SHA')
    if source_sha and mr.sha != source_sha:
        print('Skipping comment: the MR has a newer source revision.')
        return
    client.auth()
    user = client.user
    if user is None:
        raise ValueError('GitLab authentication did not return a user.')
    existing = next(
        (note for note in mr.notes.list(iterator=True, order_by='created_at', sort='desc')
         if note.author['id'] == user.id and not note.system and note.body.startswith(COMMENT_MARKER)),
        None,
    )
    if existing:
        if existing.body == body:
            print('The existing advisory comment is up to date.')
            return
        existing.body = body
        existing.save()
        print('Updated the existing advisory comment.')
    elif has_findings:
        mr.notes.create({'body': body})
        print('Created an advisory comment.')
    else:
        print('No suggestions to post.')


def post_comment(body: str, has_findings: bool) -> None:
    token = os.environ.get(TOKEN_VARIABLE)
    if not token:
        print(f'warning: {TOKEN_VARIABLE} is unavailable; report saved, MR comment skipped.', file=sys.stderr)
        return
    server_url = os.environ['CI_SERVER_URL']
    project = os.environ['CI_PROJECT_ID']
    iid = os.environ['CI_MERGE_REQUEST_IID']
    with gitlab.Gitlab(server_url, private_token=token, timeout=30, per_page=100) as client:
        update_comment(client, project, iid, body, has_findings)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--repo-root', type=Path, default=Path('.'))
    parser.add_argument('--base-ref', required=True, help='MR diff base SHA, or a local Git ref for a dry run.')
    parser.add_argument('--report', type=Path, default=Path('release-manifest-report.md'))
    parser.add_argument('--post-comment', action='store_true', help=f'Use {TOKEN_VARIABLE} to create/update an advisory MR note.')
    args = parser.parse_args()
    try:
        releases = released_versions(args.repo_root, args.base_ref)
        print(f'Component releases in this MR: {json.dumps(releases, sort_keys=True)}')
        findings = find_excluded_versions(args.repo_root, releases)
        body = format_comment(findings)
        args.report.write_text(body, encoding='utf-8')
        print(body)
        if args.post_comment:
            post_comment(body, bool(findings))
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError, yaml.YAMLError, gitlab.exceptions.GitlabError) as exc:
        print(f'error: release manifest check failed: {exc}', file=sys.stderr)
        return 1
    return 0  # Excluded versions are suggestions and never fail the check.


if __name__ == '__main__':
    sys.exit(main())
