# LineDrawing Persistent Main Edit Worktree

Last updated: 2026-08-28

## Lane identities

- Canonical source: `<workspace>/line_drawing` on `main`
- Persistent Main Edit source: `<workspace>/_worktrees/line_drawing_main_edit`
  on `codex/line-drawing-main-edit`
- Canonical package: `sCulpt.app`, bundle `com.cosm.sculpt`
- Main Edit package: `sCulpt Main Edit.app`, bundle
  `com.cosm.sculpt.main-edit`
- Canonical runtime/log namespace: `LineDrawing`
- Main Edit runtime/log namespace: `LineDrawing-Main-Edit`

The Main Edit application is a local development artifact. It is not a release
candidate, Registry record, publication, deployment, or version decision.

## Start gate

Before editing, read both lanes and the complete worktree inventory:

```sh
git -C <workspace>/line_drawing status --short --branch
git -C <workspace>/_worktrees/line_drawing_main_edit status --short --branch
git -C <workspace>/line_drawing worktree list --porcelain
git -C <workspace>/line_drawing rev-list --left-right --count \
  main...codex/line-drawing-main-edit
```

Only one writer may own the Main Edit checkout. Existing specialist, release,
repair, or retained-evidence worktrees remain separate and must not be reset,
cleaned, repurposed, or removed to simplify the Main Edit topology.

## Checkpoint gate

Run the narrowest behavior tests first, followed by the broad source and
package gates appropriate to the change:

```sh
git -C <workspace>/_worktrees/line_drawing_main_edit diff --check
make -C <workspace>/_worktrees/line_drawing_main_edit test
make -C <workspace>/_worktrees/line_drawing_main_edit package-desktop-main-edit-self-test
```

The package self-test verifies the generic local-development identity,
packaged binary digest, `main-edit` profile, isolated runtime/log namespaces,
and the distinct app and bundle identifiers. Commit only the bounded owned
change set and only after explicit operator permission.

## Package targets

```sh
make -C <workspace>/_worktrees/line_drawing_main_edit package-desktop-main-edit
make -C <workspace>/_worktrees/line_drawing_main_edit package-desktop-main-edit-self-test
make -C <workspace>/_worktrees/line_drawing_main_edit package-desktop-main-edit-refresh
```

The refresh target is a separate Desktop mutation. It refuses the canonical
Desktop destination and refuses to replace a running Main Edit application.
Building or self-testing the package does not authorize refresh or launch.

## Integration gate

Before canonical adoption:

1. classify any canonical-only commits
2. merge expected canonical drift into Main Edit
3. rerun focused, full source, and Main Edit package gates
4. adopt with a fast-forward when canonical is an ancestor, otherwise use a
   reviewed merge
5. independently read back both commits and cleanliness

Source adoption does not authorize a `VERSION` edit, release package, signing,
notarization, Registry mutation, publication, deployment, or push.

## Retain and recycle gate

Retain the clean persistent Main Edit lane after adoption by default. Recycle
only after proving that it is clean, all commits are reachable, no user-owned
untracked or ignored evidence needs retention, no process owns the checkout or
development application, and all specialist worktrees remain unaffected.
Never force-remove or destructively reset the lane.
