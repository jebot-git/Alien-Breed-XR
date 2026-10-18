# Releasing

Alien Breed 3D I releases are Windows x64 and Linux x64 builds produced by GitHub Actions.

## Create a Stable Release

1. Update the numeric CMake project version in `CMakeLists.txt` when the release series changes.
2. Commit the release-ready source.
3. Create and push a stable tag:

```bash
git tag v1.0.0
git push origin v1.0.0
```

Tags matching `v*.*.*` run `.github/workflows/release.yml`. The workflow builds clean Windows x64 and Linux x64 Release binaries, packages the staged runtime files, creates `SHA256SUMS.txt` covering both archives, creates GitHub artifact attestations where supported, and publishes a GitHub release after both builds succeed.

Tags with a prerelease suffix, such as `v1.0.0-beta.1` or `v1.0.0-rc.1`, use the same workflow but are marked as GitHub prereleases and are not marked latest.

Release assets appear on the GitHub Release for the tag:

- `alien-breed-3d-i-<tag>-windows-x64.zip`
- `alien-breed-3d-i-<tag>-linux-x64.tar.gz`
- `SHA256SUMS.txt`

The bundled `VERIFY_RELEASE.md` explains how players can verify checksums and provenance.

## CI

`.github/workflows/ci.yml` runs on pushes to `main` and on pull requests. It configures and builds Windows and Linux Release targets, checks the Windows runtime output, and uploads the Linux package as the `linux-x64` workflow artifact.

Linux CI and releases use Ubuntu 22.04, the `linux-release` CMake preset, and the same pinned SDL2 source as desktop Windows builds. The Linux `.tar.gz` preserves executable permissions and contains one top-level directory with `ab3d1`, `ab3d.ini`, `README.txt`, `VERIFY_RELEASE.md`, `fonts/`, and `data/`. System audio/display libraries are supplied by the player's Linux installation.

Local Linux packaging after building:

```bash
python3 tools/package_release.py --platform linux-x64 --build-dir build/linux-release --version v1.0.0 --output-dir dist
```

These workflows apply to tags containing the Linux build setup. Existing release tags remain unchanged.

## Optional Windows Code Signing

Signing is not required for the release workflow. To enable it later, add these repository or environment secrets:

- `WINDOWS_SIGNING_CERT_BASE64`: Base64-encoded `.pfx` signing certificate.
- `WINDOWS_SIGNING_CERT_PASSWORD`: Password for the `.pfx` certificate.

When both secrets are present, the release workflow signs `build/Release/ab3d1.exe` before packaging it.

## Security Notes

Release publishing only runs for tags in the `retro-foundry/Alien-Breed-HD` repository. The workflow uses the built-in `GITHUB_TOKEN` and grants write permissions only to the publish job. Keep release tags protected in GitHub repository settings so only trusted maintainers can create or move release tags.
