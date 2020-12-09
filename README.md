# nt

'at' wrapper for setting one time notification alarms. alarms can be set after
a provided time interval with seconds precision or at a provided time within a
day.

# Dependencies

* at
* notify-send

# Usage

```
nt -h|--help
nt <time-specification> <notification>
```

## time-specification

* relative - `[H,]M[.S]` or `[Hh][Mm][Ss]`
* absolute - `[HH]:[MM]`

## Examples

```
nt 10 '10 minutes up'
nt 30s '30 seconds up'
nt 1, '1 hour up'
nt 5.30 '5 minutes 30 seconds up'
nt .10 '10 seconds up'
nt 1,10 '1 hour 10 minutes up'
nt 11:15 '11:15 up'
nt 1: '01:00 up'
nt :5 '00:05 up'
```

# Installation

Clone the repository and run
```
cd nt
make install clean
```
Make sure that `$HOME/.local/bin` is in the PATH environment variable, as it is
the default installation destination ([Makefile](Makefile)).
