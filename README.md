# nt

'at' wrapper for setting one time notification alarms. Alarms can be set after
a specific time interval with upto seconds precision or at some time within a
day or using 'at' supported time specification (see `man at`).

# Configuration

Command used for sending notifications and prompt used when asking for
notification message can be configured in `config.h`.

> `make`, the first time you run it, or `make config.h` will create config.h by
> copying [config.def.h](config.def.h).

# Dependencies

* at
* a program capable of sending notifications (e.g. notify-send)

# Usage

```
nt -h|--help
nt <time-specification> [<notification-message>...]
nt -t <at-supported-time-specification>... [-m <notification-message>...]
ntq
ntrm -a|<id>
```

## time-specification

* relative - `[H,]M[.S]` or `[Hh][Mm][Ss]`
* absolute - `[HH]:[MM]`

## ntq

List pending notification alarms.

## ntrm

* `ntrm -a` - remove all pending notification alarms.
* `ntrm <id>` - remove pending notificaiton alarm with id `<id>`.

## Examples

```
nt 10 '10 minutes up'
nt 30s '30 seconds up'
nt 1, '1 hour up'
nt 5.30 '5 minutes 30 seconds up'
nt 2h5s '2 hours 5 seconds up'
nt .10 '10 seconds up'
nt 1,10 '1 hour 10 minutes up'
nt 11:15 '11:15 up'
nt 1: '01:00 up'
nt :5 '00:05 up'
nt -t noon tomorrow -m 'noon time'
```

# Installation

Clone the repository and run
```
cd nt
make install clean
```
> Make sure that `$HOME/.local/bin` is in your PATH environment variable, as it
> is the default installation destination ([Makefile](Makefile)).
