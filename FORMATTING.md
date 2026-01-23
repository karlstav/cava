# Local formatting (clang-format)

This repo has local helper scripts for clang-format.

## Commands

- **Format files (modify in-place)**

  `./scripts/clang-format.sh`

- **Check formatting (no changes; exits non-zero on mismatch)**

  `./scripts/clang-format-lint.sh`

## What files are included

- Only tracked `*.c` and `*.h` files.
- `third_party/` is excluded.

## Which clang-format version is used

Recommended: **clang-format 9**.

Locally, the scripts try to match clang-format 9 as closely as possible:

- `clang-format-9` if installed.
- Otherwise, if `docker` is available, run a `doozy/clang-format-lint:0.5` container (clang-format 9).
- Otherwise fall back to whatever `clang-format` is installed on your machine.

If you see a warning like:

`Warning: scripts prefer clang-format 9 ...`

you can avoid CI/local differences by installing `clang-format-9` (preferred) or installing `docker`.

## Docker notes (Linux)

If you want to use the Docker-based clang-format 9 (CI-matching) path:

1. Start the Docker daemon:

   `sudo systemctl enable --now docker`

2. Allow your user to access the Docker socket (optional, but recommended):

   `sudo usermod -aG docker $USER`

   Then log out and log back in (or reboot) so the new group membership applies.

If Docker is installed but you see:

`permission denied while trying to connect to the docker API at unix:///var/run/docker.sock`

it means you are not allowed to access the Docker socket yet (step 2 above).

If Docker fails to pull the image with something like:

`net/http: TLS handshake timeout`

that is a network/registry connectivity problem. In that situation the scripts will fall back to your locally installed `clang-format` and print a warning.

### File ownership

Do not run `make format` with `sudo`. If you do, files may become owned by `root`.

The Docker-based formatter is configured to run as your current user, so running `make format` normally should keep file ownership correct.

If some files already became `root`-owned, from the repo root you can restore ownership with:

`sudo chown -R $USER:$USER .`

## What does `make: *** ... format-check Error 1` mean?

Exit code `1` means: at least one file is not formatted according to clang-format.

To fix:

1. Run `./scripts/clang-format.sh`
2. Re-run `./scripts/clang-format-lint.sh`

If it still fails, it usually means your local `clang-format` version differs from CI. Install `clang-format-9` or use `docker` so local matches CI.

## Tip: flushing terminal escape sequences

When printing terminal control sequences (for example clearing the screen), it can help to flush stdout so the sequence takes effect immediately before further output:

```c
printf("\033[2J");
fflush(stdout);
```
