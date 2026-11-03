# Live sCulpt editor control

The Main Edit development build exposes a local, same-user endpoint for the
active native document. `tools/sculpt_mcp.py` supplies MCP over stdio and a small
deterministic CLI. It does not simulate mouse clicks or edit files behind the
running document. Commands execute on the SDL thread through existing layout,
working-view, camera, file-session and Vulkan capture APIs.

The process publishes `/tmp/sculpt-agent-UID-PID.json` and a Unix socket with mode
0600. The adapter verifies owner, permissions, socket identity and live PID. It
selects automatically only when exactly one live endpoint exists; otherwise use
`--endpoint /tmp/sculpt-agent-UID-PID.json`. No TCP listener or credentials are
introduced. Idle transport waits in a separate thread and wakes SDL for requests.

Tools: `sculpt_state`, `sculpt_inspect`, `sculpt_snapshot`, `sculpt_save_as`,
`sculpt_open`, `sculpt_view`, `sculpt_capture`, and `sculpt_quit`.

Read state first. Mutations require `expected_revision` and a unique
`request_id`. The revision is a content token for the native serialized document,
not a cryptographic authentication signature. Eight recent successful mutation
receipts are retained in process memory: exact same-byte retries reuse their
response; conflicting reuse is refused. Receipts do not survive process exit.
After an uncertain result, read current state and inspect the same destination;
do not blindly submit under a new identity.

Snapshots include current unsaved native geometry without changing the active
document or its dirty state. Save As creates a new file and establishes it as the
active saved document. Both refuse existing paths. Open and Quit refuse unsaved
edits and active drafts. Snapshot is not a capture of uncommitted text-field or
camera-path drafts. Native Open clears the old editor history, as ordinary Open
does, so preserve a snapshot before switching. Original source files are never
implicitly overwritten.

View applies a saved working view, switches to opaque Material presentation,
frames the relevant collection, and preserves native geometry and Undo. Existing
whole-van working views are also supported. Optional `eye_m` and `target_m` set a
temporary inspection camera; both must be valid finite three-element vectors.
Capture schedules a real Vulkan frame to a new absolute BMP path. Its immediate
reply means scheduled; verify file completion before claiming a delivered image.

Example deterministic read:

```
python3 -B tools/sculpt_mcp.py --call state
```

Codex registration uses the maintained `codex mcp add` CLI. Registration and
live application attachment are separate checks. The current chat can exercise
the same MCP JSON-RPC server through stdio even before its tool inventory reloads.

Qualification: AgentControl native tests cover dirty/revision/overwrite/retry
guards and atomic invalid-camera refusal. Actual MCP initialization, catalog,
native load, three view applications, collection inventory, same-byte retry,
request-ID collision and Vulkan captures are retained in
`_private_workspace_artifacts/sculpt-live-control-20261102/attempt-02/`.
This is a bounded document/view control surface, not a promise that every editor
feature has an MCP command. Existing typed offline authoring remains available.
