# nt

'at' wrapper for setting one time notification alarms. Alarms can be set after
a specific time interval with upto seconds precision or at some time within a
day.

# Configuration

The command used for sending notifications can be changed in `config.h`.

> `make`, the first time you run it, or `make config.h` will create config.h by
> copying [config.def.h](config.def.h).

# Dependencies

* at
* a program capable of sending notifications (eg. notify-send)

# Usage

```
nt -h|--help
nt <time-specification> <notification>
ntq
ntrm -a|<id>
```

## ntq

List pending notification alarms.

## ntrm

* `ntrm -a` - remove all pending notification alarms.
* `ntrm <id>` - remove pending notificaiton alarm with id `<id>`.

## time-specification

* relative - `[H,]M[.S]` or `[Hh][Mm][Ss]`
* absolute - `[HH]:[MM]`

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
```

# Installation

Clone the repository and run
```
cd nt
make install clean
```
> Make sure that `$HOME/.local/bin` is in your PATH environment variable, as it
> is the default installation destination ([Makefile](Makefile)).
