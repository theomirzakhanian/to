# time

```
use time
```

## Functions

| Function | Returns | Description |
|----------|---------|-------------|
| `time.now()` | float | Unix timestamp in seconds |
| `time.ms()` | int | Current time in milliseconds |
| `time.sleep(secs)` | none | Pause execution |
| `time.date()` | string | Current date `"YYYY-MM-DD"` |
| `time.clock()` | string | Current time `"HH:MM:SS"` |
| `time.format(ts, fmt)` | string | Format a timestamp |
| `time.year()` | int | Current year |
| `time.month()` | int | Current month (1-12) |
| `time.day()` | int | Current day (1-31) |
| `time.hour()` | int | Current hour (0-23) |
| `time.minute()` | int | Current minute (0-59) |
| `time.second()` | int | Current second (0-59) |

## Example

```
use time

print "Today is {time.date()}"
print "Time: {time.clock()}"

~ Benchmark something
start = time.ms()
~ ... work ...
elapsed = time.ms() - start
print "Took {elapsed}ms"

~ Sleep
time.sleep(1.5)   ~ wait 1.5 seconds
```
