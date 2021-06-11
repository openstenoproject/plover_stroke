import functools
import operator
import re

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

def test_setup_minimal(stroke_class):
    keys = '''
        #
        S- T- K- P- W- H- R-
        A- O-
        *
        -E -U
        -F -R -P -B -L -G -T -S -D -Z
    '''.split()
    stroke_class.setup(keys)
    helper = stroke_class._helper
    assert helper.num_keys             == len(keys)
    assert helper.keys                 == tuple(keys)
    assert helper.letters              == ''.join(keys).replace('-', '')
    assert helper.numbers              == ''.join(keys).replace('-', '')
    assert helper.implicit_hyphen_mask == 0b00000000011111100000000
    assert helper.number_key_mask      == 0b00000000000000000000000
    assert helper.numbers_mask         == 0b00000000000000000000000
    assert helper.right_keys_mask      == 0b11111111111100000000000
    assert helper.right_keys_index     == keys.index('-E')

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
    helper = stroke_class._helper
    assert helper.num_keys             == len(keys)
    assert helper.keys                 == tuple(keys)
    assert helper.letters              == ''.join(keys).replace('-', '')
    assert helper.numbers              == ''.join(keys).replace('-', '')
    assert helper.implicit_hyphen_mask == 0b00000000001111100000000
    assert helper.number_key_mask      == 0b00000000000000000000000
    assert helper.numbers_mask         == 0b00000000000000000000000
    assert helper.right_keys_mask      == 0b11111111111100000000000
    assert helper.right_keys_index     == keys.index('-E')

def test_setup_explicit_with_numbers(stroke_class):
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
    helper = stroke_class._helper
    assert helper.num_keys             == len(keys)
    assert helper.keys                 == tuple(keys)
    assert helper.letters              == ''.join(keys).replace('-', '')
    assert helper.numbers              == ''.join(numbers.get(k, k) for k in keys).replace('-', '')
    assert helper.implicit_hyphen_mask == 0b00000000001111100000000
    assert helper.number_key_mask      == 0b00000000000000000000001
    assert helper.numbers_mask         == 0b00010101010001101010110
    assert helper.right_keys_mask      == 0b11111111111100000000000
    assert helper.right_keys_index     == keys.index('-E')

IMPLICIT_HYPHENS_DETECTION_TESTS = (
    ('''
     #
     S- T- K- P- W- H- R-
     A- O-
     *
     -E -U
     -F -R -P -B -L -G -T -S -D -Z
     ''',
     'A- O- * -E -U -F'
    ),
    ('''
     #
     A- O-
     *
     ''',
     '# A- O- *'
    ),
    ('''
     -F -R -P -B -L -G -T -S -D -Z
     ''',
     '-F -R -P -B -L -G -T -S -D -Z'
    ),
    ('''
     #
     S- P- C- T- H- V- R-
     I- A-
     -E -O
     -c -s -t -h -p -r
     *
     -i -e -a -o
     ''',
     '''
     #
     S- P- C- T- H- V- R-
     I- A-
     -E -O
     -c -s -t -h -p -r
     *
     -i -e -a -o
     '''
    ),
)

@pytest.mark.parametrize('keys, implicit_hyphen_keys', IMPLICIT_HYPHENS_DETECTION_TESTS)
def test_setup_implicit_hyphens_detection(stroke_class, keys, implicit_hyphen_keys):
    keys = keys.split()
    implicit_hyphen_keys = implicit_hyphen_keys.split()
    implicit_hyphen_mask = functools.reduce(operator.or_, (
        1 << keys.index(k) for k in implicit_hyphen_keys
    ), 0)
    stroke_class.setup(keys)
    helper = stroke_class._helper
    assert helper.keys == tuple(keys)
    assert helper.implicit_hyphen_mask == implicit_hyphen_mask

INVALID_PARAMS_TESTS = (
    ('''
     #
     S- T- K- P- W- H- R-
     A- O-
     *
     -E -U
     -F -R -P -B -L -G -T -S -D -Z
     '''.split(),
     dict(number_key='V-',
          numbers={
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
          }),
     ValueError,
     "invalid `number_key`"
    ),
    ('''
     #
     S- T- K- P- W- H- R-
     A- O-
     *
     -E -U
     -F -R -P -B -L -G -T -S -D -Z
     '''.split(),
     dict(number_key='#',
          numbers={
              'S-': '1-',
              'T-': '2-',
              '-F': '-6',
              '-P': '-7',
              '-L': '-8',
              '-T': '-9',
          }),
     ValueError,
     "invalid `numbers`"
    ),
    ('''
     #
     S- T- K- P- W- H- R-
     A- O-
     *
     -E -U
     -F -R -P -B -L -G -T -S -D -Z
     '''.split(),
     dict(implicit_hyphen_keys='A- O- -E -U'.split()),
     ValueError,
     "invalid `implicit_hyphen_keys`: not a continuous block"
    ),
    ('''
     #
     S- T- K- P- W- H- R-
     A- O-
     *
     -E -U
     -F -R -P -B -L -G -T -S -D -Z
     '''.split(),
     dict(implicit_hyphen_keys='A- O- -E -U -V'.split()),
     ValueError,
     "invalid `implicit_hyphen_keys`: not all keys accounted for"
    ),
    ('''
     #
     S- T- K- P- W- H- R-
     A- O-
     *
     -E -U
     -F -R -P -B -L -G -T -S -D -Z
     '''.split(),
     dict(implicit_hyphen_keys='R- A- O- * -E -U'.split()),
     ValueError,
     "invalid `implicit_hyphen_keys`: some letters are not unique"
    ),
)

@pytest.mark.parametrize('keys, kwargs, exception, match', INVALID_PARAMS_TESTS)
def test_setup_invalid_params(stroke_class, keys, kwargs, exception, match):
    with pytest.raises(exception, match='^' + re.escape(match) + '$'):
        stroke_class.setup(keys, **kwargs)

def test_from_empty_steno(english_stroke_class):
    with pytest.raises(ValueError):
        english_stroke_class.from_steno('')

NEW_TESTS = (
    (
        '#', '#-',
        '#',
        '#',
        0b00000000000000000000001,
        False,
        False,
    ),
    (
        '# -Z', '#Z',
        '# -Z',
        '#-Z',
        0b10000000000000000000001,
        False,
        False,
    ),
    (
        'T- -B -P S-', 'ST-PB',
        'S- T- -P -B',
        'ST-PB',
        0b00000011000000000000110,
        False,
        False,
    ),
    (
        'O- -E A-', 'AO-E',
        'A- O- -E',
        'AOE',
        0b00000000000101100000000,
        False,
        False,
    ),
    (
        '-Z *', '*-Z',
        '* -Z',
        '*Z',
        0b10000000000010000000000,
        False,
        False,
    ),
    (
        '-R R-', 'RR',
        'R- -R',
        'R-R',
        0b00000000100000010000000,
        False,
        False,
    ),
    (
        'S- -P O- # T-', '#STO-P',
        '# S- T- O- -P',
        '1207',
        0b00000001000001000000111,
        True,
        True,
    ),
    (
        '1- 2- 0- -7', '#1207',
        '# S- T- O- -P',
        '1207',
        0b00000001000001000000111,
        True,
        True,
    ),
    (
        '-L -F', 'FL',
        '-F -L',
        '-FL',
        0b00000100010000000000000,
        False,
        False,
    ),
    (
        '1- 2- -E -7', '#12E7',
        '# S- T- -E -P',
        '12E7',
        0b00000001000100000000111,
        True,
        False,
    ),
    (
        '''
        # S- 1- T- 2- K- P- 3- W- H- 4- R-
        A- 5- O- 0-
        *
        -E -U
        -6 -F -R -7 -P -B -8 -L -G -9 -T -S -D -Z
        ''', '#STKPWHRAO*-EUFRPBLGTSDZ',
        '# S- T- K- P- W- H- R- A- O- * -E -U -F -R -P -B -L -G -T -S -D -Z',
        '12K3W4R50*EU6R7B8G9SDZ',
        0b11111111111111111111111,
        True,
        False,
    ),
)

@pytest.mark.parametrize('in_keys, in_rtfcre, keys, rtfcre, value, has_digit, is_number', NEW_TESTS)
def test_new(english_stroke_class, in_keys, in_rtfcre, keys, rtfcre, value, has_digit, is_number):
    in_keys = in_keys.split()
    keys = keys.split()
    for init_arg in (in_keys, in_rtfcre, rtfcre, value):
        s = english_stroke_class(init_arg)
        assert int(s) == value
        assert hash(s) == int(s)
        assert list(s) == keys
        assert s.keys() == tuple(keys)
        assert len(s) == len(keys)
        assert str(s) == rtfcre
        assert s.first() == keys[0]
        assert s.last() == keys[-1]
        assert s.has_digit() == has_digit
        assert s.is_number() == is_number

def test_empty_stroke(english_stroke_class):
    empty_stroke = english_stroke_class(0)
    assert int(empty_stroke) == 0
    assert str(empty_stroke) == ''
    assert not empty_stroke.has_digit()
    assert not empty_stroke.is_number()
    with pytest.raises(ValueError):
        empty_stroke.first()
    with pytest.raises(ValueError):
        empty_stroke.last()

AFFIX_TESTS = (
    ('#', 'prefix', 'ST', True),
    ('#', 'suffix', 'ST', False),
    ('ST', 'suffix', '#', True),
    ('ST', 'prefix', '#', False),
    ('ST', 'suffix', 'T', False),
    ('ST', 'prefix', 'T', False),
    ('T', 'suffix', 'ST', False),
    ('T', 'prefix', 'ST', False),
)

@pytest.mark.parametrize('s1, op, s2, expected', AFFIX_TESTS)
def test_affix(english_stroke_class, s1, op, s2, expected):
    op = operator.methodcaller('is_' + op, s2)
    assert op(english_stroke_class(s1)) == expected

CONTAIN_TESTS = (
    ('#', '19', True),
    ('E', 'TEFT', True),
    ('1', '#START', True),
    ('S', 'TEFT', False),
    ('TEFT', 'E', False),
)

@pytest.mark.parametrize('s1, s2, expected', CONTAIN_TESTS)
def test_contain(english_stroke_class, s1, s2, expected):
    assert (s1 in english_stroke_class(s2)) == expected

INVERT_TESTS = (
    (0, '#STKPWHRAO*EUFRPBLGTSDZ'),
    ('AOEU', '#STKPWHR*FRPBLGTSDZ'),
    (0b00010001010101100000001,
     0b11101110101010011111110),
)

@pytest.mark.parametrize('s1, s2', INVERT_TESTS)
def test_invert(english_stroke_class, s1, s2):
    assert ~english_stroke_class(s1) == s2
    assert ~english_stroke_class(s2) == s1

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

OP_TESTS = (
    ('#', '|', 'ST', '12'),
    ('12', '&', '#ST', '12'),
    ('12', '-', '#', 'ST'),
    ('PL', '+', '#', '38'),
)

@pytest.mark.parametrize('s1, op, s2, expected', OP_TESTS)
def test_op(english_stroke_class, s1, op, s2, expected):
    op = {
        '|': operator.or_,
        '&': operator.and_,
        '+': operator.add,
        '-': operator.sub,
    }[op]
    assert op(english_stroke_class(s1), s2) == expected

CMP_TESTS = (
    ('#', '<', 'ST'),
    ('T', '>', 'ST'),
    ('PH', '>', 'TH'),
    ('SH', '>', 'STH'),
    ('ST', '<=', 'STK'),
    ('STK', '<=', 'STK'),
    ('STK', '==', 'STK'),
    ('*', '!=', 'R-R'),
    ('-PB', '>', 'AOE'),
    ('R-R', '>=', 'R-F'),
    ('APBD', '>=', 'APBD'),
    ('ST-TS', '<', 'ST-TZ'),
    ('ST-TSZ', '<', 'ST-TZ'),
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

NORMALIZE_STENO_TESTS = (
    ('#STKPWHRAO*-EUFRPBLGTSDZ/12K3W4R50*-EU6R7B8G9SDZ',
     ('12K3W4R50*EU6R7B8G9SDZ', '12K3W4R50*EU6R7B8G9SDZ')),
)

@pytest.mark.parametrize('steno, expected', NORMALIZE_STENO_TESTS)
def test_normalize_steno(english_stroke_class, steno, expected):
    normalize_steno = english_stroke_class._helper.normalize_steno
    assert normalize_steno(steno) == expected
