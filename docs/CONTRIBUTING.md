# Contributing guide

We welcome contributions of all sizes, from documentation improvements to new
features. This guide explains how to get started.

If you have

- solved some [known *issue*](https://github.com/diffkemp/diffkemp/issues) or
- done changes to DiffKemp which you would like to share with the
main repository,

read the following section:

- [Contributing workflow](#workflow)
- [Pull request guidelines](#pull-request-guidelines)
- [Commit guidelines](#commit-guidelines)
- [Other tips](#other-tips)

## Workflow

1. To contribute to DiffKemp, we use *[fork and pull request workflow](https://docs.github.com/en/get-started/exploring-projects-on-github/contributing-to-a-project)*.
2. When creating a pull request, you should check that all *[status
   checks](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/collaborating-on-repositories-with-code-quality-features/about-status-checks)*
   are passing, if they are not, try to investigate the problems and fix them to
   make all status checks pass.
3. After creating the pull request, you can [*request* review from a collaborator](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/proposing-changes-to-your-work-with-pull-requests/requesting-a-pull-request-review).
4. Wait for a review from a collaborator.
5. React to the reviewers' feedback, suggestions, questions – fixing problems
   and improving the pull request until the pull request is *approved* by
   the collaborator/s.
6. We use [*rebase and merge strategy*](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/incorporating-changes-from-a-pull-request/about-pull-request-merges#rebase-and-merge-your-commits)
   for integrating the changes from the *PR* to the *master* branch.

   In case there are [merge conflicts](https://docs.github.com/en/pull-requests/collaborating-with-pull-requests/addressing-merge-conflicts/about-merge-conflicts)
   conflicts between the master branch and the PR's branch, you will need to
   [manually rebase](https://git-scm.com/book/en/v2/Git-Branching-Rebasing)
   the PR branch on the master branch and resolve the conflicts, to make it
   possible to merge your PR.
   You can use [different tools](https://medium.com/@kaltepeter/tools-to-master-merge-conflicts-6d05b21a8ba8)
   for solving these conflicts.

## Pull request guidelines

- A PR should be small, focused, and include a clear description (what, why,
  how).
- A PR should have **clean commit history**:
  - Each commit should be a **logically separated change**.
  - There should **not be "fix" or "small cleanup" commits** – these should be
    either better described or merged (squashed) with some previous commit.

  To clean up the commit history, you can use [Git interactive rebase](https://git-scm.com/book/en/v2/Git-Tools-Rewriting-History).
- All **CI checks should pass** after each commit.

## Commit guidelines

- Each commit should had a **commit message** (not just the title) explaining
  what the change is and why it is useful or necessary.
- Use [**50/72 rule**](https://www.midori-global.com/blog/2018/04/02/git-50-72-rule)
  for commit messages.

## Other tips

- Setup your editor not to remove the trailing newline character from files.
