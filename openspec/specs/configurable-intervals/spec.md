### Requirement: Poll interval is configurable via CLI
The program SHALL accept a `--poll-interval <seconds>` argument (integer ≥ 1) that overrides the default 10-second UIAutomation polling cadence.

#### Scenario: Default poll interval
- **WHEN** the user runs the program without `--poll-interval`
- **THEN** the program polls Live Captions every 10 seconds

#### Scenario: Custom poll interval
- **WHEN** the user runs the program with `--poll-interval 5`
- **THEN** the program polls Live Captions every 5 seconds

#### Scenario: Invalid poll interval rejected
- **WHEN** the user passes `--poll-interval 0` or a negative value
- **THEN** the program prints an error and exits with code 1

### Requirement: Flush interval is configurable via CLI
The program SHALL accept a `--flush-interval <seconds>` argument (integer ≥ 1) that overrides the default 60-second file-write cadence.

#### Scenario: Default flush interval
- **WHEN** the user runs the program without `--flush-interval`
- **THEN** captions are flushed to the output file every 60 seconds

#### Scenario: Custom flush interval
- **WHEN** the user runs the program with `--flush-interval 30`
- **THEN** captions are flushed to the output file every 30 seconds

#### Scenario: Invalid flush interval rejected
- **WHEN** the user passes `--flush-interval 0` or a negative value
- **THEN** the program prints an error and exits with code 1
