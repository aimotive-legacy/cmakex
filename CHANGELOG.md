Major changes the users probably wants to know about. Most bugfixes, minor changes
are not listed here, see git history.

v1.0, since 2016-10-06
----------------------

- Fix annoying 'branch not implemented' crash (occured when a dep was
  installed and later the clone was removed)
- Added --update=force update-mode.
- Added -q option to supress cmake log for deps (and changed the default
  behaviour to always print cmake log)
- Added `CMAKEX_LOG_GIT` env variable to force displaying all git commands (for
  debugging)
- Added `--update=MODE` option to automatically git-update dependencies.
- Added `--single-build-dir` command-line option
- Added `GIT_TAG_OVERRIDE` option for `add_pkg`/`def_pkg` commands
- Added using `<exe-dir>/default-cmakex-presets.yaml` if CMAKEX_PRESET_FILE
  is not set
- Added `--deps-source`, `--deps-build` and `--deps-install` command-line
  options
- Added manifest generation with `--manifest`
- Added per-config build dir feature and made it default
