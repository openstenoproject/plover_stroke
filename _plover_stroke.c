#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "structmember.h"


// Py_UNREACHABLE is only available starting with Python 3.7.
#ifdef Py_UNREACHABLE
# define UNREACHABLE()  Py_UNREACHABLE()
#elif defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 5))
# define UNREACHABLE()  __builtin_unreachable()
#elif defined(__clang__) || defined(__INTEL_COMPILER)
# define UNREACHABLE()  __builtin_unreachable()
#elif defined(_MSC_VER)
# define UNREACHABLE()  __assume(0)
#else
# define UNREACHABLE()  Py_FatalError("Unreachable C code path reached")
#endif


#define MAX_KEYS   63
#define MAX_STENO  (MAX_KEYS + 1) // All keys + one hyphen.

typedef uint64_t stroke_uint_t;
typedef int64_t  stroke_int_t;

#define INVALID_STROKE  ((stroke_uint_t)-1)

#if ULONG_MAX == ((1ULL << 64) - 1)
# define PyLong_AsStrokeUint    PyLong_AsUnsignedLong
# define PyLong_FromStrokeUint  PyLong_FromUnsignedLong
# define PyLong_FromStrokeInt   PyLong_FromLong
# define T_STROKE_UINT          T_ULONG
# define STROKE_1               1UL
#elif ULLONG_MAX == ((1ULL << 64) - 1)
# define PyLong_AsStrokeUint    PyLong_AsUnsignedLongLong
# define PyLong_FromStrokeUint  PyLong_FromUnsignedLongLong
# define PyLong_FromStrokeInt   PyLong_FromLongLong
# define T_STROKE_UINT          T_ULONGLONG
# define STROKE_1               1ULL
#else
# error no suitable 64bits type!
#endif

typedef enum
{
    KEY_SIDE_NONE,
    KEY_SIDE_LEFT,
    KEY_SIDE_RIGHT,

} key_side_t;

typedef enum
{
    CMP_OP_CMP,
    CMP_OP_EQ,
    CMP_OP_NE,
    CMP_OP_GE,
    CMP_OP_GT,
    CMP_OP_LE,
    CMP_OP_LT,

} cmp_op_t;

typedef struct
{
    unsigned      num_keys;
    key_side_t    key_side[MAX_KEYS];
    Py_UCS4       key_letter[MAX_KEYS];
    Py_UCS4       key_number[MAX_KEYS];
    stroke_uint_t implicit_hyphen_mask;
    stroke_uint_t number_key_mask;
    stroke_uint_t numbers_mask;
    unsigned      right_keys_index;

} stroke_helper_t;

typedef struct
{
    PyObject_HEAD
    stroke_helper_t helper;

} StrokeHelper;

static stroke_uint_t lsb(stroke_uint_t x)
{
    return x & (stroke_uint_t)-(stroke_int_t)x;
}

static stroke_uint_t msb(stroke_uint_t x)
{
    x |= (x >> 1);
    x |= (x >> 2);
    x |= (x >> 4);
    x |= (x >> 8);
    x |= (x >> 16);
    x |= (x >> 32);
    return x & ~(x >> 1);
}

static unsigned popcount(stroke_uint_t x)
{
    // 0x5555555555555555: 0101...
    // 0x3333333333333333: 00110011..
    // 0x0f0f0f0f0f0f0f0f:  4 zeros,  4 ones ...
    // Put count of each 2 bits into those 2 bits.
    x -= (x >> 1) & 0x5555555555555555;
    // Put count of each 4 bits into those 4 bits.
    x = (x & 0x3333333333333333) + ((x >> 2) & 0x3333333333333333);
    // Put count of each 8 bits into those 8 bits.
    x = (x + (x >> 4)) & 0x0f0f0f0f0f0f0f0f;
    // Put count of each 16 bits into their lowest 8 bits.
    x += x >>  8;
    // Put count of each 32 bits into their lowest 8 bits.
    x += x >> 16;
    // Put count of each 64 bits into their lowest 8 bits.
    x += x >> 32;
    return x & 0x7f;
}

static Py_UCS4 key_to_letter(PyObject *key, key_side_t *side)
{
    Py_ssize_t  len;
    int         kind;
    const void *data;
    Py_UCS4     letter1;
    Py_UCS4     letter2;

    if (PyUnicode_READY(key))
        return 0;

    len = PyUnicode_GET_LENGTH(key);
    kind = PyUnicode_KIND(key);
    data = PyUnicode_DATA(key);

    switch (len)
    {
    case 1:
        letter1 = PyUnicode_READ(kind, data, 0);
        if (letter1 == '-')
            break;
        *side = KEY_SIDE_NONE;
        return letter1;
    case 2:
        letter1 = PyUnicode_READ(kind, data, 0);
        letter2 = PyUnicode_READ(kind, data, 1);
        if (letter1 == '-')
        {
            if (letter2 == '-')
                break;
            *side = KEY_SIDE_RIGHT;
            return letter2;
        }
        if (letter2 != '-')
            break;
        *side = KEY_SIDE_LEFT;
        return letter1;
    default:
        break;
    }

    PyErr_Format(PyExc_ValueError, "invalid key: %R", key);
    return 0;
}

static stroke_uint_t stroke_from_ucs4(const stroke_helper_t *helper,
                                      const Py_UCS4         *stroke_ucs4,
                                      Py_ssize_t             stroke_len)
{
    stroke_uint_t  mask;
    Py_UCS4        letter;
    int            key_index;
    unsigned       stroke_index;
    const Py_UCS4 *possible_letters;

    mask = 0;
    key_index = -1;

    for (stroke_index = 0; stroke_index < stroke_len; ++stroke_index)
    {
        letter = stroke_ucs4[stroke_index];
        if (letter == '-')
        {
            if (key_index > (int)helper->right_keys_index)
                return INVALID_STROKE;
            key_index = helper->right_keys_index - 1;
            continue;
        }
        if ('0' <= letter && letter <= '9')
        {
            mask |= helper->number_key_mask;
            possible_letters = helper->key_number;
        }
        else
        {
            possible_letters = helper->key_letter;
        }
        do
        {
            if (++key_index == (int)helper->num_keys)
                return INVALID_STROKE;
        } while (letter != possible_letters[key_index]);
        mask |= STROKE_1 << key_index;
    }

    return mask;
}

static stroke_uint_t stroke_from_int(const stroke_helper_t *helper, PyObject *integer)
{
    stroke_uint_t mask = PyLong_AsStrokeUint(integer);

    if ((mask >> helper->num_keys))
    {
        PyErr_Format(PyExc_ValueError, "invalid keys mask: %R", integer);
        return INVALID_STROKE;
    }

    return mask;
}

static stroke_uint_t stroke_from_keys(const stroke_helper_t *helper, PyObject *keys_sequence) \
{
    stroke_uint_t  mask;
    PyObject      *key;
    Py_UCS4        key_letter;
    key_side_t     key_side;
    const Py_UCS4 *possible_letters;
    unsigned       k, k_end;

    mask = 0;

    for (Py_ssize_t num_keys = PySequence_Fast_GET_SIZE(keys_sequence); num_keys--; )
    {
        key = PySequence_Fast_GET_ITEM(keys_sequence, num_keys);
        if (!PyUnicode_Check(key))
        {
            PyErr_Format(PyExc_ValueError, "invalid `keys`; key %u is not a string: %R", num_keys, key);
            return INVALID_STROKE;
        }

        key_letter = key_to_letter(key, &key_side);
        if (!key_letter)
        {
            PyErr_Format(PyExc_ValueError, "invalid `keys`; key %u is not valid: %R", num_keys, key);
            return INVALID_STROKE;
        }

        if ('0' <= key_letter && key_letter <= '9')
        {
            mask |= helper->number_key_mask;
            possible_letters = helper->key_number;
        }
        else
        {
            possible_letters = helper->key_letter;
        }

        switch (key_side)
        {
        case KEY_SIDE_NONE:
            k = 0;
            k_end = helper->num_keys;
            break;
        case KEY_SIDE_LEFT:
            k = 0;
            k_end = helper->right_keys_index;
            break;
        case KEY_SIDE_RIGHT:
            k = helper->right_keys_index;
            k_end = helper->num_keys;
            break;
        default:
            UNREACHABLE();
        }

        for (;;)
        {
           if (key_letter == possible_letters[k] && key_side == helper->key_side[k])
           {
               mask |= STROKE_1 << k;
               break;
           }

           if (++k == k_end)
           {
               PyErr_Format(PyExc_ValueError, "invalid key: %R", key);
               return INVALID_STROKE;
           }
        }
    }

    return mask;
}

static stroke_uint_t stroke_from_steno(const stroke_helper_t *helper, PyObject *steno)
{
    Py_ssize_t     steno_len;
    int            steno_kind;
    const void    *steno_data;
    Py_UCS4        steno_ucs4[MAX_STENO];
    stroke_uint_t  mask;

    if (PyUnicode_READY(steno))
        return INVALID_STROKE;

    steno_len = PyUnicode_GET_LENGTH(steno);
    if (steno_len > MAX_STENO)
        goto invalid;

    steno_kind = PyUnicode_KIND(steno);
    steno_data = PyUnicode_DATA(steno);

    for (Py_ssize_t index = 0; index < steno_len; ++index)
        steno_ucs4[index] = PyUnicode_READ(steno_kind, steno_data, index);

    mask = stroke_from_ucs4(helper, steno_ucs4, steno_len);
    if (mask == INVALID_STROKE)
        goto invalid;

    return mask;

invalid:
    PyErr_Format(PyExc_ValueError, "invalid steno: %R", steno);
    return INVALID_STROKE;
}

static stroke_uint_t stroke_from_any(const stroke_helper_t *helper, PyObject *obj)
{
    if (PyLong_Check(obj))
        return stroke_from_int(helper, obj);

    if (PyUnicode_Check(obj))
        return stroke_from_steno(helper, obj);

    obj = PySequence_Fast(obj, "expected a list or tuple");
    if (obj != NULL)
        return stroke_from_keys(helper, obj);

    PyErr_Format(PyExc_TypeError,
                 "expected an integer (mask of keys), "
                 "sequence of keys, or a string (steno), "
                 "got: %R", obj);
    return INVALID_STROKE;
}

static int stroke_has_digit(const stroke_helper_t *helper, stroke_uint_t mask)
{
    return (mask & helper->number_key_mask) && (mask & helper->numbers_mask);
}

static int stroke_is_number(const stroke_helper_t *helper, stroke_uint_t mask)
{
    // Must have the number key, at least one digit, and no other non-digit key.
    return (mask & helper->number_key_mask) && mask > helper->number_key_mask && mask == (mask & (helper->number_key_mask | helper->numbers_mask));
}

static int unpack_2_strokes(const stroke_helper_t *helper, PyObject *args, const char *fn_name, stroke_uint_t *first_stroke, stroke_uint_t *second_stroke)
{
    PyObject *s1, *s2;

    if (!PyArg_UnpackTuple(args, fn_name, 2, 2, &s1, &s2))
        return 0;

    *first_stroke = stroke_from_any(helper, s1);
    if (*first_stroke == INVALID_STROKE)
        return 0;

    *second_stroke = stroke_from_any(helper, s2);
    if (*second_stroke == INVALID_STROKE)
        return 0;

    return 1;
}

static PyObject *stroke_to_keys(const stroke_helper_t *helper, stroke_uint_t mask)
{
    PyObject *stroke_keys[MAX_KEYS];
    unsigned  stroke_index;
    unsigned  key_index;
    PyObject *keys_tuple;
    Py_UCS4   key_ucs4[2];
    unsigned  key_ucs4_len;
    PyObject *key;

    for (stroke_index = key_index = 0; mask; ++key_index, mask >>= 1)
    {
        if ((mask & 1))
        {
            switch (helper->key_side[key_index])
            {
            case KEY_SIDE_NONE:
                key_ucs4[0] = helper->key_letter[key_index];
                key_ucs4_len = 1;
                break;
            case KEY_SIDE_LEFT:
                key_ucs4[0] = helper->key_letter[key_index];
                key_ucs4[1] = '-';
                key_ucs4_len = 2;
                break;
            case KEY_SIDE_RIGHT:
                key_ucs4[0] = '-';
                key_ucs4[1] = helper->key_letter[key_index];
                key_ucs4_len = 2;
                break;
            default:
                UNREACHABLE();
            }
            key = PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, key_ucs4, key_ucs4_len);
            if (key == NULL)
                goto error;
            stroke_keys[stroke_index++] = key;
        }
    }

    keys_tuple = PyTuple_New(stroke_index);
    if (keys_tuple == NULL)
        goto error;

    while (stroke_index--)
        PyTuple_SET_ITEM(keys_tuple, stroke_index, stroke_keys[stroke_index]);

    return keys_tuple;

error:
    while (stroke_index--)
        Py_DECREF(stroke_keys[stroke_index]);
    return NULL;
}

static PyObject *stroke_to_str(const stroke_helper_t *helper, stroke_uint_t mask)
{
    const Py_UCS4 *letters;
    unsigned       key_index;
    unsigned       hyphen_index;
    unsigned       stroke_index;
    Py_UCS4        stroke[MAX_STENO];

    if (stroke_has_digit(helper, mask))
    {
        mask &= ~helper->number_key_mask;
        letters = helper->key_number;
    }
    else
    {
        letters = helper->key_letter;
    }

    if ((mask & helper->implicit_hyphen_mask))
        hyphen_index = MAX_KEYS;
    else
        hyphen_index = helper->right_keys_index;

    for (stroke_index = key_index = 0; mask; ++key_index, mask >>= 1)
    {
        if ((mask & 1))
        {
            if (key_index >= hyphen_index)
            {
                stroke[stroke_index++] = '-';
                hyphen_index = MAX_KEYS;
            }
            stroke[stroke_index++] = letters[key_index];
        }
    }

    return PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, stroke, stroke_index);
}

unsigned stroke_to_sort_key(const stroke_helper_t *helper, stroke_uint_t mask, char *sort_key)
{
    unsigned key_num;
    unsigned sort_key_index;

    for (key_num = sort_key_index = 0; mask; mask >>= 1)
    {
        ++key_num;
        if ((mask & 1))
            sort_key[sort_key_index++] = key_num;
    }

    return sort_key_index;
}

static PyObject *stroke_cmp(const stroke_helper_t *helper, PyObject *args, const char *fn_name, cmp_op_t op)
{
    stroke_uint_t si1, si2, lsb1, lsb2, m;
    stroke_int_t  c;
    int           b;

    if (!unpack_2_strokes(helper, args, fn_name, &si1, &si2))
        return NULL;

    c = 0;
    while (si1 != si2)
    {
        lsb1 = lsb(si1);
        lsb2 = lsb(si2);
        c = lsb1 - lsb2;
        if (c)
            break;
        m = ~lsb1;
        si1 &= m;
        si2 &= m;
    }

    switch (op)
    {
    case CMP_OP_CMP:
        return PyLong_FromStrokeInt(c);
    case CMP_OP_EQ:
        b = c == 0;
        break;
    case CMP_OP_NE:
        b = c != 0;
        break;
    case CMP_OP_GE:
        b = c >= 0;
        break;
    case CMP_OP_GT:
        b = c > 0;
        break;
    case CMP_OP_LE:
        b = c <= 0;
        break;
    case CMP_OP_LT:
        b = c < 0;
        break;
    default:
        UNREACHABLE();
    }

    if (b)
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *normalize_stroke_ucs4(const stroke_helper_t *helper,
                                       const Py_UCS4         *stroke_ucs4,
                                       Py_ssize_t             stroke_len)
{
    stroke_uint_t mask;

    mask = stroke_from_ucs4(helper, stroke_ucs4, stroke_len);
    if (mask == INVALID_STROKE)
        return NULL;

    return stroke_to_str(helper, mask);
}

static PyObject *key_str(const stroke_helper_t *helper, unsigned key_index)
{
    Py_UCS4  key_ucs4[2];
    unsigned key_ucs4_len;

    switch (helper->key_side[key_index])
    {
    case KEY_SIDE_NONE:
        key_ucs4[0] = helper->key_letter[key_index];
        key_ucs4_len = 1;
        break;
    case KEY_SIDE_LEFT:
        key_ucs4[0] = helper->key_letter[key_index];
        key_ucs4[1] = '-';
        key_ucs4_len = 2;
        break;
    case KEY_SIDE_RIGHT:
        key_ucs4[0] = '-';
        key_ucs4[1] = helper->key_letter[key_index];
        key_ucs4_len = 2;
        break;
    default:
        UNREACHABLE();
    }

    return PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, key_ucs4, key_ucs4_len);
}

static PyObject *StrokeHelper_setup(StrokeHelper *self, PyObject *args, PyObject *kwargs)
{
    static char *kwlist[] = {"keys", "implicit_hyphen_keys", "number_key", "numbers", NULL};

    PyObject        *implicit_hyphen_keys = Py_None;
    PyObject        *number_key = Py_None;
    PyObject        *numbers = Py_None;
    PyObject        *keys_sequence;
    Py_ssize_t       num_keys;
    stroke_uint_t    unique_letters_mask;
    Py_UCS4          number_key_letter;
    PyObject        *key;
    Py_UCS4          key_letter;
    stroke_uint_t    key_mask;
    key_side_t       key_side;
    stroke_helper_t  helper;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "O|OOO", kwlist,
                                     &keys_sequence, &implicit_hyphen_keys,
                                     &number_key, &numbers))
        return NULL;

    keys_sequence = PySequence_Fast(keys_sequence, "expected `keys` to be a list or tuple");
    if (keys_sequence == NULL)
        return NULL;

    num_keys = PySequence_Fast_GET_SIZE(keys_sequence);
    if (num_keys == 0 || num_keys > MAX_KEYS)
    {
        PyErr_SetString(PyExc_ValueError, "unsupported number of keys");
        return NULL;
    }

    if (number_key == Py_None)
    {
        if (numbers != Py_None)
        {
            PyErr_SetString(PyExc_TypeError, "expected `numbers` to be None (since `number_key` is None)");
            return NULL;
        }

        number_key_letter = 0;
        numbers = NULL;
    }
    else
    {
        if (!PyUnicode_Check(number_key))
        {
            PyErr_SetString(PyExc_TypeError, "expected `number_key` to be a string");
            return NULL;
        }

        number_key_letter = key_to_letter(number_key, &key_side);
        if (!number_key_letter)
        {
            PyErr_SetString(PyExc_ValueError, "invalid `number_key`");
            return NULL;
        }

        if (!PyDict_Check(numbers))
        {
            PyErr_SetString(PyExc_TypeError, "expected `numbers` to be a dictionary");
            return NULL;
        }
    }

    if (implicit_hyphen_keys != Py_None && !PySet_Check(implicit_hyphen_keys))
    {
        PyErr_SetString(PyExc_TypeError, "expected `implicit_hyphen_keys` to be a set");
        return NULL;
    }

    helper.num_keys = (unsigned)num_keys;
    helper.right_keys_index = helper.num_keys;
    helper.implicit_hyphen_mask = 0;
    helper.number_key_mask = 0;
    helper.numbers_mask = 0;

    for (unsigned k = 0; k < helper.num_keys; ++k)
    {
        key = PySequence_Fast_GET_ITEM(keys_sequence, k);
        if (!PyUnicode_Check(key))
        {
            PyErr_Format(PyExc_ValueError, "invalid `keys`; key %u is not a string: %R", k, key);
            return NULL;
        }

        key_letter = key_to_letter(key, &key_side);
        if (!key_letter)
        {
            PyErr_Format(PyExc_ValueError, "invalid `keys`; key %u is not valid: %R", k, key);
            return NULL;
        }

        key_mask = STROKE_1 << k;

        switch (key_side)
        {
        case KEY_SIDE_NONE:
            break;
        case KEY_SIDE_LEFT:
            if (helper.right_keys_index != helper.num_keys)
            {
                PyErr_Format(PyExc_ValueError, "invalid `keys`; left-key on the right-hand side: %R", key);
                return NULL;
            }
            break;
        case KEY_SIDE_RIGHT:
            if (helper.right_keys_index == helper.num_keys)
                helper.right_keys_index = k;
            break;
        default:
            UNREACHABLE();
        }

        if (key_letter == number_key_letter)
            helper.number_key_mask = key_mask;

        if (implicit_hyphen_keys != Py_None && PySet_Contains(implicit_hyphen_keys, key))
            helper.implicit_hyphen_mask |= key_mask;

        helper.key_side[k] = key_side;
        helper.key_letter[k] = key_letter;

        if (number_key_letter)
        {
            number_key = PyDict_GetItem(numbers, key);
            if (number_key != NULL)
            {
                key_letter = key_to_letter(number_key, &key_side);
                if (!key_letter)
                {
                    PyErr_Format(PyExc_ValueError, "invalid `numbers`; entry for %R is not valid: %R", key, number_key);
                    return NULL;
                }
                helper.numbers_mask |= key_mask;
            }
        }

        helper.key_number[k] = key_letter;
    }

    if (number_key_letter)
    {
        if (!helper.number_key_mask)
        {
            PyErr_SetString(PyExc_ValueError, "invalid `number_key`");
            return NULL;
        }

        if (popcount(helper.numbers_mask) != 10)
        {
            PyErr_SetString(PyExc_ValueError, "invalid `numbers`");
            return NULL;
        }
    }

    // Find out unique letters.
    unique_letters_mask = 0;
    {
        unsigned k, l;

        for (k = 0; k < helper.num_keys; ++k)
        {
            for (l = 0; l < helper.num_keys; ++l)
                if (l != k && helper.key_letter[l] == helper.key_letter[k])
                    break;
            if (l == helper.num_keys)
                unique_letters_mask |= (STROKE_1 << k);
        }
    }

    if (implicit_hyphen_keys != Py_None)
    {
        if ((Py_ssize_t)popcount(helper.implicit_hyphen_mask) != PySet_GET_SIZE(implicit_hyphen_keys))
        {
            PyErr_SetString(PyExc_ValueError, "invalid `implicit_hyphen_keys`: not all keys accounted for");
            return NULL;
        }

        // Implicit hyphen keys must be a continuous block.
        if (helper.implicit_hyphen_mask != (// Mask of all bits <= msb.
                                            ((msb(helper.implicit_hyphen_mask) << 1) - 1) &
                                            // Mask of all bits >= lsb.
                                            ~(lsb(helper.implicit_hyphen_mask) - 1)))
        {
            PyErr_SetString(PyExc_ValueError, "invalid `implicit_hyphen_keys`: not a continuous block");
            return NULL;
        }

        if ((helper.implicit_hyphen_mask & unique_letters_mask) != helper.implicit_hyphen_mask)
        {
            PyErr_SetString(PyExc_ValueError, "invalid `implicit_hyphen_keys`: some letters are not unique");
            return NULL;
        }
    }
    else
    {
        unsigned k, l;

        for (k = helper.right_keys_index; k && (unique_letters_mask & (STROKE_1 << --k)); )
            ;
        for (l = helper.right_keys_index; l < helper.num_keys && (unique_letters_mask & (STROKE_1 << l)); ++l)
            ;

        helper.implicit_hyphen_mask = unique_letters_mask & ~((STROKE_1 << k) - 1) & ((STROKE_1 << l) - 1);
    }

    self->helper = helper;

    Py_RETURN_NONE;
}

static PyObject *StrokeHelper_get_letters(const StrokeHelper *self, void *Py_UNUSED(closure))
{
    return PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, self->helper.key_letter, self->helper.num_keys);
}

static PyObject *StrokeHelper_get_numbers(const StrokeHelper *self, void *Py_UNUSED(closure))
{
    return PyUnicode_FromKindAndData(PyUnicode_4BYTE_KIND, self->helper.key_number, self->helper.num_keys);
}

static PyObject *StrokeHelper_get_keys(const StrokeHelper *self, void *Py_UNUSED(closure))
{
    PyObject *keys_tuple;
    PyObject *key;

    keys_tuple = PyTuple_New(self->helper.num_keys);
    if (keys_tuple == NULL)
        return NULL;

    for (unsigned k = 0; k < self->helper.num_keys; ++k)
    {
        key = key_str(&self->helper, k);
        if (key == NULL)
        {
            Py_DECREF(keys_tuple);
            return NULL;
        }
        PyTuple_SET_ITEM(keys_tuple, k, key);
    }

    return keys_tuple;
}

#define STROKE_CMP_FN(FnName, Op) \
    static PyObject *StrokeHelper_##FnName(const StrokeHelper *self, PyObject *args) \
    { \
        return stroke_cmp(&self->helper, args, #FnName, Op); \
    }

STROKE_CMP_FN(stroke_cmp, CMP_OP_CMP);
STROKE_CMP_FN(stroke_eq, CMP_OP_EQ);
STROKE_CMP_FN(stroke_ne, CMP_OP_NE);
STROKE_CMP_FN(stroke_ge, CMP_OP_GE);
STROKE_CMP_FN(stroke_gt, CMP_OP_GT);
STROKE_CMP_FN(stroke_le, CMP_OP_LE);
STROKE_CMP_FN(stroke_lt, CMP_OP_LT);

#undef STROKE_CMP_FN

static PyObject *StrokeHelper_stroke_in(const StrokeHelper *self, PyObject *args)
{
    stroke_uint_t mask1, mask2;

    if (!unpack_2_strokes(&self->helper, args, "stroke_in", &mask1, &mask2))
        return NULL;

    if ((mask1 & mask2) == mask1)
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *StrokeHelper_stroke_or(const StrokeHelper *self, PyObject *args)
{
    stroke_uint_t mask1, mask2;

    if (!unpack_2_strokes(&self->helper, args, "stroke_or", &mask1, &mask2))
        return NULL;

    return PyLong_FromStrokeUint(mask1 | mask2);
}

static PyObject *StrokeHelper_stroke_and(const StrokeHelper *self, PyObject *args)
{
    stroke_uint_t mask1, mask2;

    if (!unpack_2_strokes(&self->helper, args, "stroke_and", &mask1, &mask2))
        return NULL;

    return PyLong_FromStrokeUint(mask1 & mask2);
}

static PyObject *StrokeHelper_stroke_add(const StrokeHelper *self, PyObject *args)
{
    stroke_uint_t mask1, mask2;

    if (!unpack_2_strokes(&self->helper, args, "stroke_add", &mask1, &mask2))
        return NULL;

    return PyLong_FromStrokeUint(mask1 | mask2);
}

static PyObject *StrokeHelper_stroke_sub(const StrokeHelper *self, PyObject *args)
{
    stroke_uint_t mask1, mask2;

    if (!unpack_2_strokes(&self->helper, args, "stroke_sub", &mask1, &mask2))
        return NULL;

    return PyLong_FromStrokeUint(mask1 & ~mask2);
}

static PyObject *StrokeHelper_stroke_is_prefix(const StrokeHelper *self, PyObject *args)
{
    stroke_uint_t mask1, mask2;

    if (!unpack_2_strokes(&self->helper, args, "stroke_is_prefix", &mask1, &mask2))
        return NULL;

    if (msb(mask1) < lsb(mask2))
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *StrokeHelper_stroke_is_suffix(const StrokeHelper *self, PyObject *args)
{
    stroke_uint_t mask1, mask2;

    if (!unpack_2_strokes(&self->helper, args, "stroke_is_suffix", &mask1, &mask2))
        return NULL;

    if (lsb(mask1) > msb(mask2))
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *StrokeHelper_normalize_stroke(const StrokeHelper *self, PyObject *stroke)
{
    Py_ssize_t  stroke_len;
    int         stroke_kind;
    const void *stroke_data;
    Py_UCS4     stroke_ucs4[MAX_STENO];
    PyObject   *normalized_stroke;

    if (!PyUnicode_Check(stroke))
    {
        PyErr_SetString(PyExc_TypeError, "expected a string");
        return NULL;
    }

    if (PyUnicode_READY(stroke))
        return NULL;

    stroke_len = PyUnicode_GET_LENGTH(stroke);
    if (!stroke_len || stroke_len > MAX_STENO)
        goto invalid;

    stroke_kind = PyUnicode_KIND(stroke);
    stroke_data = PyUnicode_DATA(stroke);

    for (Py_ssize_t index = 0; index < stroke_len; ++index)
        stroke_ucs4[index] = PyUnicode_READ(stroke_kind, stroke_data, index);

    normalized_stroke = normalize_stroke_ucs4(&self->helper, stroke_ucs4, stroke_len);
    if (normalized_stroke == NULL)
        goto invalid;

    return normalized_stroke;

invalid:
    PyErr_Format(PyExc_ValueError, "invalid stroke: %R", stroke);
    return NULL;
}

static PyObject *StrokeHelper_normalize_steno(const StrokeHelper *self, PyObject *steno)
{
    int          steno_kind;
    const void  *steno_data;
    Py_ssize_t   steno_len;
    Py_ssize_t   steno_index;
    Py_UCS4      stroke_ucs4[MAX_STENO + 1]; // Account for '/'.
    Py_ssize_t   stroke_len;
    PyObject    *stroke;
    Py_ssize_t   max_strokes;
    PyObject   **strokes_list;
    Py_ssize_t   num_strokes;
    PyObject    *result;

    strokes_list = NULL;
    result = NULL;

    if (!PyUnicode_Check(steno))
    {
        PyErr_SetString(PyExc_TypeError, "expected a string");
        goto end;
    }

    if (PyUnicode_READY(steno))
        goto end;

    steno_len = PyUnicode_GET_LENGTH(steno);
    if (!steno_len)
    {
        result = PyTuple_New(0);
        goto end;
    }

    max_strokes = steno_len / 2 + 1;

    strokes_list = PyMem_Malloc(max_strokes * sizeof (*strokes_list));
    if (strokes_list == NULL)
    {
        PyErr_NoMemory();
        goto end;
    }

    steno_kind = PyUnicode_KIND(steno);
    steno_data = PyUnicode_DATA(steno);

    num_strokes = 0;
    steno_index = 0;
    stroke_len = 0;

    while (1)
    {
        stroke_ucs4[stroke_len] = PyUnicode_READ(steno_kind, steno_data, steno_index);
        if (stroke_ucs4[stroke_len] == '/')
        {
            // No trailing '/' allowed.
            if (++steno_index == steno_len)
                goto invalid;
            if (!stroke_len)
            {
                // Allow one '/' at the start.
                if (num_strokes)
                    goto invalid;
                stroke = PyUnicode_New(0, 0);
                if (stroke == NULL)
                    goto error;
                strokes_list[num_strokes++] = stroke;
                continue;
            }
        }
        else if (++stroke_len > MAX_STENO)
            goto invalid;
        else if (++steno_index < steno_len)
            continue;
        stroke = normalize_stroke_ucs4(&self->helper, stroke_ucs4, stroke_len);
        if (stroke == NULL)
            goto invalid;
        assert(num_strokes < max_strokes);
        strokes_list[num_strokes++] = stroke;
        if (steno_index == steno_len)
            break;
        stroke_len = 0;
    }

    result = PyTuple_New(num_strokes);
    if (result == NULL)
        goto error;

    while (num_strokes--)
        PyTuple_SET_ITEM(result, num_strokes, strokes_list[num_strokes]);

    goto end;

invalid:
    PyErr_Format(PyExc_ValueError, "invalid steno: %R", steno);
error:
    while (num_strokes--)
        Py_XDECREF(strokes_list[num_strokes]);
end:
    PyMem_Free(strokes_list);
    return result;
}

static PyObject *StrokeHelper_steno_to_sort_key(const StrokeHelper *self, PyObject *steno)
{
    int            steno_kind;
    const void    *steno_data;
    Py_ssize_t     steno_len;
    Py_ssize_t     steno_index;
    Py_UCS4        stroke_ucs4[MAX_STENO + 1]; // Account for '/'.
    Py_ssize_t     stroke_len;
    stroke_uint_t  mask;
    char          *sort_key;
    Py_ssize_t     sort_key_index;
    Py_ssize_t     sort_key_max_len;
    PyObject      *result;

    sort_key = NULL;
    result = NULL;

    if (!PyUnicode_Check(steno))
    {
        PyErr_SetString(PyExc_TypeError, "expected a string");
        goto end;
    }

    if (PyUnicode_READY(steno))
        goto end;

    steno_len = PyUnicode_GET_LENGTH(steno);
    if (!steno_len)
        goto invalid;

    // Note: account for possible extra hyphens.
    sort_key_max_len = steno_len * 2;

    sort_key = PyMem_Malloc(sort_key_max_len * sizeof (*sort_key));
    if (sort_key == NULL)
    {
        PyErr_NoMemory();
        goto end;
    }

    steno_kind = PyUnicode_KIND(steno);
    steno_data = PyUnicode_DATA(steno);

    sort_key_index = 0;
    steno_index = 0;
    stroke_len = 0;
    while (1)
    {
        stroke_ucs4[stroke_len] = PyUnicode_READ(steno_kind, steno_data, steno_index);
        if (stroke_ucs4[stroke_len] == '/')
        {
            // No trailing '/' allowed.
            if (++steno_index == steno_len)
                goto invalid;
            if (!stroke_len)
            {
                // Allow one '/' at the start.
                if (sort_key_index)
                    goto invalid;
                sort_key[sort_key_index++] = 0;
                continue;
            }
        }
        else if (++stroke_len > MAX_STENO)
            goto invalid;
        else if (++steno_index < steno_len)
            continue;
        mask = stroke_from_ucs4(&self->helper, stroke_ucs4, stroke_len);
        if (mask == INVALID_STROKE)
            goto invalid;
        sort_key_index += stroke_to_sort_key(&self->helper, mask, &sort_key[sort_key_index]);
        if (steno_index == steno_len)
            break;
        sort_key[sort_key_index++] = 0;
        stroke_len = 0;
    }
    assert(sort_key_index <= sort_key_max_len);

    result = PyBytes_FromStringAndSize(sort_key, sort_key_index);

    goto end;

invalid:
    PyErr_Format(PyExc_ValueError, "invalid steno: %R", steno);
end:
    PyMem_Free(sort_key);
    return result;
}

static PyObject *StrokeHelper_stroke_from_any(const StrokeHelper *self, PyObject *obj)
{
    stroke_uint_t mask;

    mask = stroke_from_any(&self->helper, obj);
    if (mask == INVALID_STROKE)
        return NULL;

    return PyLong_FromStrokeUint(mask);
}

static PyObject *StrokeHelper_stroke_from_int(const StrokeHelper *self, PyObject *integer)
{
    stroke_uint_t mask;

    mask = stroke_from_int(&self->helper, integer);
    if (mask == INVALID_STROKE)
        return NULL;

    return PyLong_FromStrokeUint(mask);
}

static PyObject *StrokeHelper_stroke_from_keys(const StrokeHelper *self, PyObject *keys_sequence)
{
    stroke_uint_t mask;

    keys_sequence = PySequence_Fast(keys_sequence, "expected a list or tuple");
    if (keys_sequence == NULL)
        return NULL;

    mask = stroke_from_keys(&self->helper, keys_sequence);
    if (mask == INVALID_STROKE)
        return NULL;

    return PyLong_FromStrokeUint(mask);
}

static PyObject *StrokeHelper_stroke_from_steno(const StrokeHelper *self, PyObject *steno)
{
    stroke_uint_t mask;

    if (!PyUnicode_Check(steno))
    {
        PyErr_SetString(PyExc_TypeError, "expected a string");
        return NULL;
    }

    mask = stroke_from_steno(&self->helper, steno);
    if (mask == INVALID_STROKE)
        return NULL;

    return PyLong_FromStrokeUint(mask);
}

static PyObject *StrokeHelper_stroke_to_keys(const StrokeHelper *self, PyObject *stroke)
{
    stroke_uint_t mask;

    mask = stroke_from_any(&self->helper, stroke);
    if (mask == INVALID_STROKE)
        return NULL;

    return stroke_to_keys(&self->helper, mask);
}

static PyObject *StrokeHelper_stroke_first_key(const StrokeHelper *self, PyObject *stroke)
{
    stroke_uint_t mask;
    unsigned      first_key;

    mask = stroke_from_any(&self->helper, stroke);
    if (mask == INVALID_STROKE)
        return NULL;

    if (!mask)
    {
        PyErr_SetString(PyExc_ValueError, "empty stroke");
        return NULL;
    }

    first_key = popcount(lsb(mask) - 1);

    return key_str(&self->helper, first_key);
}

static PyObject *StrokeHelper_stroke_last_key(const StrokeHelper *self, PyObject *stroke)
{
    stroke_uint_t mask;
    unsigned      last_key;

    mask = stroke_from_any(&self->helper, stroke);
    if (mask == INVALID_STROKE)
        return NULL;

    if (!mask)
    {
        PyErr_SetString(PyExc_ValueError, "empty stroke");
        return NULL;
    }

    last_key = popcount(msb(mask) - 1);

    return key_str(&self->helper, last_key);
}

static PyObject *StrokeHelper_stroke_invert(const StrokeHelper *self, PyObject *stroke)
{
    stroke_uint_t mask;

    mask = stroke_from_any(&self->helper, stroke);
    if (mask == INVALID_STROKE)
        return NULL;

    mask = ~mask & ((STROKE_1 << self->helper.num_keys) - 1);

    return PyLong_FromStrokeUint(mask);
}

static PyObject *StrokeHelper_stroke_len(const StrokeHelper *self, PyObject *stroke)
{
    stroke_uint_t mask;

    mask = stroke_from_any(&self->helper, stroke);
    if (mask == INVALID_STROKE)
        return NULL;

    return PyLong_FromStrokeInt(popcount(mask));
}

static PyObject *StrokeHelper_stroke_has_digit(const StrokeHelper *self, PyObject *stroke)
{
    stroke_uint_t mask;

    mask = stroke_from_any(&self->helper, stroke);
    if (mask == INVALID_STROKE)
        return NULL;

    if (stroke_has_digit(&self->helper, mask))
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *StrokeHelper_stroke_is_number(const StrokeHelper *self, PyObject *stroke)
{
    stroke_uint_t mask;

    mask = stroke_from_any(&self->helper, stroke);
    if (mask == INVALID_STROKE)
        return NULL;

    if (stroke_is_number(&self->helper, mask))
        Py_RETURN_TRUE;

    Py_RETURN_FALSE;
}

static PyObject *StrokeHelper_stroke_to_steno(const StrokeHelper *self, PyObject *stroke)
{
    stroke_uint_t mask;

    mask = stroke_from_any(&self->helper, stroke);
    if (mask == INVALID_STROKE)
        return NULL;

    return stroke_to_str(&self->helper, mask);
}

static PyObject *StrokeHelper_stroke_to_sort_key(const StrokeHelper *self, PyObject *stroke)
{
    char          sort_key[MAX_KEYS];
    unsigned      sort_key_len;
    stroke_uint_t mask;

    mask = stroke_from_any(&self->helper, stroke);
    if (mask == INVALID_STROKE)
        return NULL;

    sort_key_len = stroke_to_sort_key(&self->helper, mask, sort_key);

    return PyBytes_FromStringAndSize(sort_key, sort_key_len);
}

static PyGetSetDef StrokeHelper_getset[] =
{
    {"keys", (getter)StrokeHelper_get_keys, NULL, "List of supported keys.", NULL},
    {"letters", (getter)StrokeHelper_get_letters, NULL, "Letters for the supported keys.", NULL},
    {"numbers", (getter)StrokeHelper_get_numbers, NULL, "Numbers for the supported keys.", NULL},
    {NULL}
};

static PyMemberDef StrokeHelper_members[] =
{
    {"num_keys"            , T_UINT       , offsetof(StrokeHelper, helper.num_keys)            , READONLY, "Number of keys."},
    {"implicit_hyphen_mask", T_STROKE_UINT, offsetof(StrokeHelper, helper.implicit_hyphen_mask), READONLY, "Implicit hyphen mask."},
    {"number_key_mask"     , T_STROKE_UINT, offsetof(StrokeHelper, helper.number_key_mask)     , READONLY, "Number key mask."},
    {"numbers_mask"        , T_STROKE_UINT, offsetof(StrokeHelper, helper.numbers_mask)        , READONLY, "Numbers mask."},
    {"right_keys_index"    , T_UINT       , offsetof(StrokeHelper, helper.right_keys_index)    , READONLY, "Right keys index."},
    {NULL}
};

static PyMethodDef StrokeHelper_methods[] =
{
    {"setup"             , (PyCFunction)StrokeHelper_setup             , METH_VARARGS | METH_KEYWORDS, "Setup."},
    // Steno.
    {"normalize_stroke"  , (PyCFunction)StrokeHelper_normalize_stroke  , METH_O, "Normalize stroke."},
    {"normalize_steno"   , (PyCFunction)StrokeHelper_normalize_steno   , METH_O, "Normalize steno."},
    {"steno_to_sort_key" , (PyCFunction)StrokeHelper_steno_to_sort_key , METH_O, "Convert steno to a binary sort key."},
    // Stroke: new.
    {"stroke_from_any"   , (PyCFunction)StrokeHelper_stroke_from_any   , METH_O, "Convert an integer (keys mask), string (steno), or sequence of keys to a stroke."},
    {"stroke_from_int"   , (PyCFunction)StrokeHelper_stroke_from_int   , METH_O, "Convert an integer (keys mask) to a stroke."},
    {"stroke_from_keys"  , (PyCFunction)StrokeHelper_stroke_from_keys  , METH_O, "Convert keys to a stroke."},
    {"stroke_from_steno" , (PyCFunction)StrokeHelper_stroke_from_steno , METH_O, "Convert steno to a stroke."},
    // Stroke: methods.
    {"stroke_first_key"  , (PyCFunction)StrokeHelper_stroke_first_key  , METH_O, "Return the stroke first key."},
    {"stroke_last_key"   , (PyCFunction)StrokeHelper_stroke_last_key   , METH_O, "Return the stroke last key."},
    {"stroke_invert"     , (PyCFunction)StrokeHelper_stroke_invert     , METH_O, "Invert stroke."},
    {"stroke_len"        , (PyCFunction)StrokeHelper_stroke_len        , METH_O, "Return the stroke number of keys."},
    {"stroke_has_digit"  , (PyCFunction)StrokeHelper_stroke_has_digit  , METH_O, "Return True if the stroke contains one or more digits."},
    {"stroke_is_number"  , (PyCFunction)StrokeHelper_stroke_is_number  , METH_O, "Return True if the stroke is a number."},
    // Stroke: ops.
    {"stroke_cmp"        , (PyCFunction)StrokeHelper_stroke_cmp        , METH_VARARGS, "Compare strokes."},
    {"stroke_eq"         , (PyCFunction)StrokeHelper_stroke_eq         , METH_VARARGS, "Compare strokes: `s1 == s2`."},
    {"stroke_ne"         , (PyCFunction)StrokeHelper_stroke_ne         , METH_VARARGS, "Compare strokes: `s1 != s2`."},
    {"stroke_ge"         , (PyCFunction)StrokeHelper_stroke_ge         , METH_VARARGS, "Compare strokes: `s1 >= s2`."},
    {"stroke_gt"         , (PyCFunction)StrokeHelper_stroke_gt         , METH_VARARGS, "Compare strokes: `s1 > s2`."},
    {"stroke_le"         , (PyCFunction)StrokeHelper_stroke_le         , METH_VARARGS, "Compare strokes: `s1 <= s2`."},
    {"stroke_lt"         , (PyCFunction)StrokeHelper_stroke_lt         , METH_VARARGS, "Compare strokes: `s1 < s2`."},
    {"stroke_in"         , (PyCFunction)StrokeHelper_stroke_in         , METH_VARARGS, "`s1 in s2."},
    {"stroke_or"         , (PyCFunction)StrokeHelper_stroke_or         , METH_VARARGS, "`s1 | s2."},
    {"stroke_and"        , (PyCFunction)StrokeHelper_stroke_and        , METH_VARARGS, "`s1 & s2."},
    {"stroke_add"        , (PyCFunction)StrokeHelper_stroke_add        , METH_VARARGS, "`s1 + s2."},
    {"stroke_sub"        , (PyCFunction)StrokeHelper_stroke_sub        , METH_VARARGS, "`s1 - s2."},
    {"stroke_is_prefix"  , (PyCFunction)StrokeHelper_stroke_is_prefix  , METH_VARARGS, "Check if `s1` is a prefix of `s2`."},
    {"stroke_is_suffix"  , (PyCFunction)StrokeHelper_stroke_is_suffix  , METH_VARARGS, "Check if `s1` is a suffix of `s2`."},
    // Stroke: convert.
    {"stroke_to_keys"    , (PyCFunction)StrokeHelper_stroke_to_keys    , METH_O, "Convert stroke to a tuple of keys."},
    {"stroke_to_steno"   , (PyCFunction)StrokeHelper_stroke_to_steno   , METH_O, "Convert stroke to steno."},
    {"stroke_to_sort_key", (PyCFunction)StrokeHelper_stroke_to_sort_key, METH_O, "Convert stroke to a binary sort key."},
    {NULL}
};

static PyTypeObject StrokeHelperType =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name      = "stroke_helper.StrokeHelper",
    .tp_basicsize = sizeof (StrokeHelper),
    .tp_itemsize  = 0,
    .tp_flags     = Py_TPFLAGS_DEFAULT,
    .tp_new       = PyType_GenericNew,
    .tp_methods   = StrokeHelper_methods,
    .tp_members   = StrokeHelper_members,
    .tp_getset    = StrokeHelper_getset,
};

static struct PyModuleDef module =
{
    PyModuleDef_HEAD_INIT,
    .m_name = "_plover_stroke",
    .m_size = -1,
};

PyMODINIT_FUNC PyInit__plover_stroke(void)
{
    PyObject *m;

    if (PyType_Ready(&StrokeHelperType) < 0)
        return NULL;

    m = PyModule_Create(&module);
    if (m == NULL)
        return NULL;

    Py_INCREF(&StrokeHelperType);

    if (PyModule_AddObject(m, "StrokeHelper", (PyObject *)&StrokeHelperType) < 0)
    {
        Py_DECREF(&StrokeHelperType);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}
