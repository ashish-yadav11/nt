# nt

'at' wrapper for setting one time notification alarms. Alarms can be set after
a specific time interval with upto seconds precision or at some time within a
day or using 'at' supported time specification (see `man at`).

# Configuration

Command used for sending notifications and prompt used when asking for
notification message can be configured in config.h.

> `make`, the first time you run it, or `make config.h` will create config.h by
> copying [config.def.h](config.def.h).

# Dependencies

* at
* a program capable of sending notifications (e.g. notify-send)

# Usage

```
nt -h|--help
nt <time-specification> [<notification-message>...]
nt -a <at-supported-time-specification>... [-m <notification-message>...]
ntq
ntrm -a|-A|-l|-n|<id>
```

## time-specification

* relative - `M[.M]` or `[H[.H]h][M[.M]m][Ss]`
* absolute - `[HH]:[MM]`

## ntq

List pending notification alarms.

## ntrm

* `ntrm -a` - remove all pending notification alarms.
* `ntrm -A` - remove all pending notification alarms without confirmation.
* `ntrm -l` - remove the last added notification alarm.
* `ntrm -n` - remove the next notification alarm.
* `ntrm <id>` - remove pending notificaiton alarm with id `<id>`.

## Examples

```
nt 15 '15 minutes up'
nt 30s '30 seconds up'
nt 4.5 '4.5 minutes up'
nt .5h 'half an hour up'
nt 2h5s '2 hours 5 seconds up'
nt 11:15 '11:15 up'
nt 1: '01:00 up'
nt :5 '00:05 up'
nt -a noon tomorrow -m 'noon time'
```

# Installation

Clone the repository and run
```
cd nt
make
sudo make install
```

# Extra Tip

If you want pending ntq alarms to be shown instantly after waking up from
suspend, add an executable file with the following lines in
`/usr/lib/systemd/system-sleep/` -

```
#!/bin/sh
read -r PID </var/run/atd.pid && kill -HUP "$PID"
ntping
```

The first line pings at daemon to check for pending jobs (might not work on
every system, works on Arch). The second line pings already running nt jobs to
recalculate time left to sleep and notify if time is up.
