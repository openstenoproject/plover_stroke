def msb(x):
    x |= (x >> 1)
    x |= (x >> 2)
    x |= (x >> 4)
    x |= (x >> 8)
    x |= (x >> 16)
    x |= (x >> 32)
    return x & ~(x >> 1)

def lsb(x):
    return x & -x

def popcount(x):
    # 0x5555555555555555: 0101...
    # 0x3333333333333333: 00110011..
    # 0x0f0f0f0f0f0f0f0f:  4 zeros,  4 ones ...
    # Put count of each 2 bits into those 2 bits.
    x -= (x >> 1) & 0x5555555555555555
    # Put count of each 4 bits into those 4 bits.
    x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333)
    # Put count of each 8 bits into those 8 bits.
    x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0f
    # Put count of each 16 bits into their lowest 8 bits.
    x += x >>  8
    # Put count of each 32 bits into their lowest 8 bits.
    x += x >> 16
    # Put count of each 64 bits into their lowest 8 bits.
    x += x >> 32
    return x & 0x7f


def cmp_strokes(s1, s2):
    si1 = int(s1)
    si2 = int(s2)
    m = si1 ^ si2
    si1 = (si1 & m) or si1
    lsb1 = si1 & -si1
    si2 = (si2 & m) or si2
    lsb2 = si2 & -si2
    return lsb1 - lsb2


_NUMBERS = set('0123456789')


class BaseStroke(int):

    KEYS = None
    KEYS_NUMBER = None
    KEYS_MASK = 0
    KEYS_LETTERS = None
    KEYS_IMPLICIT_HYPHEN = None
    KEY_FIRST_RIGHT_INDEX = None
    KEY_TO_MASK = None
    KEY_FROM_MASK = None
    KEY_TO_NUMBER = None
    NUMBERS = None
    NUMBER_KEY = None
    NUMBER_MASK = 0
    NUMBER_TO_KEY = None

    @classmethod
    def setup(cls, keys, implicit_hyphen_keys=None,
              number_key=None, numbers=None):
        assert len(keys) <= 64
        if numbers is None:
            numbers = {}
        cls.KEYS = tuple(keys)
        cls.KEYS_MASK = (1 << len(cls.KEYS)) - 1
        cls.KEY_TO_MASK = {k: 1 << n for n, k in enumerate(cls.KEYS)}
        cls.KEY_FROM_MASK = dict(zip(cls.KEY_TO_MASK.values(), cls.KEY_TO_MASK.keys()))
        # Find left and right letters.
        cls.KEY_FIRST_RIGHT_INDEX = None
        cls.KEYS_LETTERS = ''
        letters_left = {}
        letters_right = {}
        for n, k in enumerate(keys):
            assert len(k) <= 2
            if len(k) == 1:
                assert k != '-'
                l = k
                is_left = False
                is_right = False
            elif len(k) == 2:
                is_left = k[1] == '-'
                is_right = k[0] == '-'
                assert is_left != is_right
                l = k.replace('-', '')
            cls.KEYS_LETTERS += l
            if cls.KEY_FIRST_RIGHT_INDEX is None:
                if not is_right:
                    assert k not in letters_left
                    letters_left[l] = k
                    continue
                cls.KEY_FIRST_RIGHT_INDEX = n
            # Invalid: ['-R', '-L']
            assert not is_left
            # Invalid: ['-R', '-R']
            assert k not in letters_right
            # Invalid: ['#', '-R', '#']
            assert is_right or l not in letters_left
            letters_right[l] = k
        # Find implicit hyphen keys/letters.
        implicit_hyphen_letters = {}
        for k in reversed(keys[:cls.KEY_FIRST_RIGHT_INDEX]):
            l = k.replace('-', '')
            if l in letters_right:
                break
            implicit_hyphen_letters[l] = k
        for k in keys[cls.KEY_FIRST_RIGHT_INDEX:]:
            l = k.replace('-', '')
            if l in letters_left:
                break
            implicit_hyphen_letters[l] = k
        if implicit_hyphen_keys is not None:
            # Hyphen keys must be a continous block.
            hyphens_str = lambda l: ''.join(sorted(l, key=cls.KEYS.index))
            all_hyphens = hyphens_str(implicit_hyphen_letters.values())
            hyphens = hyphens_str(k for k in implicit_hyphen_keys
                                  if k not in numbers.values())
            assert hyphens in all_hyphens
            cls.KEYS_IMPLICIT_HYPHEN = set(implicit_hyphen_keys)
        else:
            cls.KEYS_IMPLICIT_HYPHEN = set(implicit_hyphen_letters.values())
        cls.NUMBERS = set()
        cls.NUMBER_MASK = 0
        cls.NUMBER_TO_KEY = {}
        cls.KEY_TO_NUMBER = {}
        if number_key is not None:
            cls.KEY_TO_NUMBER = dict(numbers)
            cls.NUMBER_KEY = number_key
            cls.NUMBER_MASK |= cls.KEY_TO_MASK[number_key]
            for key, num in cls.KEY_TO_NUMBER.items():
                cls.NUMBER_TO_KEY[num.strip('-')] = (key.strip('-'), cls.KEYS.index(key))
                cls.KEY_TO_MASK[num] = cls.KEY_TO_MASK[key]
                cls.NUMBER_MASK |= cls.KEY_TO_MASK[key]
                cls.NUMBERS.add(num)

    @classmethod
    def from_steno(cls, steno):
        if not steno:
            return cls.from_integer(0)
        n = 0
        keys = set()
        for letter in steno:
            if letter == '-':
                if n >= cls.KEY_FIRST_RIGHT_INDEX:
                    raise ValueError('invalid letter %r in %r' % (letter, steno))
                n = cls.KEY_FIRST_RIGHT_INDEX
                continue
            if letter in _NUMBERS:
                letter, n = cls.NUMBER_TO_KEY[letter]
                keys.add(cls.NUMBER_KEY)
            n = cls.KEYS_LETTERS.find(letter, n)
            if -1 == n:
                raise ValueError('invalid letter %r in %r' % (letter, steno))
            keys.add(cls.KEYS[n])
        return cls.from_keys(keys)

    @classmethod
    def from_keys(cls, keys):
        value = 0
        for k in keys:
            if k in cls.NUMBERS:
                value |= cls.KEY_TO_MASK[cls.NUMBER_KEY]
            value |= cls.KEY_TO_MASK[k]
        return cls.from_integer(value)

    @classmethod
    def from_integer(cls, value):
        assert (value & ~cls.KEYS_MASK) == 0
        return int.__new__(cls, value)

    def __new__(cls, value=None):
        if value is None:
            return cls.from_integer(0)
        if isinstance(value, BaseStroke):
            return value
        if isinstance(value, int):
            return cls.from_integer(value)
        if isinstance(value, str):
            return cls.from_steno(value)
        return cls.from_keys(value)

    def isnumber(self):
        if self.NUMBER_KEY is None:
            return False
        v = int(self)
        nm = self.KEY_TO_MASK[self.NUMBER_KEY]
        return (
            v == (v & self.NUMBER_MASK)
            and v & nm and v != nm
        )

    def __hash__(self):
        return int(self)

    def __lt__(self, other):
        return cmp_strokes(self, self.__class__(other)) < 0

    def __le__(self, other):
        return cmp_strokes(self, self.__class__(other)) <= 0

    def __eq__(self, other):
        return int(self) == int(self.__class__(other))

    def __ne__(self, other):
        return int(self) != int(self.__class__(other))

    def __gt__(self, other):
        return cmp_strokes(self, self.__class__(other)) > 0

    def __ge__(self, other):
        return cmp_strokes(self, self.__class__(other)) >= 0

    def __contains__(self, other):
        ov = int(self.__class__(other))
        return ov == int(self) & ov

    def __invert__(self):
        return self.from_integer(~int(self) & self.KEYS_MASK)

    def __or__(self, other):
        return self.from_integer(int(self) | int(self.__class__(other)))

    def __and__(self, other):
        return self.from_integer(int(self) & int(self.__class__(other)))

    def __add__(self, other):
        return self | other

    def __sub__(self, other):
        return self.from_integer(int(self) & ~int(self.__class__(other)))

    def __len__(self):
        return popcount(int(self))

    def __iter__(self):
        v = int(self)
        while v:
            m = lsb(v)
            yield self.KEY_FROM_MASK[m]
            v &= ~m

    def __repr__(self):
        isnumber = self.isnumber()
        left = ''
        middle = ''
        right = ''
        for k in self:
            if isnumber:
                if k == self.NUMBER_KEY:
                    continue
                k = self.KEY_TO_NUMBER[k]
            l = k.replace('-', '')
            if k in self.KEYS_IMPLICIT_HYPHEN:
                middle += l
            elif k[0] == '-':
                right += l
            else:
                left += l
        s = left
        if not middle and right:
            s += '-'
        else:
            s += middle
        s += right
        return s

    def __str__(self):
        return self.__repr__()

    def first(self):
        return self.KEY_FROM_MASK[lsb(int(self))]

    def last(self):
        return self.KEY_FROM_MASK[msb(int(self))]

    def keys(self):
        return list(self)

    def is_prefix(self, other):
        return msb(int(self)) < lsb(int(self.__class__(other)))

    def is_suffix(self, other):
        return lsb(int(self)) > msb(int(self.__class__(other)))

    @classmethod
    def xrange(cls, start, stop=None):
        start = int(cls(start))
        if stop is None:
            start, stop = 0, start
        if -1 == stop:
            stop = (1 << len(cls.KEYS)) - 1
        stop = int(cls(stop))
        assert start <= stop
        for v in range(start, stop):
            yield cls(v)

    def xsuffixes(self, stop=None):
        """Generate all stroke prefixed by <self>
        (not included), until <stop> (included).
        """
        start_bit = msb(int(self)) << 1
        end_bit = 1 << len(self.KEYS)
        assert start_bit <= end_bit
        count = popcount((end_bit - 1) & ~(start_bit - 1))
        prefix = int(self)
        shift = popcount(start_bit - 1)
        if stop is None:
            stop = end_bit - 1
        stop = int(self.__class__(stop))
        for n in range(1, (1 << count)):
            v = n << shift
            assert (prefix & v) == 0
            v |= prefix
            yield self.__class__(v)
            if v == stop:
                break


# Prevent use of 'from stroke import *'.
__all__ = ()
