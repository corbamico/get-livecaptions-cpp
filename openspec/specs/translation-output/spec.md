### Requirement: Translated output routes to the configured output sink
When `--translate` is specified, translated caption lines SHALL be written to the same output destination as raw captions (the file specified by `--output`, or stdout when `--output -` is used). Translated output SHALL NOT be written to stderr.

#### Scenario: Translation to file
- **WHEN** the user runs with `--translate es --output captions.txt`
- **THEN** translated lines appear in `captions.txt` alongside raw captions, and stderr receives no translated text

#### Scenario: Translation to stdout
- **WHEN** the user runs with `--translate es --output -`
- **THEN** translated lines appear on stdout alongside raw captions

#### Scenario: No translation configured
- **WHEN** the user runs without `--translate`
- **THEN** no translated output is written anywhere

### Requirement: Translation errors are reported to stderr regardless of output sink
When a translation call fails, the error message SHALL be written to `stderr` so it does not corrupt the output file.

#### Scenario: LibreTranslate unreachable
- **WHEN** LibreTranslate is unreachable and `--translate` is active
- **THEN** an error message appears on stderr, and the output file contains no corrupted translation output

#### Scenario: LibreTranslate response timeout
- **WHEN** LibreTranslate does not respond within 10 seconds
- **THEN** the program logs a timeout error to stderr and resumes the poll loop within ~10 seconds
