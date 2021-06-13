from _plover_stroke import StrokeHelper


class BaseStroke(int):

    _helper = None

    @classmethod
    def setup(cls, keys, implicit_hyphen_keys=None,
              number_key=None, numbers=None):
        cls._helper = StrokeHelper()
        if number_key is None:
            assert numbers is None
        else:
            assert numbers is not None
        if implicit_hyphen_keys is not None and not isinstance(implicit_hyphen_keys, set):
            implicit_hyphen_keys = set(implicit_hyphen_keys)
        cls._helper.setup(keys, implicit_hyphen_keys=implicit_hyphen_keys,
                          number_key=number_key, numbers=numbers)

    @classmethod
    def from_steno(cls, steno):
        return int.__new__(cls, cls._helper.stroke_from_steno(steno))

    @classmethod
    def from_keys(cls, keys):
        return int.__new__(cls, cls._helper.stroke_from_keys(keys))

    @classmethod
    def from_integer(cls, integer):
        return int.__new__(cls, cls._helper.stroke_from_int(integer))

    def __new__(cls, value):
        return int.__new__(cls, cls._helper.stroke_from_any(value))

    def __hash__(self):
        return int(self)

    def __eq__(self, other):
        return self._helper.stroke_eq(self, other)

    def __ge__(self, other):
        return self._helper.stroke_ge(self, other)

    def __gt__(self, other):
        return self._helper.stroke_gt(self, other)

    def __le__(self, other):
        return self._helper.stroke_le(self, other)

    def __lt__(self, other):
        return self._helper.stroke_lt(self, other)

    def __ne__(self, other):
        return self._helper.stroke_ne(self, other)

    def __contains__(self, other):
        return self._helper.stroke_in(other, self)

    def __invert__(self):
        return self.from_integer(self._helper.stroke_invert(self))

    def __or__(self, other):
        return self.from_integer(self._helper.stroke_or(self, other))

    def __and__(self, other):
        return self.from_integer(self._helper.stroke_and(self, other))

    def __add__(self, other):
        return self.from_integer(self._helper.stroke_or(self, other))

    def __sub__(self, other):
        return self.from_integer(self._helper.stroke_sub(self, other))

    def __len__(self):
        return self._helper.stroke_len(self)

    def __iter__(self):
        return iter(self._helper.stroke_to_keys(self))

    def __repr__(self):
        return self._helper.stroke_to_steno(self)

    def __str__(self):
        return self._helper.stroke_to_steno(self)

    def first(self):
        return self._helper.stroke_first_key(self)

    def last(self):
        return self._helper.stroke_last_key(self)

    def keys(self):
        return self._helper.stroke_to_keys(self)

    def has_digit(self):
        return self._helper.stroke_has_digit(self)

    def is_number(self):
        return self._helper.stroke_is_number(self)

    def is_prefix(self, other):
        return self._helper.stroke_is_prefix(self, other)

    def is_suffix(self, other):
        return self._helper.stroke_is_suffix(self, other)


# Prevent use of 'from stroke import *'.
__all__ = ()
