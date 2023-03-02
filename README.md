# Flipper Nested Recovery script

Script recovers keys from collected authorization attempts (nonces).
You can collect nonces on Flipper Zero with https://github.com/AloneLiberty/FlipperNested

Flipper Zero should be connected with USB cable and not used by other software (./fbt log, qFlipper)

Won't work on Windows, because uses pthreads

## Installation

```bash
python setup.py install --user
```

## Usage

```bash
$ FlipperNested
Checking 12345678.nonces
Calculating for key type A, sector 0
Found 1 key(s): ['ffffffffffff']
...
Found potential 32 keys, use "Check found keys" in app

```