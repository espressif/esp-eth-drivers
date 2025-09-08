# Contributing Guidelines

Welcome to the **Additional Ethernet Drivers** project! We appreciate your interest in contributing. Whether you're reporting an issue, proposing a feature, or submitting a pull request (PR), your contributions are highly valued.

## Submitting a Pull Request (PR)

- **Fork the Repository**
  - Start by forking the [Additional Ethernet Drivers repository](https://github.com/espressif/esp-eth-drivers) on GitHub to make your changes.

- **Install Pre-commit Hooks**
  - Set up pre-commit hooks to ensure consistent code style and linting:

    ```bash
    pip install pre-commit
    pre-commit install-hooks
    ```

- **Commit Message Format**
  - Follow the [Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/) specification when writing commit messages.
  - Commit message title format:
    - Template: `[type]([scope]): Message`
    - Example: `feat(ksz8863): Add port tail tagging` or `fix(lan867x): Correct datatype in lan867x_custom_ioctl`

> [!TIP]
> You can use [Commitizen](https://commitizen-tools.github.io/commitizen/) to help you format your commits correctly.
> After installing Commitizen (see [official instructions](https://commitizen-tools.github.io/commitizen/#installation)), stage your changes and run:
>
> ```bash
> cz commit
> ```
>
> Then follow the prompts provided by Commitizen.

- **Send a Pull Request**
  - Submit your PR and collaborate with the maintainers through the review process. Please be patient — multiple review rounds and adjustments may be required.
  - For quicker merging, focus your contribution on a single feature or topic. Larger or multi-topic contributions may take longer to review.

## Merging the Pull Request (PR)

The content of the PR is not directly merged on Github but it is synchronized to Espressif's internal mirror repository using [GitHub PR to Gitlab MR Sync](https://github.com/espressif/sync-pr-to-gitlab) Github action. Additional internal review may be performed and the changes are tested on target using internal runners.

## Git History Strategy

Linear git commit history is used in this repository. To keep the history clean, squash your commits from the development phase into the smallest amount of logical units prior submitting the PR. For example, if you update a single component, you use single commit. If you update multiple components and with logically unrelated features, you can use separate commit for each component. Avoid using the following patterns:
```
commit 1: feat(ksz8863): added a new cool feature
commit 2: fix(ksz8863): fixed a bug introduced in commit 1
```

## Release Process

We use [Release Please](https://github.com/googleapis/release-please/tree/main) to automate the release process. It automatically generates the changelog by parsing the Git history (hence the requirement for Conventional Commit messages), bumps component versions, and creates GitHub releases and tags.

**How it works:**
1. **Automatic version bump PRs**: They are contiguously created and updated with each releasable version commit automatically by Release Please action.
2. **Component is ready for release**: Once component is ready to be released, the bump PR is merged by repository maintainer using the same process as described in [Merging the Pull Request section](#merging-the-pull-request-pr). Since linear history, which may require rebase, is used, internal repository `master` branch is temporarily locked during the merge process to bump commits SHAs match and so release step work correctly. DO NOT rebase during this step!
3. **Github release and tag**: Release Please action automatically creates Github Release and adds version tag to the bump PR commit.
