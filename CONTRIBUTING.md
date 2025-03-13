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

## Release Process

We use [Release Please](https://github.com/googleapis/release-please/tree/main) to automate the release process. It automatically generates the changelog by parsing the Git history (hence the requirement for Conventional Commit messages), bumps component versions, and creates GitHub releases and tags.

To release a new version of a component, the component maintainer must:

1. Ensure the build and tests pass on the latest commit.
2. Export their GitHub token to the `GITHUB_TOKEN` environment variable.

    ```sh
    export GITHUB_TOKEN=your_github_token
    ```

3. Bump the version using the provided script:

    ```sh
    ./tools/version_bump.sh
    ```
    or if you want to version bump just single component, navigate to the component's folder and run:
    ```sh
    ../tools/version_bump.sh
    ```

> [!NOTE]
> When run from the repository root, the script will bump the version of **all** components with pending changes.
> To bump only a specific component, run the script from that component’s directory.

> [!NOTE]
> You can perform a dry run by passing the `--dry-run` argument:
>
> ```bash
> ./tools/version_bump.sh --dry-run
> ```

4. A version bump PR will be created automatically.
5. Wait for approval from the repository maintainer.
