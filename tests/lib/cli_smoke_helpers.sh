#!/usr/bin/env bash

ld_cli_smoke_label() {
  printf '%s' "${LD_CLI_SMOKE_LABEL:-line-cli-smoke}"
}

ld_cli_smoke_error() {
  echo "[$(ld_cli_smoke_label)] ERROR: $*" >&2
}

ld_cli_smoke_reset_dir() {
  local path="$1"
  rm -rf "$path"
  mkdir -p "$path"
}

ld_cli_smoke_require_pattern() {
  local pattern="$1"
  local path="$2"
  if ! rg -q "$pattern" "$path"; then
    ld_cli_smoke_error "expected pattern '$pattern' in $path"
    exit 1
  fi
}

ld_cli_smoke_require_file() {
  local path="$1"
  if [[ ! -f "$path" ]]; then
    ld_cli_smoke_error "expected file $path"
    exit 1
  fi
}

ld_cli_smoke_expect_failure() {
  local name="$1"
  local err_path="$2"
  shift 2
  if "$@" 2>"$err_path"; then
    ld_cli_smoke_error "$name failure unexpectedly passed"
    exit 1
  fi
}
