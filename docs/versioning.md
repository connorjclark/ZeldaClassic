# Versioning

ZC's versioning is a variation of [semver](https://semver.org/).

Version strings consist of three numbers: `major.minor.patch`.

* Stable releases are `major.minor.0`. Ex: `3.1.0` is the second stable release of major version 3.
* Each nightly release increments the patch component of the last stable release, and also includes the date. Ex: `3.1.1-nightly+2024-01-01` is the first nightly after the `3.1.0` stable release.
* We aim to release a stable version on every first Monday of the month.

## Development

`src/base/version.cpp` is where version information is stored. No version string exists in source, but is instead configured at build time.

For local/CI test builds: the most recent release tag is used. See `scripts/generate_version_fallback_header.py` and the `version_fallback` build target.

For official builds via GitHub Actions: The "Release" action lets you choose between a "stable" or a "nightly" release, and `.github/workflows/print-release-name-vars.py` determines what the next version should be accordingly.

Various legacy versioning fields still exist in the code base and in the quest file format. They are all frozen now, and won't be updated anymore:

* `zelda_version_string` contains the full version string (aka the git tag)
* `zelda_version` / `ZC_VERSION` is deprecated (hex value, forever fixed at `0x255`)
*  The major, minor and patch components replace the previous "main, second, third" fields, but the "fourth" field is now deprecated and always zero.
* `new_version_id_{alpha,beta,gamma,release}` fields are deprecated and now always 0.
