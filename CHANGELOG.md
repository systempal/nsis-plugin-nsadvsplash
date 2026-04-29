# Changelog

Tutte le modifiche rilevanti a questo progetto sono documentate in questo file.

Il formato segue [Keep a Changelog](https://keepachangelog.com/it/1.0.0/),
e il progetto aderisce al [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.1] — 2026-04-29

### Changed
- Build script completamente riscritto: Colors, Spinner, BuildConfig, build parallelo con ThreadPoolExecutor, CPU info, output colorato OK/FAIL
- CI: rimosso flag `--verbose` (non esistente) in favore di `--verbosity minimal`

## [1.0.0] — 2026-04-29

### Added
- Prima release come repository indipendente
- Build script unificato (`build_plugin.py`) con CLI canonica
- CI/CD via GitHub Actions (mirror automatico Gitea → GitHub)
- Documentazione completa (README, CONTRIBUTING, SECURITY)

[Unreleased]: https://gitea.emulab.it/Simone/nsis-plugin-nsadvsplash/compare/v1.0.1...HEAD
[1.0.1]: https://gitea.emulab.it/Simone/nsis-plugin-nsadvsplash/compare/v1.0.0...v1.0.1
[1.0.0]: https://gitea.emulab.it/Simone/nsis-plugin-nsadvsplash/releases/tag/v1.0.0
