# Plover Stroke

Helper class for working with steno strokes.

Usage:

``` python
# Setup:

from plover_stroke import BaseStroke

class Stroke(BaseStroke):
     pass

Stroke.setup(
    # System keys.
    '''
    #
    S- T- K- P- W- H- R-
    A- O-
    *
    -E -U
    -F -R -P -B -L -G -T -S -D -Z
    '''.split(),
    # Implicit hyphen keys (optional, automatically
    # deduced from system keys if not passed).
    'A- O- * -E -U'.split(),
    # Number bar key and numbers keys (optional).
    '#', {
    'S-': '1-',
    'T-': '2-',
    'P-': '3-',
    'H-': '4-',
    'A-': '5-',
    'O-': '0-',
    '-F': '-6',
    '-P': '-7',
    '-L': '-8',
    '-T': '-9',
    })

# Creating strokes:

Stroke(56)
# => KPW
Stroke(('-F', 'S-', '-S', 'A-', '*')) 
# => SA*FS
Stroke('R-')
# => R
Stroke('L-')
# => invalid, raise a ValueError

# Methods:

s = Stroke('STK')

s.keys()
# => ('S-', 'T-', 'K-')
s.is_number()
# => False
int(s)
# => 14
s == 0b00000000000000000001110
# => True

# Strokes can be compared:
sorted(map(Stroke, 'AOE ST-PB *Z # R-R'.split()))
# => [#, ST-PB, R-R, AOE, *Z]
```


## Release history

### 1.0.0 [unreleased]

* drop `Stroke.xrange` and `Stroke.xsuffixes` methods
* `Stroke.keys()` now return a tuple
* fix corner case when parsing steno (`RR` -> `R-R`)
* fix RTFCRE when numbers are involved (align with Plover's behavior)
* fix implicit hyphen handling when numbers are involved
* renamed `Stroke.isnumber` to `Stroke.is_number`
* speed up implementation through a C extension


### 0.4.0

* fix stroke comparison


### 0.3.3

* fix `python_requires` package metadata


### 0.3.2

* first public release
