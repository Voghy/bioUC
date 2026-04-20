# File Naming Convention

All dataset files follow the format:

`SXX_task_protocol_trial.csv`

## Components

- `SXX`: dataset-level anonymized `subject_id` (e.g., `S01`, `S02`)
- `task`: movement task
  - `gait`
  - `stair_ascent`
  - `stair_descent`
- `protocol`: experimental protocol
  - `10MWT` (10 Meter Walking Test)
  - `9SAD` (Nine Stairs Ascent and Descent Test)
- `trial`: repetition number (two digits, e.g., `01`, `02`, `03`)

## Subject ID Scope

- `SXX` values are global subject identifiers for the entire released dataset.
- The same `SXX` value refers to the same participant across `gait`, `stair_ascent`, and `stair_descent`.
- Use `data/metadata/subject_key.csv` to map each `subject_id` to subject initials and task availability.

## Examples

- `S02_gait_10MWT_01.csv`
- `S02_gait_10MWT_02.csv`
- `S02_stair_ascent_9SAD_01.csv`
- `S02_stair_descent_9SAD_01.csv`

## Notes

- Task names use lowercase.
- Protocol codes are uppercase.
- Underscore (`_`) is used as separator.
- No spaces or special characters are allowed.
