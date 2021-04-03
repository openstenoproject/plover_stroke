import operator

import pytest

from plover_stroke import BaseStroke


@pytest.fixture
def stroke_class():
    class Stroke(BaseStroke):
        pass
    return Stroke

@pytest.fixture
def english_stroke_class(stroke_class):
    stroke_class.setup(
        '''
        #
        S- T- K- P- W- H- R-
        A- O-
        *
        -E -U
        -F -R -P -B -L -G -T -S -D -Z
        '''.split(),
        'A- O- * -E -U'.split(),
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
        }
    )
    return stroke_class

def test_setup_explicit(stroke_class):
    keys = '''
        #
        S- T- K- P- W- H- R-
        A- O-
        *
        -E -U
        -F -R -P -B -L -G -T -S -D -Z
    '''.split()
    implicit_hyphen_keys = 'A- O- * -E -U'.split()
    stroke_class.setup(keys, implicit_hyphen_keys)
    assert stroke_class.KEYS == tuple(keys)
    assert stroke_class.KEYS_IMPLICIT_HYPHEN == set(implicit_hyphen_keys)
    assert stroke_class.KEYS_LETTERS == ''.join(keys).replace('-', '')
    assert stroke_class.KEY_FIRST_RIGHT_INDEX == keys.index('-E')

def test_setup_explicit_with_numbers(stroke_class):
    keys = '''
        #
        S- T- K- P- W- H- R-
        A- O-
        *
        -E -U
        -F -R -P -B -L -G -T -S -D -Z
    '''.split()
    implicit_hyphen_keys = 'A- O- 5- 0- -E -U *'.split()
    number_key = '#'
    numbers = {
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
    }
    stroke_class.setup(keys, implicit_hyphen_keys, number_key, numbers)
    assert stroke_class.KEYS == tuple(keys)
    assert stroke_class.KEYS_IMPLICIT_HYPHEN == set(implicit_hyphen_keys)
    assert stroke_class.KEYS_LETTERS == ''.join(keys).replace('-', '')
    assert stroke_class.KEY_FIRST_RIGHT_INDEX == keys.index('-E')

def test_setup_implicit_hyphens(stroke_class):
    keys = '''
        #
        S- T- K- P- W- H- R-
        A- O-
        *
        -E -U
        -F -R -P -B -L -G -T -S -D -Z
    '''.split()
    stroke_class.setup(keys)
    assert stroke_class.KEYS == tuple(keys)
    assert stroke_class.KEYS_IMPLICIT_HYPHEN == {'A-', 'O-', '*', '-E', '-U', '-F'}
    assert stroke_class.KEYS_LETTERS == ''.join(keys).replace('-', '')
    assert stroke_class.KEY_FIRST_RIGHT_INDEX == keys.index('-E')

def test_setup_numbers(stroke_class):
    keys = '''
        #
        S- T- K- P- W- H- R-
        A- O-
        *
        -E -U
        -F -R -P -B -L -G -T -S -D -Z
    '''.split()
    implicit_hyphen_keys = 'A- O- * -E -U'.split()
    number_key = '#'
    numbers = {
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
    }
    stroke_class.setup(keys, implicit_hyphen_keys, number_key, numbers)
    assert stroke_class.KEYS == tuple(keys)
    assert stroke_class.KEYS_IMPLICIT_HYPHEN == set(implicit_hyphen_keys)
    assert stroke_class.KEYS_LETTERS == ''.join(keys).replace('-', '')
    assert stroke_class.KEY_FIRST_RIGHT_INDEX == keys.index('-E')
    assert stroke_class.NUMBER_KEY == number_key
    assert stroke_class.NUMBER_TO_KEY == {
        v.replace('-', ''): (k.replace('-', ''), keys.index(k))
        for k, v in numbers.items()
    }

NEW_TESTS = (
    (
        '#',
        '#',
        '#',
        0b00000000000000000000001,
        False,
    ),
    (
        'T- -B -P S-',
        'S- T- -P -B',
        'ST-PB',
        0b00000011000000000000110,
        False,
    ),
    (
        'O- -E A-',
        'A- O- -E',
        'AOE',
        0b00000000000101100000000,
        False,
    ),
    (
        '-Z *',
        '* -Z',
        '*Z',
        0b10000000000010000000000,
        False,
    ),
    (
        '-R R-',
        'R- -R',
        'R-R',
        0b00000000100000010000000,
        False,
    ),
    (
        'S- -P O- # T-',
        '# S- T- O- -P',
        '120-7',
        0b00000001000001000000111,
        True,
    ),
    (
        '1- 2- 0- -7',
        '# S- T- O- -P',
        '120-7',
        0b00000001000001000000111,
        True,
    ),
    (
        '-L -F',
        '-F -L',
        '-FL',
        0b00000100010000000000000,
        False,
    ),
)

@pytest.mark.parametrize('in_keys, keys, rtfcre, value, isnumber', NEW_TESTS)
def test_new(english_stroke_class, in_keys, keys, rtfcre, value, isnumber):
    in_keys = in_keys.split()
    keys = keys.split()
    for init_arg in (in_keys, rtfcre, value):
        s = english_stroke_class(init_arg)
        assert s == value
        assert s.keys() == keys
        assert s.isnumber() == isnumber
        assert str(s) == rtfcre

HASH_TESTS = (
    ('#',    0b00000000000000000000001),
    ('ST',   0b00000000000000000000110),
    ('STK',  0b00000000000000000001110),
    ('*',    0b00000000000010000000000),
    ('-PB',  0b00000011000000000000000),
    ('AOE',  0b00000000000101100000000),
    ('R-R',  0b00000000100000010000000),
    ('R-F',  0b00000000010000010000000),
    ('APBD', 0b01000011000000100000000),
)

@pytest.mark.parametrize('steno, hash_value', HASH_TESTS)
def test_hash(english_stroke_class, steno, hash_value):
    assert hash(english_stroke_class(steno)) == hash_value

CMP_TESTS = (
    ('#', '<', 'ST'),
    ('T', '>', 'ST'),
    ('PH', '>', 'TH'),
    ('SH', '<', 'STH'),
    ('ST', '<=', 'STK'),
    ('STK', '<=', 'STK'),
    ('STK', '==', 'STK'),
    ('*', '!=', 'R-R'),
    ('-PB', '>', 'AOE'),
    ('R-R', '>=', 'R-F'),
    ('APBD', '>=', 'APBD'),
)

@pytest.mark.parametrize('steno, op, other_steno', CMP_TESTS)
def test_cmp(english_stroke_class, steno, op, other_steno):
    op = {
        '<': operator.lt,
        '<=': operator.le,
        '==': operator.eq,
        '!=': operator.ne,
        '>=': operator.ge,
        '>': operator.gt,
    }[op]
    assert op(english_stroke_class(steno), other_steno)

def test_sort(english_stroke_class):
    unsorted_strokes = [
        english_stroke_class(s)
        for s in '''
        AOE
        ST-PB
        *Z
        #
        R-R
        '''.split()
    ]
    sorted_strokes = [
        english_stroke_class(s)
        for s in '''
        #
        ST-PB
        R-R
        AOE
        *Z
        '''.split()
    ]
    assert list(sorted(unsorted_strokes)) == sorted_strokes

def test_xrange(english_stroke_class):
    expected = [
        english_stroke_class(s)
        for s in '''
        ST
        #ST
        K
        #K
        SK
        #SK
        TK
        #TK
        STK
        #STK
        P
        #P
        SP
        #SP
        '''.split()
    ]
    assert list(english_stroke_class.xrange('ST', 'TP')) == expected

def test_xsuffixes(english_stroke_class):
    expected = [
        english_stroke_class(s)
        for s in '''
        -TS
        -TD
        -TSD
        -TZ
        -TSZ
        -TDZ
        -TSDZ
        '''.split()
    ]
    assert list(english_stroke_class('-T').xsuffixes()) == expected

def test_no_numbers_system():
    class Stroke(BaseStroke):
        pass
    Stroke.setup((
        '#',
        'S-', 'T-', 'K-', 'P-', 'W-', 'H-', 'R-',
        'A-', 'O-',
        '*',
        '-E', '-U',
        '-F', '-R', '-P', '-B', '-L', '-G', '-T', '-S', '-D', '-Z',
    ),
        ('A-', 'O-', '*', '-E', '-U')
    )
    s1 = Stroke(23)

