//     Copyright 2016, Kay Hayen, mailto:kay.hayen@gmail.com
//
//     Part of "Nuitka", an optimizing Python compiler that is compatible and
//     integrates with CPython, but also works on its own.
//
//     Licensed under the Apache License, Version 2.0 (the "License");
//     you may not use this file except in compliance with the License.
//     You may obtain a copy of the License at
//
//        http://www.apache.org/licenses/LICENSE-2.0
//
//     Unless required by applicable law or agreed to in writing, software
//     distributed under the License is distributed on an "AS IS" BASIS,
//     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
//     See the License for the specific language governing permissions and
//     limitations under the License.
//
/* Implementations of compiled code helpers.

 * The definition of a compiled code helper is that it's being used in
 * generated C code and provides part of the operations implementation.
 *
 * Currently we also have standalone mode related code here, patches to CPython
 * runtime that we do, and e.g. the built-in module. TODO: Move these to their
 * own files for clarity.
 */


#include "nuitka/prelude.h"

extern PyObject *const_str_plain_compile;

NUITKA_DEFINE_BUILTIN( compile )

#if PYTHON_VERSION < 300
PyObject *COMPILE_CODE( PyObject *source_code, PyObject *file_name, PyObject *mode, PyObject *flags, PyObject *dont_inherit )
#else
PyObject *COMPILE_CODE( PyObject *source_code, PyObject *file_name, PyObject *mode, PyObject *flags, PyObject *dont_inherit, PyObject *optimize )
#endif
{
    // May be a source, but also could already be a compiled object, in which
    // case this should just return it.
    if ( PyCode_Check( source_code ) )
    {
        return INCREASE_REFCOUNT( source_code );
    }

    PyObject *pos_args = PyTuple_New(3);
    PyTuple_SET_ITEM( pos_args, 0, INCREASE_REFCOUNT( source_code ) );
    PyTuple_SET_ITEM( pos_args, 1, INCREASE_REFCOUNT( file_name ) );
    PyTuple_SET_ITEM( pos_args, 2, INCREASE_REFCOUNT( mode ) );

    PyObject *kw_args = NULL;

    if ( flags != NULL )
    {
        if ( kw_args == NULL ) kw_args = PyDict_New();
        PyDict_SetItemString( kw_args, "flags", flags );
    }

    if ( dont_inherit != NULL )
    {
        if ( kw_args == NULL ) kw_args = PyDict_New();
        PyDict_SetItemString( kw_args, "dont_inherit", dont_inherit );
    }

#if PYTHON_VERSION >= 300
    if ( optimize != NULL )
    {
        if ( kw_args == NULL ) kw_args = PyDict_New();
        PyDict_SetItemString( kw_args, "optimize", optimize );
    }
#endif

    NUITKA_ASSIGN_BUILTIN( compile );

    PyObject *result = CALL_FUNCTION(
        NUITKA_ACCESS_BUILTIN( compile ),
        pos_args,
        kw_args
    );

    Py_DECREF( pos_args );
    Py_XDECREF( kw_args );

    return result;
}

PyObject *EVAL_CODE( PyObject *code, PyObject *globals, PyObject *locals )
{
    CHECK_OBJECT( code );
    CHECK_OBJECT( globals );
    CHECK_OBJECT( locals );

    if ( PyDict_Check( globals ) == 0 )
    {
        PyErr_Format( PyExc_TypeError, "exec: arg 2 must be a dictionary or None" );
        return NULL;
    }

    // TODO: Our re-formulation prevents this externally, doesn't it.
    if ( locals == Py_None )
    {
        locals = globals;
    }

    if ( PyMapping_Check( locals ) == 0 )
    {
        PyErr_Format( PyExc_TypeError, "exec: arg 3 must be a mapping or None" );
        return NULL;
    }

    // Set the __builtins__ in globals, it is expected to be present.
    if ( PyDict_GetItem( globals, const_str_plain___builtins__ ) == NULL )
    {
        if ( PyDict_SetItem( globals, const_str_plain___builtins__, (PyObject *)builtin_module ) != 0 )
        {
            return NULL;
        }
    }

#if PYTHON_VERSION < 300
    PyObject *result = PyEval_EvalCode( (PyCodeObject *)code, globals, locals );
#else
    PyObject *result = PyEval_EvalCode( code, globals, locals );
#endif

    if (unlikely( result == NULL ))
    {
        return NULL;
    }

    return result;
}

NUITKA_DEFINE_BUILTIN( open );

PyObject *BUILTIN_OPEN( PyObject *file_name, PyObject *mode, PyObject *buffering )
{
    NUITKA_ASSIGN_BUILTIN( open );

    if ( file_name == NULL )
    {
        return CALL_FUNCTION_NO_ARGS(
            NUITKA_ACCESS_BUILTIN( open )
        );
    }
    else if ( mode == NULL )
    {
        PyObject *args[] = {
            file_name
        };
        return CALL_FUNCTION_WITH_ARGS1(
            NUITKA_ACCESS_BUILTIN( open ),
            args
        );

    }
    else if ( buffering == NULL )
    {
        PyObject *args[] = {
            file_name,
            mode
        };
        return CALL_FUNCTION_WITH_ARGS2(
            NUITKA_ACCESS_BUILTIN( open ),
            args
        );
    }
    else
    {
        PyObject *args[] = {
            file_name,
            mode,
            buffering
        };
        return CALL_FUNCTION_WITH_ARGS3(
            NUITKA_ACCESS_BUILTIN( open ),
            args
        );
    }
}

PyObject *STRING_FROM_CHAR( unsigned char c )
{
    // TODO: A switch statement might be faster, because no object needs to be
    // created at all, this here is how CPython does it.
    char s[1];
    s[0] = (char)c;

#if PYTHON_VERSION < 300
    return PyString_FromStringAndSize( s, 1 );
#else
    return PyUnicode_FromStringAndSize( s, 1 );
#endif
}

PyObject *BUILTIN_CHR( PyObject *value )
{
    long x = PyInt_AsLong( value );

#if PYTHON_VERSION < 300
    if ( x < 0 || x >= 256 )
    {
        PyErr_Format( PyExc_ValueError, "chr() arg not in range(256)" );
        return NULL;
    }

    // TODO: A switch statement might be faster, because no object needs to be
    // created at all, this is how CPython does it.
    char s[1];
    s[0] = (char)x;

    return PyString_FromStringAndSize( s, 1 );
#else
    PyObject *result = PyUnicode_FromOrdinal( x );

    if (unlikely( result == NULL ))
    {
        return NULL;
    }

    assert( PyUnicode_Check( result ));

    return result;
#endif
}

PyObject *BUILTIN_ORD( PyObject *value )
{
    long result;

    if (likely( PyBytes_Check( value ) ))
    {
        Py_ssize_t size = PyBytes_GET_SIZE( value );

        if (likely( size == 1 ))
        {
            result = (long)( ((unsigned char *)PyBytes_AS_STRING( value ))[0] );
        }
        else
        {
            PyErr_Format( PyExc_TypeError, "ord() expected a character, but string of length %zd found", size );
            return NULL;
        }
    }
    else if ( PyByteArray_Check( value ) )
    {
        Py_ssize_t size = PyByteArray_GET_SIZE( value );

        if (likely( size == 1 ))
        {
            result = (long)( ((unsigned char *)PyByteArray_AS_STRING( value ))[0] );
        }
        else
        {
            PyErr_Format( PyExc_TypeError, "ord() expected a character, but byte array of length %zd found", size );
            return NULL;
        }
    }
    else if ( PyUnicode_Check( value ) )
    {
#if PYTHON_VERSION >= 330
        if (unlikely( PyUnicode_READY( value ) == -1 ))
        {
            return NULL;
        }

        Py_ssize_t size = PyUnicode_GET_LENGTH( value );
#else
        Py_ssize_t size = PyUnicode_GET_SIZE( value );
#endif

        if (likely( size == 1 ))
        {
#if PYTHON_VERSION >= 330
            result = (long)( PyUnicode_READ_CHAR( value, 0 ) );
#else
            result = (long)( *PyUnicode_AS_UNICODE( value ) );
#endif
        }
        else
        {
            PyErr_Format( PyExc_TypeError, "ord() expected a character, but unicode string of length %zd found", size );
            return NULL;
        }
    }
    else
    {
        PyErr_Format( PyExc_TypeError, "ord() expected string of length 1, but %s found", Py_TYPE( value )->tp_name );
        return NULL;
    }

    return PyInt_FromLong( result );
}

PyObject *BUILTIN_BIN( PyObject *value )
{
    // Note: I don't really know why ord and hex don't use this as well.
    PyObject *result = PyNumber_ToBase( value, 2 );

    if ( unlikely( result == NULL ))
    {
        return NULL;
    }

    return result;
}

PyObject *BUILTIN_OCT( PyObject *value )
{
#if PYTHON_VERSION >= 300
    PyObject *result = PyNumber_ToBase( value, 8 );

    if ( unlikely( result == NULL ))
    {
        return NULL;
    }

    return result;
#else
    if (unlikely( value == NULL ))
    {
        PyErr_Format( PyExc_TypeError, "oct() argument can't be converted to oct" );
        return NULL;
    }

    PyNumberMethods *nb = Py_TYPE( value )->tp_as_number;

    if (unlikely( nb == NULL || nb->nb_oct == NULL ))
    {
        PyErr_Format( PyExc_TypeError, "oct() argument can't be converted to oct" );
        return NULL;
    }

    PyObject *result = (*nb->nb_oct)( value );

    if ( result )
    {
        if (unlikely( !PyString_Check( result ) ))
        {
            PyErr_Format( PyExc_TypeError, "__oct__ returned non-string (type %s)", Py_TYPE( result )->tp_name );

            Py_DECREF( result );
            return NULL;
        }
    }

    return result;
#endif
}

PyObject *BUILTIN_HEX( PyObject *value )
{
#if PYTHON_VERSION >= 300
    PyObject *result = PyNumber_ToBase( value, 16 );

    if ( unlikely( result == NULL ))
    {
        return NULL;
    }

    return result;
#else
    if (unlikely( value == NULL ))
    {
        PyErr_Format( PyExc_TypeError, "hex() argument can't be converted to hex" );
        return NULL;
    }

    PyNumberMethods *nb = Py_TYPE( value )->tp_as_number;

    if (unlikely( nb == NULL || nb->nb_hex == NULL ))
    {
        PyErr_Format( PyExc_TypeError, "hex() argument can't be converted to hex" );
        return NULL;
    }

    PyObject *result = (*nb->nb_hex)( value );

    if (likely( result ))
    {
        if (unlikely( !PyString_Check( result ) ))
        {
            PyErr_Format( PyExc_TypeError, "__hex__ returned non-string (type %s)", Py_TYPE( result )->tp_name );

            Py_DECREF( result );
            return NULL;
        }
    }

    return result;
#endif
}

PyObject *BUILTIN_HASH( PyObject *value )
{
    Py_hash_t hash = PyObject_Hash( value );

    if (unlikely( hash == -1 ))
    {
        return NULL;
    }

#if PYTHON_VERSION < 300
    return PyInt_FromLong( hash );
#else
    return PyLong_FromSsize_t( hash );
#endif
}

PyObject *BUILTIN_BYTEARRAY( PyObject *value )
{
    PyObject *result = PyByteArray_FromObject( value );

    if ( unlikely( result == NULL ))
    {
        return NULL;
    }

    return result;
}


// From CPython:
typedef struct {
    PyObject_HEAD
    PyObject *it_callable;
    PyObject *it_sentinel;
} calliterobject;

PyObject *BUILTIN_ITER2( PyObject *callable, PyObject *sentinel )
{
    calliterobject *result = PyObject_GC_New( calliterobject, &PyCallIter_Type );

    if (unlikely( result == NULL ))
    {
        return NULL;
    }

    // Note: References were taken at call site already.
    result->it_callable = INCREASE_REFCOUNT( callable );
    result->it_sentinel = INCREASE_REFCOUNT( sentinel );

    Nuitka_GC_Track( result );

    return (PyObject *)result;
}

PyObject *BUILTIN_TYPE1( PyObject *arg )
{
    return INCREASE_REFCOUNT( (PyObject *)Py_TYPE( arg ) );
}

extern PyObject *const_str_plain___module__;

PyObject *BUILTIN_TYPE3( PyObject *module_name, PyObject *name, PyObject *bases, PyObject *dict )
{
    PyObject *pos_args = PyTuple_New(3);
    PyTuple_SET_ITEM( pos_args, 0, INCREASE_REFCOUNT( name ) );
    PyTuple_SET_ITEM( pos_args, 1, INCREASE_REFCOUNT( bases ) );
    PyTuple_SET_ITEM( pos_args, 2, INCREASE_REFCOUNT( dict ) );

    PyObject *result = PyType_Type.tp_new(
        &PyType_Type,
        pos_args,
        NULL
    );


    if (unlikely( result == NULL ))
    {
        Py_DECREF( pos_args );
        return NULL;
    }

    PyTypeObject *type = Py_TYPE( result );

    if (likely( PyType_IsSubtype( type, &PyType_Type ) ))
    {
        if (
#if PYTHON_VERSION < 300
            PyType_HasFeature( type, Py_TPFLAGS_HAVE_CLASS ) &&
#endif
            type->tp_init != NULL
           )
        {
            int res = type->tp_init( result, pos_args, NULL );

            if (unlikely( res < 0 ))
            {
                Py_DECREF( pos_args );
                Py_DECREF( result );
                return NULL;
            }
        }
    }

    Py_DECREF( pos_args );

    int res = PyObject_SetAttr( result, const_str_plain___module__, module_name );

    if ( res < 0 )
    {
        return NULL;
    }

    return result;
}

Py_ssize_t ESTIMATE_RANGE( long low, long high, long step )
{
    if ( low >= high )
    {
        return 0;
    }
    else
    {
        return ( high - low - 1 ) / step + 1;
    }
}

#if PYTHON_VERSION < 300
static PyObject *_BUILTIN_RANGE_INT3( long low, long high, long step )
{
    assert( step != 0 );

    Py_ssize_t size;

    if ( step > 0 )
    {
        size = ESTIMATE_RANGE( low, high, step );
    }
    else
    {
        size = ESTIMATE_RANGE( high, low, -step );
    }

    PyObject *result = PyList_New( size );

    long current = low;

    for ( int i = 0; i < size; i++ )
    {
        PyList_SET_ITEM( result, i, PyInt_FromLong( current ) );
        current += step;
    }

    return result;
}

static PyObject *_BUILTIN_RANGE_INT2( long low, long high )
{
    return _BUILTIN_RANGE_INT3( low, high, 1 );
}

static PyObject *_BUILTIN_RANGE_INT( long boundary )
{
    return _BUILTIN_RANGE_INT2( 0, boundary );
}

static PyObject *TO_RANGE_ARG( PyObject *value, char const *name )
{
    if (likely( PyInt_Check( value ) || PyLong_Check( value ) ))
    {
        return INCREASE_REFCOUNT( value );
    }

    PyTypeObject *type = Py_TYPE( value );
    PyNumberMethods *tp_as_number = type->tp_as_number;

    // Everything that casts to int is allowed.
    if (
#if PYTHON_VERSION >= 270
        PyFloat_Check( value ) ||
#endif
        tp_as_number == NULL ||
        tp_as_number->nb_int == NULL
       )
    {
        PyErr_Format( PyExc_TypeError, "range() integer %s argument expected, got %s.", name, type->tp_name );
        return NULL;
    }

    PyObject *result = tp_as_number->nb_int( value );

    if (unlikely( result == NULL ))
    {
        return NULL;
    }

    return result;
}
#endif

NUITKA_DEFINE_BUILTIN( range );

#if PYTHON_VERSION < 300
PyObject *BUILTIN_RANGE( PyObject *boundary )
{
    PyObject *boundary_temp = TO_RANGE_ARG( boundary, "end" );

    if (unlikely( boundary_temp == NULL ))
    {
        return NULL;
    }

    long start = PyInt_AsLong( boundary_temp );

    if ( start == -1 && ERROR_OCCURRED() )
    {
        CLEAR_ERROR_OCCURRED();

        PyObject *args[] = { boundary_temp };

        NUITKA_ASSIGN_BUILTIN( range );

        PyObject *result = CALL_FUNCTION_WITH_ARGS1(
            NUITKA_ACCESS_BUILTIN( range ),
            args
        );

        Py_DECREF( boundary_temp );

        return result;
    }
    Py_DECREF( boundary_temp );

    return _BUILTIN_RANGE_INT( start );
}

PyObject *BUILTIN_RANGE2( PyObject *low, PyObject *high )
{
    PyObject *low_temp = TO_RANGE_ARG( low, "start" );

    if (unlikely( low_temp == NULL ))
    {
        return NULL;
    }

    PyObject *high_temp = TO_RANGE_ARG( high, "end" );

    if (unlikely( high_temp == NULL ))
    {
        Py_DECREF( low_temp );
        return NULL;
    }

    bool fallback = false;

    long start = PyInt_AsLong( low_temp );

    if (unlikely( start == -1 && ERROR_OCCURRED() ))
    {
        CLEAR_ERROR_OCCURRED();
        fallback = true;
    }

    long end = PyInt_AsLong( high_temp );

    if (unlikely( end == -1 && ERROR_OCCURRED() ))
    {
        CLEAR_ERROR_OCCURRED();
        fallback = true;
    }

    if ( fallback )
    {
        PyObject *pos_args = PyTuple_New( 2 );
        PyTuple_SET_ITEM( pos_args, 0, low_temp );
        PyTuple_SET_ITEM( pos_args, 1, high_temp );

        NUITKA_ASSIGN_BUILTIN( range );

        PyObject *result = CALL_FUNCTION_WITH_POSARGS(
            NUITKA_ACCESS_BUILTIN( range ),
            pos_args
        );

        Py_DECREF( pos_args );

        return result;
    }
    else
    {
        Py_DECREF( low_temp );
        Py_DECREF( high_temp );

        return _BUILTIN_RANGE_INT2( start, end );
    }
}

PyObject *BUILTIN_RANGE3( PyObject *low, PyObject *high, PyObject *step )
{
    PyObject *low_temp = TO_RANGE_ARG( low, "start" );

    if (unlikely( low_temp == NULL ))
    {
        return NULL;
    }

    PyObject *high_temp = TO_RANGE_ARG( high, "end" );

    if (unlikely( high_temp == NULL ))
    {
        Py_DECREF( low_temp );
        return NULL;
    }

    PyObject *step_temp = TO_RANGE_ARG( step, "step" );

    if (unlikely( high_temp == NULL ))
    {
        Py_DECREF( low_temp );
        Py_DECREF( high_temp );
        return NULL;
    }

    bool fallback = false;

    long start = PyInt_AsLong( low_temp );

    if (unlikely( start == -1 && ERROR_OCCURRED() ))
    {
        CLEAR_ERROR_OCCURRED();
        fallback = true;
    }

    long end = PyInt_AsLong( high_temp );

    if (unlikely( end == -1 && ERROR_OCCURRED() ))
    {
        CLEAR_ERROR_OCCURRED();
        fallback = true;
    }

    long step_long = PyInt_AsLong( step_temp );

    if (unlikely( step_long == -1 && ERROR_OCCURRED() ))
    {
        CLEAR_ERROR_OCCURRED();
        fallback = true;
    }

    if ( fallback )
    {
        PyObject *pos_args = PyTuple_New( 3 );
        PyTuple_SET_ITEM( pos_args, 0, low_temp );
        PyTuple_SET_ITEM( pos_args, 1, high_temp );
        PyTuple_SET_ITEM( pos_args, 2, step_temp );

        NUITKA_ASSIGN_BUILTIN( range );

        PyObject *result = CALL_FUNCTION_WITH_POSARGS(
            NUITKA_ACCESS_BUILTIN( range ),
            pos_args
        );

        Py_DECREF( pos_args );

        return result;
    }
    else
    {
        Py_DECREF( low_temp );
        Py_DECREF( high_temp );
        Py_DECREF( step_temp );

        if (unlikely( step_long == 0 ))
        {
            PyErr_Format( PyExc_ValueError, "range() step argument must not be zero" );
            return NULL;
        }

        return _BUILTIN_RANGE_INT3( start, end, step_long );
    }
}

#endif

#if PYTHON_VERSION < 300
NUITKA_DEFINE_BUILTIN( xrange );
#endif

/* Built-in xrange (Python2) or xrange (Python3) with one argument. */
PyObject *BUILTIN_XRANGE1( PyObject *low )
{
    PyObject *args[1] = {
        low
    };

#if PYTHON_VERSION < 300
    NUITKA_ASSIGN_BUILTIN( xrange );

    return CALL_FUNCTION_WITH_ARGS1(
        NUITKA_ACCESS_BUILTIN( xrange ),
        args
    );
#else
    NUITKA_ASSIGN_BUILTIN( range );

    return CALL_FUNCTION_WITH_ARGS1(
        NUITKA_ACCESS_BUILTIN( range ),
        args
    );
#endif
}

/* Built-in xrange (Python2) or xrange (Python3) with two arguments. */
PyObject *BUILTIN_XRANGE2( PyObject *low, PyObject *high )
{
    PyObject *args[2] = {
        low,
        high
    };

#if PYTHON_VERSION < 300
    NUITKA_ASSIGN_BUILTIN( xrange );

    return CALL_FUNCTION_WITH_ARGS2(
        NUITKA_ACCESS_BUILTIN( xrange ),
        args
    );
#else
    NUITKA_ASSIGN_BUILTIN( range );

    return CALL_FUNCTION_WITH_ARGS2(
        NUITKA_ACCESS_BUILTIN( range ),
        args
    );
#endif
}

/* Built-in xrange (Python2) or xrange (Python3) with three arguments. */
PyObject *BUILTIN_XRANGE3( PyObject *low, PyObject *high, PyObject *step )
{
    PyObject *args[] = {
        low,
        high,
        step
    };

#if PYTHON_VERSION < 300
    NUITKA_ASSIGN_BUILTIN( xrange );

    return CALL_FUNCTION_WITH_ARGS3(
        NUITKA_ACCESS_BUILTIN( xrange ),
        args
    );
#else
    NUITKA_ASSIGN_BUILTIN( range );

    return CALL_FUNCTION_WITH_ARGS3(
        NUITKA_ACCESS_BUILTIN( range ),
        args
    );

#endif
}

PyObject *BUILTIN_LEN( PyObject *value )
{
    CHECK_OBJECT( value );

    Py_ssize_t res = PyObject_Size( value );

    if (unlikely( res < 0 && ERROR_OCCURRED() ))
    {
        return NULL;
    }

    return PyInt_FromSsize_t( res );
}

NUITKA_DEFINE_BUILTIN( format );

PyObject *BUILTIN_FORMAT( PyObject *value, PyObject *format_spec )
{
    CHECK_OBJECT( value );
    CHECK_OBJECT( format_spec );

    NUITKA_ASSIGN_BUILTIN( format );

    PyObject *args[2] = {
        value,
        format_spec
    };

    return CALL_FUNCTION_WITH_ARGS2(
        NUITKA_ACCESS_BUILTIN( format ),
        args
    );
}


NUITKA_DEFINE_BUILTIN( __import__ );

PyObject *IMPORT_MODULE( PyObject *module_name, PyObject *globals, PyObject *locals, PyObject *import_items, PyObject *level )
{
    CHECK_OBJECT( module_name );
    CHECK_OBJECT( globals );
    CHECK_OBJECT( locals );
    CHECK_OBJECT( import_items );
    CHECK_OBJECT( level );

    PyObject *pos_args = PyTuple_New(5);
    PyTuple_SET_ITEM( pos_args, 0, INCREASE_REFCOUNT( module_name ) );
    PyTuple_SET_ITEM( pos_args, 1, INCREASE_REFCOUNT( globals ) );
    PyTuple_SET_ITEM( pos_args, 2, INCREASE_REFCOUNT( locals ) );
    PyTuple_SET_ITEM( pos_args, 3, INCREASE_REFCOUNT( import_items ) );
    PyTuple_SET_ITEM( pos_args, 4, INCREASE_REFCOUNT( level ) );

    NUITKA_ASSIGN_BUILTIN( __import__ );

    PyObject *import_result = CALL_FUNCTION_WITH_POSARGS(
        NUITKA_ACCESS_BUILTIN( __import__ ),
        pos_args
    );

    Py_DECREF( pos_args );

    return import_result;
}

extern PyObject *const_str_plain___all__;

bool IMPORT_MODULE_STAR( PyObject *target, bool is_module, PyObject *module )
{
    // Check parameters.
    CHECK_OBJECT( module );
    CHECK_OBJECT( target );

    PyObject *iter;
    bool all_case;

    PyObject *all = PyObject_GetAttr( module, const_str_plain___all__ );
    if ( all != NULL )
    {
        iter = MAKE_ITERATOR( all );
        Py_DECREF( all );

        if (unlikely( iter == NULL ))
        {
            return false;
        }

        all_case = true;
    }
    else if ( EXCEPTION_MATCH_BOOL_SINGLE( GET_ERROR_OCCURRED(), PyExc_AttributeError ) )
    {
        CLEAR_ERROR_OCCURRED();

        iter = MAKE_ITERATOR( PyModule_GetDict( module ) );
        CHECK_OBJECT( iter );

        all_case = false;
    }
    else
    {
        return false;
    }


    for(;;)
    {
        PyObject *item = ITERATOR_NEXT( iter );
        if ( item == NULL ) break;

        assert( Nuitka_String_Check( item ) );

        // TODO: Not yet clear, what happens with __all__ and "_" of its
        // contents.
        if ( all_case == false )
        {
            if ( Nuitka_String_AsString_Unchecked( item )[0] == '_' )
            {
                continue;
            }
        }

        // TODO: What if it isn't there, because of e.g. wrong __all__ value.
        PyObject *value = LOOKUP_ATTRIBUTE( module, item );

        // TODO: Check if the reference is handled correctly
        if ( is_module )
        {
            SET_ATTRIBUTE( target, item,  value );
        }
        else
        {
            SET_SUBSCRIPT( target, item, value );
        }

        Py_DECREF( value );
        Py_DECREF( item );
    }

    Py_DECREF( iter );

    return !ERROR_OCCURRED();
}

// Helper functions for print. Need to play nice with Python softspace
// behaviour.

#if PYTHON_VERSION >= 300
extern PyObject *const_str_plain_end;
extern PyObject *const_str_plain_file;
extern PyObject *const_str_empty;

NUITKA_DEFINE_BUILTIN( print );
#endif


bool PRINT_NEW_LINE_TO( PyObject *file )
{
#if PYTHON_VERSION < 300
    if ( file == NULL || file == Py_None )
    {
        file = GET_STDOUT();
    }

    // need to hold a reference to the file or else __getattr__ may release
    // "file" in the mean time.
    Py_INCREF( file );

    if (unlikely( PyFile_WriteString( "\n", file ) == -1))
    {
        Py_DECREF( file );
        return false;
    }

    PyFile_SoftSpace( file, 0 );
    CHECK_OBJECT( file );

    Py_DECREF( file );
    return true;
#else
    NUITKA_ASSIGN_BUILTIN( print );

    if (likely( file == NULL ))
    {
        PyObject *result = CALL_FUNCTION_NO_ARGS(
            NUITKA_ACCESS_BUILTIN( print )
        );
        Py_XDECREF( result );
        return result != NULL;
    }
    else
    {
        PyObject *kw_args = PyDict_New();
        PyDict_SetItem( kw_args, const_str_plain_file, GET_STDOUT() );

        PyObject *result = CALL_FUNCTION_WITH_KEYARGS(
            NUITKA_ACCESS_BUILTIN( print ),
            kw_args
        );

        Py_DECREF( kw_args );
        Py_XDECREF( result );

        return result != NULL;
    }
#endif
}


bool PRINT_ITEM_TO( PyObject *file, PyObject *object )
{
// The print built-in function cannot replace "softspace" behavior of CPython
// print statement, so this code is really necessary.
#if PYTHON_VERSION < 300
    if ( file == NULL || file == Py_None )
    {
        file = GET_STDOUT();
    }

    CHECK_OBJECT( file );
    CHECK_OBJECT( object );

    // need to hold a reference to the file or else "__getattr__" code may
    // release "file" in the mean time.
    Py_INCREF( file );

    // Check for soft space indicator
    if ( PyFile_SoftSpace( file, 0 ) )
    {
        if (unlikely( PyFile_WriteString( " ", file ) == -1 ))
        {
            Py_DECREF( file );
            return false;
        }
    }

    if ( unlikely( PyFile_WriteObject( object, file, Py_PRINT_RAW ) == -1 ))
    {
        Py_DECREF( file );
        return false;
    }

    if ( PyString_Check( object ) )
    {
        char *buffer;
        Py_ssize_t length;

#ifndef __NUITKA_NO_ASSERT__
        int status =
#endif
            PyString_AsStringAndSize( object, &buffer, &length );
        assert( status != -1 );

        if ( length == 0 || !isspace( Py_CHARMASK(buffer[length-1])) || buffer[ length - 1 ] == ' ' )
        {
            PyFile_SoftSpace( file, 1 );
        }
    }
    else if ( PyUnicode_Check( object ) )
    {
        Py_UNICODE *buffer = PyUnicode_AS_UNICODE( object );
        Py_ssize_t length = PyUnicode_GET_SIZE( object );

        if ( length == 0 || !Py_UNICODE_ISSPACE( buffer[length-1]) || buffer[length-1] == ' ' )
        {
            PyFile_SoftSpace( file, 1 );
        }
    }
    else
    {
        PyFile_SoftSpace( file, 1 );
    }

    CHECK_OBJECT( file );
    Py_DECREF( file );

    return true;
#else
    NUITKA_ASSIGN_BUILTIN( print );

    if (likely( file == NULL ))
    {
        PyObject *args[] = { object };

        PyObject *result = CALL_FUNCTION_WITH_ARGS1(
            NUITKA_ACCESS_BUILTIN( print ),
            args
        );

        Py_XDECREF( result );

        return result != NULL;
    }
    else
    {
        PyObject *print_kw = PyDict_New();
        PyDict_SetItem( print_kw, const_str_plain_end, const_str_empty );
        PyDict_SetItem( print_kw, const_str_plain_file, GET_STDOUT() );

        PyObject *print_args = PyTuple_New( 1 );
        PyTuple_SET_ITEM( print_args, 0, INCREASE_REFCOUNT( object ) );

        PyObject *res = CALL_FUNCTION(
            NUITKA_ACCESS_BUILTIN( print ),
            print_args,
            print_kw
        );

        Py_DECREF( print_args );
        Py_DECREF( print_kw );

        Py_XDECREF( res );

        return res != NULL;
    }
#endif
}

void PRINT_REFCOUNT( PyObject *object )
{
    char buffer[ 1024 ];
    sprintf( buffer, " refcnt %" PY_FORMAT_SIZE_T "d ", Py_REFCNT( object ) );

    PRINT_STRING( buffer );
}

bool PRINT_STRING( char const *str )
{
    int res = PyFile_WriteString( str, GET_STDOUT() );

    return res != -1;
}

bool PRINT_REPR( PyObject *object )
{
    bool res;

    if ( object != NULL )
    {
        PyObject *repr = PyObject_Repr( object );
        res = PRINT_ITEM( repr );
        Py_DECREF( repr );
    }
    else
    {
        res = PRINT_NULL();
    }

    return res;
}

bool PRINT_NULL( void )
{
    return PRINT_STRING("<NULL>");
}

void PRINT_EXCEPTION( PyObject *exception_type, PyObject *exception_value, PyObject *exception_tb )
{
    PRINT_REPR( exception_type );
    PRINT_STRING("|");
    PRINT_REPR( exception_value );
#if PYTHON_VERSION >= 300
    if ( exception_value != NULL )
    {
        PRINT_STRING(" <- ");
        PRINT_REPR( PyException_GetContext( exception_value ) );
    }
#endif
    PRINT_STRING("|");
    PRINT_REPR( exception_tb );

    PRINT_NEW_LINE();
}

// TODO: Could be ported, the "printf" stuff would need to be split. On Python3
// the normal C print output gets lost.
#if PYTHON_VERSION < 300
void PRINT_TRACEBACK( PyTracebackObject *traceback )
{
    PRINT_STRING("Dumping traceback:\n");

    if ( traceback == NULL ) PRINT_STRING( "<NULL traceback?!>\n" );

    while( traceback )
    {
        printf( " line %d (frame object chain):\n", traceback->tb_lineno );

        PyFrameObject *frame = traceback->tb_frame;

        while ( frame )
        {
            printf( "  Frame at %s\n", PyString_AsString( PyObject_Str( (PyObject *)frame->f_code )));

            frame = frame->f_back;
        }

        assert( traceback->tb_next != traceback );
        traceback = traceback->tb_next;
    }

    PRINT_STRING("End of Dump.\n");
}
#endif


PyObject *GET_STDOUT()
{
    PyObject *result = PySys_GetObject( (char *)"stdout" );

    if (unlikely( result == NULL ))
    {
        PyErr_Format( PyExc_RuntimeError, "lost sys.stdout" );
        return NULL;
    }

    return result;
}

PyObject *GET_STDERR()
{
    PyObject *result = PySys_GetObject( (char *)"stderr" );

    if (unlikely( result == NULL ))
    {
        PyErr_Format( PyExc_RuntimeError, "lost sys.stderr" );
        return NULL;
    }

    return result;
}

bool PRINT_NEW_LINE( void )
{
    PyObject *target = GET_STDOUT();

    return target != NULL && PRINT_NEW_LINE_TO( target );
}

bool PRINT_ITEM( PyObject *object )
{
    PyObject *target = GET_STDOUT();

    return target != NULL && PRINT_ITEM_TO( target, object );
}



PyObject *UNSTREAM_CONSTANT( unsigned char const *buffer, Py_ssize_t size )
{
    assert( buffer );

    // We unstream difficult constant objects using the "pickle" module, this is
    // aimed at being the exception, e.g. unicode that doesn't fit into UTF-8
    // will be dealt with like this.
    static PyObject *module_pickle = NULL;

    if ( module_pickle == NULL )
    {
#if PYTHON_VERSION < 300
        module_pickle = PyImport_ImportModule( "cPickle" );
#else
        module_pickle = PyImport_ImportModule( "pickle" );
#endif
        if (unlikely( module_pickle == NULL ))
        {
            PyErr_Print();
        }

        assert( module_pickle );
    }

    static PyObject *function_pickle_loads = NULL;

    if ( function_pickle_loads == NULL )
    {
        function_pickle_loads = PyObject_GetAttrString(
            module_pickle,
            "loads"
        );

        if (unlikely( function_pickle_loads == NULL ))
        {
            PyErr_Print();
        }

        assert( function_pickle_loads );
    }

    PyObject *result = PyObject_CallFunction(
        function_pickle_loads,
#if PYTHON_VERSION < 300
        (char *)"(s#)", // TODO: Why the ()
#else
        (char *)"y#",
#endif
        buffer,
        size
    );

    if (unlikely( result == NULL ))
    {
        PyErr_Print();
    }

    CHECK_OBJECT( result );

    return result;
}

#if PYTHON_VERSION < 300
PyObject *UNSTREAM_UNICODE( unsigned char const *buffer, Py_ssize_t size )
{
    PyObject *result = PyUnicode_FromStringAndSize( (char const  *)buffer, size );

    assert( !ERROR_OCCURRED() );
    CHECK_OBJECT( result );

    return result;
}
#endif

PyObject *UNSTREAM_STRING( unsigned char const *buffer, Py_ssize_t size, bool intern )
{
#if PYTHON_VERSION < 300
    PyObject *result = PyString_FromStringAndSize( (char const  *)buffer, size );
#else
    PyObject *result = PyUnicode_FromStringAndSize( (char const  *)buffer, size );
#endif

    assert( !ERROR_OCCURRED() );
    CHECK_OBJECT( result );
    assert( Nuitka_String_Check( result ) );

#if PYTHON_VERSION < 300
    assert( PyString_Size( result ) == size );
#endif

    if ( intern )
    {
        Nuitka_StringIntern( &result );

        CHECK_OBJECT( result );
        assert( Nuitka_String_Check( result ) );

#if PYTHON_VERSION < 300
        assert( PyString_Size( result ) == size );
#else
        assert( PyUnicode_GET_SIZE( result ) == size );
#endif
    }

    return result;
}

PyObject *UNSTREAM_CHAR( unsigned char value, bool intern )
{
#if PYTHON_VERSION < 300
    PyObject *result = PyString_FromStringAndSize( (char const  *)&value, 1 );
#else
    PyObject *result = PyUnicode_FromStringAndSize( (char const  *)&value, 1 );
#endif

    assert( !ERROR_OCCURRED() );
    CHECK_OBJECT( result );
    assert( Nuitka_String_Check( result ) );

#if PYTHON_VERSION < 300
    assert( PyString_Size( result ) == 1 );
#else
    assert( PyUnicode_GET_SIZE( result ) == 1 );
#endif

    if ( intern )
    {
        Nuitka_StringIntern( &result );

        CHECK_OBJECT( result );
        assert( Nuitka_String_Check( result ) );

#if PYTHON_VERSION < 300
        assert( PyString_Size( result ) == 1 );
#else
        assert( PyUnicode_GET_SIZE( result ) == 1 );
#endif
    }

    return result;
}

PyObject *UNSTREAM_FLOAT( unsigned char const *buffer )
{
    double x = _PyFloat_Unpack8( buffer, 1 );
    assert( x != -1.0 || !PyErr_Occurred() );

    PyObject *result = PyFloat_FromDouble(x);
    assert( result != NULL );

    return result;
}

#if PYTHON_VERSION >= 300
PyObject *UNSTREAM_BYTES( unsigned char const *buffer, Py_ssize_t size )
{
    PyObject *result = PyBytes_FromStringAndSize( (char const  *)buffer, size );
    assert( !ERROR_OCCURRED() );
    CHECK_OBJECT( result );

    assert( PyBytes_GET_SIZE( result ) == size );
    return result;
}
#endif


#if PYTHON_VERSION < 300

static void set_slot( PyObject **slot, PyObject *value )
{
    PyObject *temp = *slot;
    Py_XINCREF( value );
    *slot = value;
    Py_XDECREF( temp );
}

extern PyObject *const_str_plain___getattr__;
extern PyObject *const_str_plain___setattr__;
extern PyObject *const_str_plain___delattr__;

static void set_attr_slots( PyClassObject *klass )
{
    set_slot( &klass->cl_getattr, FIND_ATTRIBUTE_IN_CLASS( klass, const_str_plain___getattr__ ) );
    set_slot( &klass->cl_setattr, FIND_ATTRIBUTE_IN_CLASS( klass, const_str_plain___setattr__ ) );
    set_slot( &klass->cl_delattr, FIND_ATTRIBUTE_IN_CLASS( klass, const_str_plain___delattr__ ) );
}

static bool set_dict( PyClassObject *klass, PyObject *value )
{
    if ( value == NULL || !PyDict_Check( value ) )
    {
        PyErr_SetString( PyExc_TypeError, (char *)"__dict__ must be a dictionary object" );
        return false;
    }
    else
    {
        set_slot( &klass->cl_dict, value );
        set_attr_slots( klass );

        return true;
    }
}

static bool set_bases( PyClassObject *klass, PyObject *value )
{
    if ( value == NULL || !PyTuple_Check( value ) )
    {
        PyErr_SetString( PyExc_TypeError, (char *)"__bases__ must be a tuple object" );
        return false;
    }
    else
    {
        Py_ssize_t n = PyTuple_Size( value );

        for ( Py_ssize_t i = 0; i < n; i++ )
        {
            PyObject *base = PyTuple_GET_ITEM( value, i );

            if (unlikely( !PyClass_Check( base ) ))
            {
                PyErr_SetString( PyExc_TypeError, (char *)"__bases__ items must be classes" );
                return false;
            }

            if (unlikely( PyClass_IsSubclass( base, (PyObject *)klass) ))
            {
                PyErr_SetString( PyExc_TypeError, (char *)"a __bases__ item causes an inheritance cycle" );
                return false;
            }
        }

        set_slot( &klass->cl_bases, value );
        set_attr_slots( klass );

        return true;
    }
}

static bool set_name( PyClassObject *klass, PyObject *value )
{
    if ( value == NULL || !PyDict_Check( value ) )
    {
        PyErr_SetString( PyExc_TypeError, (char *)"__name__ must be a string object" );
        return false;
    }

    if ( strlen( PyString_AS_STRING( value )) != (size_t)PyString_GET_SIZE( value ) )
    {
        PyErr_SetString( PyExc_TypeError, (char *)"__name__ must not contain null bytes" );
        return false;
    }

    set_slot( &klass->cl_name, value );
    return true;
}

static int nuitka_class_setattr( PyClassObject *klass, PyObject *attr_name, PyObject *value )
{
    char *sattr_name = PyString_AsString( attr_name );

    if ( sattr_name[0] == '_' && sattr_name[1] == '_' )
    {
        Py_ssize_t n = PyString_Size( attr_name );

        if ( sattr_name[ n-2 ] == '_' && sattr_name[ n-1 ] == '_' )
        {
            if ( strcmp( sattr_name, "__dict__" ) == 0 )
            {
                if ( set_dict( klass, value ) == false )
                {
                    return -1;
                }
                else
                {
                    return 0;
                }
            }
            else if ( strcmp( sattr_name, "__bases__" ) == 0 )
            {
                if ( set_bases( klass, value ) == false )
                {
                    return -1;
                }
                else
                {
                    return 0;
                }
            }
            else if ( strcmp( sattr_name, "__name__" ) == 0 )
            {
                if ( set_name( klass, value ) == false )
                {
                    return -1;
                }
                else
                {
                    return 0;
                }
            }
            else if ( strcmp( sattr_name, "__getattr__" ) == 0 )
            {
                set_slot( &klass->cl_getattr, value );
            }
            else if ( strcmp(sattr_name, "__setattr__" ) == 0 )
            {
                set_slot( &klass->cl_setattr, value );
            }
            else if ( strcmp(sattr_name, "__delattr__" ) == 0 )
            {
                set_slot( &klass->cl_delattr, value );
            }
        }
    }

    if ( value == NULL )
    {
        int status = PyDict_DelItem( klass->cl_dict, attr_name );

        if ( status < 0 )
        {
            PyErr_Format( PyExc_AttributeError, "class %s has no attribute '%s'", PyString_AS_STRING( klass->cl_name ), sattr_name );
        }

        return status;
    }
    else
    {
        return PyDict_SetItem( klass->cl_dict, attr_name, value );
    }
}

static PyObject *nuitka_class_getattr( PyClassObject *klass, PyObject *attr_name )
{
    char *sattr_name = PyString_AsString( attr_name );

    if ( sattr_name[0] == '_' && sattr_name[1] == '_' )
    {
        if ( strcmp( sattr_name, "__dict__" ) == 0 )
        {
            return INCREASE_REFCOUNT( klass->cl_dict );
        }
        else if ( strcmp(sattr_name, "__bases__" ) == 0 )
        {
            return INCREASE_REFCOUNT( klass->cl_bases );
        }
        else if ( strcmp(sattr_name, "__name__" ) == 0 )
        {
            return klass->cl_name ? INCREASE_REFCOUNT( klass->cl_name ) : INCREASE_REFCOUNT( Py_None );
        }
    }

    PyObject *value = FIND_ATTRIBUTE_IN_CLASS( klass, attr_name );

    if (unlikely( value == NULL ))
    {
        PyErr_Format( PyExc_AttributeError, "class %s has no attribute '%s'", PyString_AS_STRING( klass->cl_name ), sattr_name );
        return NULL;
    }

    PyTypeObject *type = Py_TYPE( value );

    descrgetfunc tp_descr_get = PyType_HasFeature( type, Py_TPFLAGS_HAVE_CLASS ) ? type->tp_descr_get : NULL;

    if ( tp_descr_get == NULL )
    {
        return INCREASE_REFCOUNT( value );
    }
    else
    {
        return tp_descr_get( value, (PyObject *)NULL, (PyObject *)klass );
    }
}

#endif

void enhancePythonTypes( void )
{
#if PYTHON_VERSION < 300
    // Our own variant won't call PyEval_GetRestricted, saving quite some cycles
    // not doing that.
    PyClass_Type.tp_setattro = (setattrofunc)nuitka_class_setattr;
    PyClass_Type.tp_getattro = (getattrofunc)nuitka_class_getattr;
#endif
}

#ifdef __APPLE__
extern "C" wchar_t* _Py_DecodeUTF8_surrogateescape(const char *s, Py_ssize_t size);
#endif

#ifdef __FreeBSD__
#include <floatingpoint.h>
#endif

#include <locale.h>

#if PYTHON_VERSION >= 300
argv_type_t convertCommandLineParameters( int argc, char **argv )
{
#if _WIN32
    int new_argc;

    argv_type_t result = CommandLineToArgvW( GetCommandLineW(), &new_argc );
    assert( new_argc == argc );
    return result;
#else
    // Originally taken from CPython3: There seems to be no sane way to use
    static wchar_t **argv_copy;
    argv_copy = (wchar_t **)PyMem_Malloc(sizeof(wchar_t*) * argc);

    // Temporarily disable locale for conversions to not use it.
    char *oldloc = strdup( setlocale( LC_ALL, NULL ) );
    setlocale( LC_ALL, "" );

    for ( int i = 0; i < argc; i++ )
    {
#ifdef __APPLE__
        argv_copy[i] = _Py_DecodeUTF8_surrogateescape( argv[ i ], strlen( argv[ i ] ) );
#elif PYTHON_VERSION < 350
        argv_copy[i] = _Py_char2wchar( argv[ i ], NULL );
#else
        argv_copy[i] = Py_DecodeLocale( argv[ i ], NULL );
#endif

        assert ( argv_copy[ i ] );
    }

    setlocale( LC_ALL, oldloc );
    free( oldloc );

    return argv_copy;
#endif
}
#endif

bool setCommandLineParameters( int argc, argv_type_t argv, bool initial )
{
    bool is_multiprocessing_fork = false;

    if ( initial )
    {
        /* We might need to skip what multiprocessing has told us. */
        for ( int i = 1; i < argc; i++ )
        {
#if PYTHON_VERSION < 300
            if ((strcmp(argv[i], "--multiprocessing-fork")) == 0 && (i+1 < argc))
#else
            wchar_t constant_buffer[100];
            mbstowcs( constant_buffer, "--multiprocessing-fork", 100 );
            if ((wcscmp(argv[i], constant_buffer )) == 0 && (i+1 < argc))
#endif
            {
                is_multiprocessing_fork = true;
                break;
            }
        }
    }

    if ( initial )
    {
        Py_SetProgramName( argv[0] );
    }
    else
    {
        PySys_SetArgv( argc, argv );
    }

    return is_multiprocessing_fork;
}

typedef struct {
    PyObject_HEAD
    PyTypeObject *type;
    PyObject *obj;
    PyTypeObject *obj_type;
} superobject;

extern PyObject *const_str_plain___class__;

PyObject *BUILTIN_SUPER( PyObject *type, PyObject *object )
{
    CHECK_OBJECT( type );

    superobject *result = PyObject_GC_New( superobject, &PySuper_Type );
    assert( result );

    if ( object == Py_None )
    {
        object = NULL;
    }

    if (unlikely( PyType_Check( type ) == false ))
    {
        PyErr_Format( PyExc_RuntimeError, "super(): __class__ is not a type (%s)", Py_TYPE( type )->tp_name );

        return NULL;
    }

    result->type = (PyTypeObject *)INCREASE_REFCOUNT( type );
    if ( object )
    {
        result->obj = INCREASE_REFCOUNT( object );

        if ( PyType_Check( object ) && PyType_IsSubtype( (PyTypeObject *)object, (PyTypeObject *)type ))
        {
            result->obj_type = (PyTypeObject *)INCREASE_REFCOUNT( object );
        }
        else if ( PyType_IsSubtype( Py_TYPE(object ), (PyTypeObject *)type) )
        {
            result->obj_type = (PyTypeObject *)INCREASE_REFCOUNT( (PyObject *)Py_TYPE( object ) );
        }
        else
        {
            PyObject *class_attr = PyObject_GetAttr( object, const_str_plain___class__);

            if (likely( class_attr != NULL && PyType_Check( class_attr ) && (PyTypeObject *)class_attr != Py_TYPE( object ) ))
            {
                result->obj_type = (PyTypeObject *)class_attr;
            }
            else
            {
                if ( class_attr == NULL )
                {
                    CLEAR_ERROR_OCCURRED();
                }
                else
                {
                    Py_DECREF( class_attr );
                }

                PyErr_Format(
                    PyExc_TypeError,
                    "super(type, obj): obj must be an instance or subtype of type"
                );

                return NULL;
            }
        }
    }
    else
    {
        result->obj = NULL;
        result->obj_type = NULL;
    }

    Nuitka_GC_Track( result );

    CHECK_OBJECT( (PyObject *)result );
    assert( Py_TYPE( result ) == &PySuper_Type );

    return (PyObject *)result;
}

PyObject *BUILTIN_CALLABLE( PyObject *value )
{
    return PyBool_FromLong( (long)PyCallable_Check( value ) );
}

PyObject *original_isinstance = NULL;

// Note: Installed and used by "InspectPatcher".
int Nuitka_IsInstance( PyObject *inst, PyObject *cls )
{
    CHECK_OBJECT( original_isinstance );
    CHECK_OBJECT( inst );
    CHECK_OBJECT( cls );

    // Quick path.
    if ( Py_TYPE( inst ) == (PyTypeObject *)cls )
    {
        return true;
    }

    // Our paths for the types we need to hook.
    if ( cls == (PyObject *)&PyFunction_Type && Nuitka_Function_Check( inst ) )
    {
        return true;
    }

    if ( cls == (PyObject *)&PyGen_Type && Nuitka_Generator_Check( inst ) )
    {
        return true;
    }

    if ( cls == (PyObject *)&PyMethod_Type && Nuitka_Method_Check( inst ) )
    {
        return true;
    }

    if ( cls == (PyObject *)&PyFrame_Type && Nuitka_Frame_Check( inst ) )
    {
        return true;
    }

#if PYTHON_VERSION >= 350
    if ( cls == (PyObject *)&PyCoro_Type && Nuitka_Coroutine_Check( inst ) )
    {
        return true;
    }
#endif

    // May need to be recursive for tuple arguments.
    if ( PyTuple_Check( cls ) )
    {
        for ( Py_ssize_t i = 0, size = PyTuple_GET_SIZE( cls ); i < size; i++ )
        {
            PyObject *element = PyTuple_GET_ITEM( cls, i );

            if (unlikely( Py_EnterRecursiveCall( (char *)" in __instancecheck__" ) ))
            {
                return -1;
            }

            int res = Nuitka_IsInstance( inst, element );

            Py_LeaveRecursiveCall();

            if ( res != 0 )
            {
                return res;
            }
        }

        return 0;
    }
    else
    {
        PyObject *args[] = { inst, cls };
        PyObject *result = CALL_FUNCTION_WITH_ARGS2(
            original_isinstance,
            args
        );

        if ( result == NULL )
        {
            return -1;
        }

        int res = PyObject_IsTrue( result );
        Py_DECREF( result );

        return res;
    }
}

PyObject *BUILTIN_ISINSTANCE( PyObject *inst, PyObject *cls )
{
    int res = Nuitka_IsInstance( inst, cls );

    if (unlikely( res < 0 ))
    {
        return NULL;
    }

    return BOOL_FROM( res != 0 );
}

PyObject *BUILTIN_GETATTR( PyObject *object, PyObject *attribute, PyObject *default_value )
{
#if PYTHON_VERSION < 300
    if ( PyUnicode_Check( attribute ) )
    {
        attribute = _PyUnicode_AsDefaultEncodedString( attribute, NULL );

        if (unlikely( attribute == NULL ))
        {
            return NULL;
        }
    }

    if (unlikely( !PyString_Check( attribute ) ))
    {
        PyErr_Format( PyExc_TypeError, "getattr(): attribute name must be string" );
        return NULL;
    }
#else
    if (!PyUnicode_Check( attribute ))
    {
        PyErr_Format( PyExc_TypeError, "getattr(): attribute name must be string" );
        return NULL;
    }
#endif

    PyObject *result = PyObject_GetAttr( object, attribute );

    if ( result == NULL )
    {
        if ( default_value != NULL && EXCEPTION_MATCH_BOOL_SINGLE( GET_ERROR_OCCURRED(), PyExc_AttributeError ) )
        {
            CLEAR_ERROR_OCCURRED();

            return INCREASE_REFCOUNT( default_value );
        }
        else
        {
            return NULL;
        }
    }
    else
    {
        return result;
    }
}

PyObject *BUILTIN_SETATTR( PyObject *object, PyObject *attribute, PyObject *value )
{
    int res = PyObject_SetAttr( object, attribute, value );

    if ( res < 0 )
    {
        return NULL;
    }

    return BOOL_FROM( res == 0 );
}

PyDictObject *dict_builtin = NULL;
PyModuleObject *builtin_module = NULL;

static PyTypeObject Nuitka_BuiltinModule_Type =
{
    PyVarObject_HEAD_INIT(NULL, 0)
    "compiled_module",                           // tp_name
    sizeof(PyModuleObject),                      // tp_size
};

extern PyObject *const_str_plain_open;

int Nuitka_BuiltinModule_SetAttr( PyModuleObject *module, PyObject *name, PyObject *value )
{
    CHECK_OBJECT( (PyObject *)module );
    CHECK_OBJECT( name );

    // This is used for "del" as well.
    assert( value == NULL || Py_REFCNT( value ) > 0 );

    // only checks the builtins that we can refresh at this time, if we have
    // many value to check maybe need create a dict first.
    bool found = false;

    int res = PyObject_RichCompareBool( name, const_str_plain_open, Py_EQ );

    if (unlikely( res == -1 ))
    {
        return -1;
    }
    if ( res == 1 )
    {
        NUITKA_UPDATE_BUILTIN( open, value );
        found = true;
    }

    if ( found == false )
    {
        res = PyObject_RichCompareBool( name, const_str_plain___import__, Py_EQ );

        if (unlikely( res == -1 ))
        {
            return -1;
        }

        if ( res == 1 )
        {
            NUITKA_UPDATE_BUILTIN( __import__, value );
            found = true;
        }
    }

#if PYTHON_VERSION >= 300
    if ( found == false )
    {
        res = PyObject_RichCompareBool( name, const_str_plain_print, Py_EQ );

        if (unlikely( res == -1 ))
        {
            return -1;
        }

        if ( res == 1 )
        {
            NUITKA_UPDATE_BUILTIN( print, value );
            found = true;
        }
    }
#endif

    return PyObject_GenericSetAttr( (PyObject *)module, name, value );
}


#include <osdefs.h>

#if defined(_WIN32)
#include <Shlwapi.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <dlfcn.h>
#include <libgen.h>
#else
#include <dlfcn.h>
#include <libgen.h>
#endif

#if defined( __FreeBSD__ )
#include <sys/sysctl.h>
#endif

#if defined(_NUITKA_EXE)

char *getBinaryDirectoryUTF8Encoded()
{
    static char binary_directory[ MAXPATHLEN + 1 ];
    static bool init_done = false;

    if ( init_done )
    {
        return binary_directory;
    }

#if defined(_WIN32)

#if PYTHON_VERSION >= 300
    WCHAR binary_directory2[ MAXPATHLEN + 1 ];
    binary_directory2[0] = 0;

    DWORD res = GetModuleFileNameW( NULL, binary_directory2, MAXPATHLEN + 1 );
    assert( res != 0 );

    int res2 = WideCharToMultiByte( CP_UTF8, 0, binary_directory2, -1, binary_directory, MAXPATHLEN + 1, NULL, NULL );
    assert( res2 != 0 );
#else
    DWORD res = GetModuleFileName( NULL, binary_directory, MAXPATHLEN + 1 );
    assert( res != 0 );
#endif
    PathRemoveFileSpec( binary_directory );
#elif defined(__APPLE__)
    uint32_t bufsize = MAXPATHLEN + 1;
    int res =_NSGetExecutablePath( binary_directory, &bufsize );

    if (unlikely( res != 0 ))
    {
        abort();
    }

    // On MacOS, the "dirname" call creates a separate internal string, we can
    // safely copy back.
    strncpy(binary_directory, dirname(binary_directory), MAXPATHLEN + 1);

#elif defined( __FreeBSD__ )
    /* Not all of FreeBSD has /proc file system, so use the appropriate
     * "sysctl" instead.
     */
    int mib[4];
    mib[0] = CTL_KERN;
    mib[1] = KERN_PROC;
    mib[2] = KERN_PROC_PATHNAME;
    mib[3] = -1;
    size_t cb = sizeof( binary_directory );
    sysctl( mib, 4, binary_directory, &cb, NULL, 0 );

    /* We want the directory name, the above gives the full executable name. */
    strcpy( binary_directory, dirname( binary_directory ) );
#else
    /* The remaining platforms, mostly Linux or compatible. */

    /* The "readlink" call does not terminate result, so fill zeros there, then
     * it is a proper C string right away. */
    memset( binary_directory, 0, MAXPATHLEN + 1 );
    ssize_t res = readlink( "/proc/self/exe", binary_directory, MAXPATHLEN );

    if (unlikely( res == -1 ))
    {
        abort();
    }

    strcpy( binary_directory, dirname( binary_directory ) );
#endif
    init_done = true;
    return binary_directory;
}

char *getBinaryDirectoryHostEncoded()
{
#if defined(_WIN32)
    static char binary_directory[ MAXPATHLEN + 1 ];
    static bool init_done = false;

    if ( init_done )
    {
        return binary_directory;
    }

#if PYTHON_VERSION >= 300
    WCHAR binary_directory2[ MAXPATHLEN + 1 ];
    binary_directory2[0] = 0;

    DWORD res = GetModuleFileNameW( NULL, binary_directory2, MAXPATHLEN + 1 );
    assert( res != 0 );

    int res2 = WideCharToMultiByte( CP_ACP, 0, binary_directory2, -1, binary_directory, MAXPATHLEN + 1, NULL, NULL );
    assert( res2 != 0 );
#else
    DWORD res = GetModuleFileName( NULL, binary_directory, MAXPATHLEN + 1 );
    assert( res != 0 );
#endif
    PathRemoveFileSpec( binary_directory );

    init_done = true;
    return binary_directory;
#else
    return getBinaryDirectoryUTF8Encoded();
#endif
}

static PyObject *getBinaryDirectoryObject()
{
    static PyObject *binary_directory = NULL;

    if ( binary_directory != NULL )
    {
        return binary_directory;
    }

#if PYTHON_VERSION >= 300
    binary_directory = PyUnicode_FromString( getBinaryDirectoryUTF8Encoded() );
#else
    binary_directory = PyString_FromString( getBinaryDirectoryUTF8Encoded() );
#endif

    if (unlikely( binary_directory == NULL ))
    {
        PyErr_Print();
        abort();
    }

    return binary_directory;
}

#else
static char *getDllDirectory()
{
#if defined(_WIN32)
    static char path[ MAXPATHLEN + 1 ];
    HMODULE hm = NULL;
    path[0] = '\0';

#if PYTHON_VERSION >= 300
    WCHAR path2[ MAXPATHLEN + 1 ];
    path2[0] = 0;

    int res = GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR) &getDllDirectory,
        &hm
    );
    assert( res != 0 );

    int res2 = WideCharToMultiByte(CP_UTF8, 0, path2, -1, path, MAXPATHLEN + 1, NULL, NULL);
    assert( res2 != 0 );
#else
    int res = GetModuleHandleExA(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR) &getDllDirectory,
        &hm
    );
    assert( res != 0 );
#endif

    PathRemoveFileSpec( path );

    return path;

#else
    Dl_info where;
    int res = dladdr( (void *)getDllDirectory, &where );
    assert( res != 0 );

    return dirname( (char *)where.dli_fname );
#endif
}
#endif

void _initBuiltinModule()
{
#if _NUITKA_MODULE
    if ( builtin_module ) return;
#else
    assert( builtin_module == NULL );
#endif

#if PYTHON_VERSION < 300
    builtin_module = (PyModuleObject *)PyImport_ImportModule( "__builtin__" );
#else
    builtin_module = (PyModuleObject *)PyImport_ImportModule( "builtins" );
#endif
    assert( builtin_module );
    dict_builtin = (PyDictObject *)builtin_module->md_dict;
    assert( PyDict_Check( dict_builtin ) );

#ifdef _NUITKA_STANDALONE
    int res = PyDict_SetItemString(
        (PyObject *)dict_builtin,
        "__nuitka_binary_dir",
        getBinaryDirectoryObject()
    );

    assert( res == 0 );
#endif

    // init Nuitka_BuiltinModule_Type, PyType_Ready wont copy all member from
    // base type, so we need copy all members from PyModule_Type manual for
    // safety.  PyType_Ready will change tp_flags, we need define it again. Set
    // tp_setattro to Nuitka_BuiltinModule_SetAttr and we can detect value
    // change. Set tp_base to PyModule_Type and PyModule_Check will pass.
    Nuitka_BuiltinModule_Type.tp_dealloc = PyModule_Type.tp_dealloc;
    Nuitka_BuiltinModule_Type.tp_repr = PyModule_Type.tp_repr;
    Nuitka_BuiltinModule_Type.tp_setattro = (setattrofunc)Nuitka_BuiltinModule_SetAttr;
    Nuitka_BuiltinModule_Type.tp_getattro = PyModule_Type.tp_getattro;
    Nuitka_BuiltinModule_Type.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_BASETYPE;
    Nuitka_BuiltinModule_Type.tp_doc = PyModule_Type.tp_doc;
    Nuitka_BuiltinModule_Type.tp_traverse = PyModule_Type.tp_traverse;
    Nuitka_BuiltinModule_Type.tp_members = PyModule_Type.tp_members;
    Nuitka_BuiltinModule_Type.tp_base = &PyModule_Type;
    Nuitka_BuiltinModule_Type.tp_dictoffset = PyModule_Type.tp_dictoffset;
    Nuitka_BuiltinModule_Type.tp_init = PyModule_Type.tp_init;
    Nuitka_BuiltinModule_Type.tp_alloc = PyModule_Type.tp_alloc;
    Nuitka_BuiltinModule_Type.tp_new = PyModule_Type.tp_new;
    Nuitka_BuiltinModule_Type.tp_free = PyModule_Type.tp_free;
    int ret = PyType_Ready( &Nuitka_BuiltinModule_Type );
    assert( ret == 0 );

    // Replace type of builtin module to take over.
    ((PyObject *)builtin_module)->ob_type = &Nuitka_BuiltinModule_Type;
    assert( PyModule_Check( builtin_module ) == 1 );
}


PyObject *callPythonFunction( PyObject *func, PyObject **args, int count )
{
    PyCodeObject *co = (PyCodeObject *)PyFunction_GET_CODE( func );
    PyObject *globals = PyFunction_GET_GLOBALS( func );
    PyObject *argdefs = PyFunction_GET_DEFAULTS( func );

#if PYTHON_VERSION >= 300
    PyObject *kwdefs = PyFunction_GET_KW_DEFAULTS( func );

    if ( kwdefs == NULL && argdefs == NULL && co->co_argcount == count &&
        co->co_flags == ( CO_OPTIMIZED | CO_NEWLOCALS | CO_NOFREE ))
#else
    if ( argdefs == NULL && co->co_argcount == count &&
        co->co_flags == ( CO_OPTIMIZED | CO_NEWLOCALS | CO_NOFREE ))
#endif
    {
        PyThreadState *tstate = PyThreadState_GET();
        CHECK_OBJECT( globals );

        PyFrameObject *frame = PyFrame_New( tstate, co, globals, NULL );

        if (unlikely( frame == NULL ))
        {
            return NULL;
        };

        for ( int i = 0; i < count; i++ )
        {
            frame->f_localsplus[i] = args[i];
            Py_INCREF( frame->f_localsplus[i] );
        }

        PyObject *result = PyEval_EvalFrameEx( frame, 0 );

        // Frame release protects against recursion as it may lead to variable
        // destruction.
        ++tstate->recursion_depth;
        Py_DECREF( frame );
        --tstate->recursion_depth;

        return result;
    }

    PyObject **defaults = NULL;
    int nd = 0;

    if ( argdefs != NULL )
    {
        defaults = &PyTuple_GET_ITEM( argdefs, 0 );
        nd = (int)( Py_SIZE( argdefs ) );
    }

    PyObject *result = PyEval_EvalCodeEx(
#if PYTHON_VERSION >= 300
        (PyObject *)co,
#else
        co,        // code object
#endif
        globals,   // globals
        NULL,      // no locals
        args,      // args
        count,     // argcount
        NULL,      // kwds
        0,         // kwcount
        defaults,  // defaults
        nd,        // defcount
#if PYTHON_VERSION >= 300
        kwdefs,
#endif
        PyFunction_GET_CLOSURE( func )
    );

    return result;
}

static PyObject *_fast_function_noargs( PyObject *func )
{
    PyCodeObject *co = (PyCodeObject *)PyFunction_GET_CODE( func );
    PyObject *globals = PyFunction_GET_GLOBALS( func );
    PyObject *argdefs = PyFunction_GET_DEFAULTS( func );

#if PYTHON_VERSION >= 300
    PyObject *kwdefs = PyFunction_GET_KW_DEFAULTS( func );

    if ( kwdefs == NULL && argdefs == NULL && co->co_argcount == 0 &&
        co->co_flags == ( CO_OPTIMIZED | CO_NEWLOCALS | CO_NOFREE ))
#else
    if ( argdefs == NULL && co->co_argcount == 0 &&
        co->co_flags == ( CO_OPTIMIZED | CO_NEWLOCALS | CO_NOFREE ))
#endif
    {
        PyThreadState *tstate = PyThreadState_GET();
        CHECK_OBJECT( globals );

        PyFrameObject *frame = PyFrame_New( tstate, co, globals, NULL );

        if (unlikely( frame == NULL ))
        {
            return NULL;
        };

        PyObject *result = PyEval_EvalFrameEx( frame, 0 );

        // Frame release protects against recursion as it may lead to variable
        // destruction.
        ++tstate->recursion_depth;
        Py_DECREF( frame );
        --tstate->recursion_depth;

        return result;
    }

    PyObject **defaults = NULL;
    int nd = 0;

    if ( argdefs != NULL )
    {
        defaults = &PyTuple_GET_ITEM( argdefs, 0 );
        nd = (int)( Py_SIZE( argdefs ) );
    }

    PyObject *result = PyEval_EvalCodeEx(
#if PYTHON_VERSION >= 300
        (PyObject *)co,
#else
        co,        // code object
#endif
        globals,   // globals
        NULL,      // no locals
        NULL,      // args
        0,         // argcount
        NULL,      // kwds
        0,         // kwcount
        defaults,  // defaults
        nd,        // defcount
#if PYTHON_VERSION >= 300
        kwdefs,
#endif
        PyFunction_GET_CLOSURE( func )
    );

    return result;
}

PyObject *CALL_FUNCTION_NO_ARGS( PyObject *called )
{
    CHECK_OBJECT( called );

    if ( Nuitka_Function_Check( called ) )
    {
        if (unlikely( Py_EnterRecursiveCall( (char *)" while calling a Python object" ) ))
        {
            return NULL;
        }

        struct Nuitka_FunctionObject *function = (struct Nuitka_FunctionObject *)called;

        PyObject *result;

        if ( function->m_args_simple && 0 == function->m_args_positional_count )
        {
            result = function->m_c_code( function, NULL );
        }
        else if ( function->m_args_simple && function->m_defaults_given == function->m_args_positional_count )
        {
            PyObject **python_pars = &PyTuple_GET_ITEM( function->m_defaults, 0 );

            for( Py_ssize_t i = 0; i < function->m_defaults_given; i++ )
            {
                Py_INCREF( python_pars[ i ] );
            }

            result = function->m_c_code( function, python_pars );
        }
        else
        {
#ifdef _MSC_VER
            PyObject **python_pars = (PyObject **)_alloca( sizeof( PyObject * ) * function->m_args_overall_count );
#else
            PyObject *python_pars[ function->m_args_overall_count ];
#endif
            memset( python_pars, 0, function->m_args_overall_count * sizeof(PyObject *) );

            if ( parseArgumentsPos( function, python_pars, NULL, 0 ) )
            {
                result = function->m_c_code( function, python_pars );
            }
            else
            {
                result = NULL;
            }
        }

        Py_LeaveRecursiveCall();

        return result;
    }
    else if ( Nuitka_Method_Check( called ) )
    {
        struct Nuitka_MethodObject *method = (struct Nuitka_MethodObject *)called;

        // Unbound method without arguments, let the error path be slow.
        if ( method->m_object != NULL )
        {
            if (unlikely( Py_EnterRecursiveCall( (char *)" while calling a Python object" ) ))
            {
                return NULL;
            }

            struct Nuitka_FunctionObject *function = method->m_function;
            PyObject *result;

            if ( function->m_args_simple && 1 == function->m_args_positional_count )
            {
                Py_INCREF( method->m_object );

                result = function->m_c_code( function, &method->m_object );
            }
            else if ( function->m_args_simple && function->m_defaults_given == function->m_args_positional_count - 1 )
            {
#ifdef _MSC_VER
                PyObject **python_pars = (PyObject **)_alloca( sizeof( PyObject * ) * function->m_args_overall_count );
#else
                PyObject *python_pars[ function->m_args_overall_count ];
#endif
                python_pars[0] = method->m_object;
                memcpy( python_pars+1, &PyTuple_GET_ITEM( function->m_defaults, 0 ), sizeof(PyObject *) * function->m_defaults_given );

                for( Py_ssize_t i = 0; i < function->m_args_positional_count; i++ )
                {
                    Py_INCREF( python_pars[ i ] );
                }

                result = function->m_c_code( function, python_pars );
            }
            else
            {
                result = Nuitka_CallMethodFunctionNoArgs( function, method->m_object );
            }

            Py_LeaveRecursiveCall();

            return result;
        }
    }
    else if ( PyFunction_Check( called ) )
    {
        return _fast_function_noargs( called );
    }

    return CALL_FUNCTION(
        called,
        const_tuple_empty,
        NULL
    );
}

#if defined(_NUITKA_STANDALONE) || _NUITKA_FROZEN > 0

#ifdef _NUITKA_STANDALONE
extern PyObject *const_str_plain___file__;

void setEarlyFrozenModulesFileAttribute( void )
{
    Py_ssize_t ppos = 0;
    PyObject *key, *value;

#if PYTHON_VERSION < 300
    PyObject *file_value = MAKE_RELATIVE_PATH( PyString_FromString( "not_present.py" ) );
#else
    PyObject *file_value = MAKE_RELATIVE_PATH( PyUnicode_FromString( "not_present.py" ) );
#endif

    assert( file_value );

    while( PyDict_Next( PyImport_GetModuleDict(), &ppos, &key, &value ) )
    {
        if ( key != NULL && value != NULL && PyModule_Check( value ) )
        {
            if ( !PyObject_HasAttr( value, const_str_plain___file__ ) )
            {
                PyObject_SetAttr( value, const_str_plain___file__, file_value );
            }
        }
    }

    Py_DECREF( file_value );

    assert( !ERROR_OCCURRED() );
}
#endif


#endif


PyObject *MAKE_RELATIVE_PATH( PyObject *relative )
{
    static PyObject *our_path_object = NULL;

    if ( our_path_object == NULL )
    {
#if defined( _NUITKA_EXE )
        our_path_object = getBinaryDirectoryObject();
#else
#if PYTHON_VERSION >= 300
        our_path_object = PyUnicode_FromString( getDllDirectory() );
#else
        our_path_object = PyString_FromString( getDllDirectory() );
#endif

#endif
    }

    char sep[2] = { SEP, 0 };

#if PYTHON_VERSION < 300
    PyObject *result = PyNumber_Add( our_path_object, PyString_FromString( sep ) );
#else
    PyObject *result = PyNumber_Add( our_path_object, PyUnicode_FromString( sep ) );
#endif

    assert( result );

#if PYTHON_VERSION < 300
    result = PyNumber_InPlaceAdd( result, relative );
#else
    result = PyNumber_InPlaceAdd( result, relative );
#endif

    assert( result );

    return result;
}


#ifdef _NUITKA_EXE

NUITKA_DEFINE_BUILTIN( type )
NUITKA_DEFINE_BUILTIN( len )
NUITKA_DEFINE_BUILTIN( repr )
NUITKA_DEFINE_BUILTIN( int )
NUITKA_DEFINE_BUILTIN( iter )
#if PYTHON_VERSION < 300
NUITKA_DEFINE_BUILTIN( long )
#endif

void _initBuiltinOriginalValues()
{
    NUITKA_ASSIGN_BUILTIN( type );
    NUITKA_ASSIGN_BUILTIN( len );
    NUITKA_ASSIGN_BUILTIN( range );
    NUITKA_ASSIGN_BUILTIN( repr );
    NUITKA_ASSIGN_BUILTIN( int );
    NUITKA_ASSIGN_BUILTIN( iter );
#if PYTHON_VERSION < 300
    NUITKA_ASSIGN_BUILTIN( long );
#endif

    CHECK_OBJECT( _python_original_builtin_value_range );
}

#endif

// Used for threading.
#if PYTHON_VERSION >= 300
volatile int _Py_Ticker = _Py_CheckInterval;
#endif

// Reverse operation mapping.
static int const swapped_op[] =
{
    Py_GT, Py_GE, Py_EQ, Py_NE, Py_LT, Py_LE
};

#if PYTHON_VERSION >= 270
iternextfunc default_iternext;

extern PyObject *const_str_plain___iter__;

void _initSlotIternext()
{
    PyObject *pos_args = PyTuple_New(1);
    PyTuple_SET_ITEM(
        pos_args,
        0,
        INCREASE_REFCOUNT( (PyObject *)&PyBaseObject_Type )
    );

    PyObject *kw_args = PyDict_New();
    PyDict_SetItem( kw_args, const_str_plain___iter__, Py_True );

    PyObject *c = PyObject_CallFunctionObjArgs(
        (PyObject *)&PyType_Type,
        const_str_plain___iter__,
        pos_args,
        kw_args,
        NULL
    );
    Py_DECREF( pos_args );
    Py_DECREF( kw_args );

    PyObject *r = PyObject_CallFunctionObjArgs(
        c,
        NULL
    );
    Py_DECREF( c );

    CHECK_OBJECT( r );
    assert( Py_TYPE( r )->tp_iternext );

    default_iternext = Py_TYPE(r)->tp_iternext;

    Py_DECREF( r );
}
#endif

#if PYTHON_VERSION < 300

extern PyObject *const_str_plain___cmp__;
static cmpfunc default_tp_compare;

void _initSlotCompare()
{
    // Create a class with "__cmp__" attribute, to get a hand at the default
    // implementation of tp_compare. It's not part of the API and with shared
    // libraries it's not accessible. The name does not matter, nor does the
    // actual value used for "__cmp__".

    PyObject *pos_args = PyTuple_New(1);
    PyTuple_SET_ITEM(
        pos_args,
        0,
        INCREASE_REFCOUNT( (PyObject *)&PyInt_Type )
    );

    PyObject *kw_args = PyDict_New();
    PyDict_SetItem( kw_args, const_str_plain___cmp__, Py_True );

    PyObject *c = PyObject_CallFunctionObjArgs(
        (PyObject *)&PyType_Type,
        const_str_plain___cmp__,
        pos_args,
        kw_args,
        NULL
    );
    Py_DECREF( pos_args );
    Py_DECREF( kw_args );

    PyObject *r = PyObject_CallFunctionObjArgs(
        c,
        NULL
    );
    Py_DECREF( c );

    CHECK_OBJECT( r );
    assert( Py_TYPE( r )->tp_compare );

    default_tp_compare = Py_TYPE( r )->tp_compare;

    Py_DECREF( r );
}

#define RICHCOMPARE( t ) ( PyType_HasFeature((t), Py_TPFLAGS_HAVE_RICHCOMPARE) ? (t)->tp_richcompare : NULL )

static inline int adjust_tp_compare( int c )
{
    if ( PyErr_Occurred() )
    {
        return -2;
    }
    else if (c < -1 || c > 1)
    {
        return c < -1 ? -1 : 1;
    }
    else
    {
        return c;
    }
}

static inline int coerce_objects( PyObject **pa, PyObject **pb )
{
    PyObject *a = *pa;
    PyObject *b = *pb;

    // Shortcut only for old-style types
    if ( a->ob_type == b->ob_type && !PyType_HasFeature( a->ob_type, Py_TPFLAGS_CHECKTYPES ))
    {
        Py_INCREF( a );
        Py_INCREF( b );

        return 0;
    }
    if ( a->ob_type->tp_as_number && a->ob_type->tp_as_number->nb_coerce )
    {
        int res = (*a->ob_type->tp_as_number->nb_coerce)( pa, pb );

        if ( res <= 0 )
        {
            return res;
        }
    }
    if ( b->ob_type->tp_as_number && b->ob_type->tp_as_number->nb_coerce )
    {
        int res = (*b->ob_type->tp_as_number->nb_coerce)( pb, pa );

        if ( res <= 0 )
        {
            return res;
        }
    }

    return 1;
}

static int try_3way_compare( PyObject *a, PyObject *b )
{
    cmpfunc f1 = a->ob_type->tp_compare;
    cmpfunc f2 = b->ob_type->tp_compare;
    int c;

    // Same compares, just use it.
    if ( f1 != NULL && f1 == f2 )
    {
        c = (*f1)( a, b );
        return adjust_tp_compare( c );
    }

    // If one slot is _PyObject_SlotCompare (which we got our hands on under a
    // different name in case it's a shared library), prefer it.
    if ( f1 == default_tp_compare || f2 == default_tp_compare )
    {
        return default_tp_compare( a, b );
    }

    // Try coercion.
    c = coerce_objects( &a, &b );

    if (c < 0)
    {
        return -2;
    }
    if (c > 0)
    {
        return 2;
    }

    f1 = a->ob_type->tp_compare;
    if ( f1 != NULL && f1 == b->ob_type->tp_compare )
    {
        c = (*f1)( a, b );
        Py_DECREF( a );
        Py_DECREF( b );

        return adjust_tp_compare(c);
    }

    // No comparison defined.
    Py_DECREF( a );
    Py_DECREF( b );
    return 2;
}

PyObject *MY_RICHCOMPARE( PyObject *a, PyObject *b, int op )
{
    CHECK_OBJECT( a );
    CHECK_OBJECT( b );

    // TODO: Type a-ware rich comparison would be really nice, but this is what
    // CPython does, and should be even in "richcomparisons.h" as the first
    // thing, so it's even cheaper.
    if ( PyInt_CheckExact( a ) && PyInt_CheckExact( b ))
    {
        long aa, bb;
#ifdef __NUITKA_NO_ASSERT__
        bool res;
#else
        bool res = false;
#endif

        aa = PyInt_AS_LONG( a );
        bb = PyInt_AS_LONG( b );

        switch( op )
        {
            case Py_LT: res = aa <  bb; break;
            case Py_LE: res = aa <= bb; break;
            case Py_EQ: res = aa == bb; break;
            case Py_NE: res = aa != bb; break;
            case Py_GT: res = aa >  bb; break;
            case Py_GE: res = aa >= bb; break;
            default: assert( false );
        }
        return INCREASE_REFCOUNT( BOOL_FROM( res ) );
    }

    // TODO: Get hint from recursion control if that's needed.
    if (unlikely( Py_EnterRecursiveCall((char *)" in cmp") ))
    {
        return NULL;
    }

    PyObject *result;

    // If the types are equal, we may get away immediately.
    if ( a->ob_type == b->ob_type && !PyInstance_Check( a ) )
    {
        richcmpfunc frich = RICHCOMPARE( a->ob_type );

        if ( frich != NULL )
        {
            result = (*frich)( a, b, op );

            if (result != Py_NotImplemented)
            {
                Py_LeaveRecursiveCall();
                return result;
            }

            Py_DECREF( result );
        }

        // No rich comparison, but maybe compare works.
        cmpfunc fcmp = a->ob_type->tp_compare;
        if ( fcmp != NULL )
        {
            int c = (*fcmp)( a, b );
            c = adjust_tp_compare( c );

            Py_LeaveRecursiveCall();

            if ( c == -2 )
            {
                return NULL;
            }

            switch( op )
            {
                case Py_LT: c = c <  0; break;
                case Py_LE: c = c <= 0; break;
                case Py_EQ: c = c == 0; break;
                case Py_NE: c = c != 0; break;
                case Py_GT: c = c >  0; break;
                case Py_GE: c = c >= 0; break;
            }

            return INCREASE_REFCOUNT( BOOL_FROM( c != 0 ) );
        }
    }

    // Fast path was not successful or not taken
    richcmpfunc f;

    if ( a->ob_type != b->ob_type && PyType_IsSubtype( b->ob_type, a->ob_type ) )
    {
        f = RICHCOMPARE( b->ob_type );

        if ( f != NULL )
        {
            result = (*f)( b, a, swapped_op[ op ] );

            if ( result != Py_NotImplemented )
            {
                Py_LeaveRecursiveCall();
                return result;
            }

           Py_DECREF( result );
        }
    }

    f = RICHCOMPARE( a->ob_type );
    if ( f != NULL )
    {
        result = (*f)( a, b, op );

        if ( result != Py_NotImplemented )
        {
            Py_LeaveRecursiveCall();
            return result;
        }

        Py_DECREF( result );
    }

    f = RICHCOMPARE( b->ob_type );
    if ( f != NULL )
    {
        result = (*f)( b, a, swapped_op[ op ] );

        if ( result != Py_NotImplemented )
        {
            Py_LeaveRecursiveCall();
            return result;
        }

        Py_DECREF( result );
    }

    int c;

    if ( PyInstance_Check( a ) )
    {
        c = (*a->ob_type->tp_compare)( a, b );
    }
    else if ( PyInstance_Check( b ) )
    {
        c = (*b->ob_type->tp_compare)( a, b );
    }
    else
    {
        c = try_3way_compare( a, b );
    }

    if ( c >= 2 )
    {
        if ( a->ob_type == b->ob_type )
        {
            Py_uintptr_t aa = (Py_uintptr_t)a;
            Py_uintptr_t bb = (Py_uintptr_t)b;

            c = ( aa < bb ) ? -1 : ( aa > bb ) ? 1 : 0;
        }
        else if ( a == Py_None )
        {
            // None is smaller than everything else
            c = -1;
        }
        else if ( b == Py_None )
        {
            // None is smaller than everything else
            c = 1;
        }
        else if ( PyNumber_Check( a ) )
        {
            // different type: compare type names but numbers are smaller than
            // others.
            if ( PyNumber_Check( b ) )
            {
                // Both numbers, need to make a decision based on types.
                Py_uintptr_t aa = (Py_uintptr_t)Py_TYPE( a );
                Py_uintptr_t bb = (Py_uintptr_t)Py_TYPE( b );

                c = ( aa < bb ) ? -1 : ( aa > bb ) ? 1 : 0;
            }
            else
            {
                c = -1;
            }
        }
        else if ( PyNumber_Check( b ) )
        {
            c = 1;
        }
        else
        {
            int s = strcmp( a->ob_type->tp_name, b->ob_type->tp_name );

            if ( s < 0 )
            {
                c = -1;
            }
            else if ( s > 0 )
            {
                c = 1;
            }
            else
            {
                // Same type name need to make a decision based on types.
                Py_uintptr_t aa = (Py_uintptr_t)Py_TYPE( a );
                Py_uintptr_t bb = (Py_uintptr_t)Py_TYPE( b );

                c = ( aa < bb ) ? -1 : ( aa > bb ) ? 1 : 0;
            }
        }
    }

    Py_LeaveRecursiveCall();

    if (unlikely( c <= -2 ))
    {
        return NULL;
    }

    switch( op )
    {
        case Py_LT: c = c <  0; break;
        case Py_LE: c = c <= 0; break;
        case Py_EQ: c = c == 0; break;
        case Py_NE: c = c != 0; break;
        case Py_GT: c = c >  0; break;
        case Py_GE: c = c >= 0; break;
    }

    return INCREASE_REFCOUNT( BOOL_FROM( c != 0 ) );
}

PyObject *MY_RICHCOMPARE_NORECURSE( PyObject *a, PyObject *b, int op )
{
    CHECK_OBJECT( a );
    CHECK_OBJECT( b );

    // TODO: Type a-ware rich comparison would be really nice, but this is what
    // CPython does, and should be even in "richcomparisons.h" as the first
    // thing, so it's even cheaper.
    if ( PyInt_CheckExact( a ) && PyInt_CheckExact( b ))
    {
        long aa, bb;
#ifdef __NUITKA_NO_ASSERT__
        bool res;
#else
        bool res = false;
#endif

        aa = PyInt_AS_LONG( a );
        bb = PyInt_AS_LONG( b );

        switch( op )
        {
            case Py_LT: res = aa <  bb; break;
            case Py_LE: res = aa <= bb; break;
            case Py_EQ: res = aa == bb; break;
            case Py_NE: res = aa != bb; break;
            case Py_GT: res = aa >  bb; break;
            case Py_GE: res = aa >= bb; break;
            default: assert( false );
        }
        return INCREASE_REFCOUNT( BOOL_FROM( res ) );
    }

    PyObject *result;

    // If the types are equal, we may get away immediately.
    if ( a->ob_type == b->ob_type && !PyInstance_Check( a ) )
    {
        richcmpfunc frich = RICHCOMPARE( a->ob_type );

        if ( frich != NULL )
        {
            result = (*frich)( a, b, op );

            if (result != Py_NotImplemented)
            {
                return result;
            }

            Py_DECREF( result );
        }

        // No rich comparison, but maybe compare works.
        cmpfunc fcmp = a->ob_type->tp_compare;
        if ( fcmp != NULL )
        {
            int c = (*fcmp)( a, b );
            c = adjust_tp_compare( c );

            if ( c == -2 )
            {
                return NULL;
            }

            switch( op )
            {
                case Py_LT: c = c <  0; break;
                case Py_LE: c = c <= 0; break;
                case Py_EQ: c = c == 0; break;
                case Py_NE: c = c != 0; break;
                case Py_GT: c = c >  0; break;
                case Py_GE: c = c >= 0; break;
            }

            return INCREASE_REFCOUNT( BOOL_FROM( c != 0 ) );
        }
    }

    // Fast path was not successful or not taken
    richcmpfunc f;

    if ( a->ob_type != b->ob_type && PyType_IsSubtype( b->ob_type, a->ob_type ) )
    {
        f = RICHCOMPARE( b->ob_type );

        if ( f != NULL )
        {
            result = (*f)( b, a, swapped_op[ op ] );

            if ( result != Py_NotImplemented )
            {
                return result;
            }

           Py_DECREF( result );
        }
    }

    f = RICHCOMPARE( a->ob_type );
    if ( f != NULL )
    {
        result = (*f)( a, b, op );

        if ( result != Py_NotImplemented )
        {
            return result;
        }

        Py_DECREF( result );
    }

    f = RICHCOMPARE( b->ob_type );
    if ( f != NULL )
    {
        result = (*f)( b, a, swapped_op[ op ] );

        if ( result != Py_NotImplemented )
        {
            return result;
        }

        Py_DECREF( result );
    }

    int c;

    if ( PyInstance_Check( a ) )
    {
        c = (*a->ob_type->tp_compare)( a, b );
    }
    else if ( PyInstance_Check( b ) )
    {
        c = (*b->ob_type->tp_compare)( a, b );
    }
    else
    {
        c = try_3way_compare( a, b );
    }

    if ( c >= 2 )
    {
        if ( a->ob_type == b->ob_type )
        {
            Py_uintptr_t aa = (Py_uintptr_t)a;
            Py_uintptr_t bb = (Py_uintptr_t)b;

            c = ( aa < bb ) ? -1 : ( aa > bb ) ? 1 : 0;
        }
        else if ( a == Py_None )
        {
            // None is smaller than everything else
            c = -1;
        }
        else if ( b == Py_None )
        {
            // None is smaller than everything else
            c = 1;
        }
        else if ( PyNumber_Check( a ) )
        {
            // different type: compare type names but numbers are smaller than
            // others.
            if ( PyNumber_Check( b ) )
            {
                // Both numbers, need to make a decision based on types.
                Py_uintptr_t aa = (Py_uintptr_t)Py_TYPE( a );
                Py_uintptr_t bb = (Py_uintptr_t)Py_TYPE( b );

                c = ( aa < bb ) ? -1 : ( aa > bb ) ? 1 : 0;
            }
            else
            {
                c = -1;
            }
        }
        else if ( PyNumber_Check( b ) )
        {
            c = 1;
        }
        else
        {
            int s = strcmp( a->ob_type->tp_name, b->ob_type->tp_name );

            if ( s < 0 )
            {
                c = -1;
            }
            else if ( s > 0 )
            {
                c = 1;
            }
            else
            {
                // Same type name need to make a decision based on types.
                Py_uintptr_t aa = (Py_uintptr_t)Py_TYPE( a );
                Py_uintptr_t bb = (Py_uintptr_t)Py_TYPE( b );

                c = ( aa < bb ) ? -1 : ( aa > bb ) ? 1 : 0;
            }
        }
    }

    if (unlikely( c <= -2 ))
    {
        return NULL;
    }

    switch( op )
    {
        case Py_LT: c = c <  0; break;
        case Py_LE: c = c <= 0; break;
        case Py_EQ: c = c == 0; break;
        case Py_NE: c = c != 0; break;
        case Py_GT: c = c >  0; break;
        case Py_GE: c = c >= 0; break;
    }

    return INCREASE_REFCOUNT( BOOL_FROM( c != 0 ) );
}

#else

// Table for operation names as strings.
static char const *op_strings[] =
{
    "<", "<=", "==", "!=", ">", ">="
};

PyObject *MY_RICHCOMPARE( PyObject *a, PyObject *b, int op )
{
    CHECK_OBJECT( a );
    CHECK_OBJECT( b );

    if (unlikely( Py_EnterRecursiveCall( (char *)" in comparison" ) ))
    {
        return NULL;
    }

    bool checked_reverse_op = false;
    PyObject *result = NULL;
    richcmpfunc f;

    if ( a->ob_type != b->ob_type && PyType_IsSubtype( b->ob_type, a->ob_type ) )
    {
        f = b->ob_type->tp_richcompare;
        if ( f != NULL )
        {
            checked_reverse_op = true;

            result = (*f)( b, a, swapped_op[ op ] );

            if (unlikely( result == NULL ))
            {
                Py_LeaveRecursiveCall();
                return NULL;
            }

            if ( result == Py_NotImplemented )
            {
                Py_DECREF( result );
                result = NULL;
            }

        }
    }

    if ( result == NULL )
    {
        f = a->ob_type->tp_richcompare;

        if ( f != NULL )
        {
            result = (*f)( a, b, op );

            if (unlikely( result == NULL ))
            {
                Py_LeaveRecursiveCall();
                return NULL;
            }

            if ( result == Py_NotImplemented )
            {
                Py_DECREF( result );
                result = NULL;
            }
        }
    }

    if ( result == NULL && checked_reverse_op == false )
    {
        f = b->ob_type->tp_richcompare;

        if ( f != NULL )
        {
            result = (*f)( b, a, swapped_op[ op ] );

            if (unlikely( result == NULL ))
            {
                Py_LeaveRecursiveCall();
                return NULL;
            }

            if ( result == Py_NotImplemented )
            {
                Py_DECREF( result );
                result = NULL;
            }
        }
    }

    Py_LeaveRecursiveCall();

    if ( result != NULL )
    {
        return result;
    }

    // If it is not implemented, do identify checks as "==" and "!=" and
    // otherwise give an error
    if ( op == Py_EQ )
    {
        return INCREASE_REFCOUNT( BOOL_FROM( a == b ) );
    }
    else if ( op == Py_NE )
    {
        return INCREASE_REFCOUNT( BOOL_FROM( a != b ) );
    }
    else
    {
#if PYTHON_VERSION < 360
        PyErr_Format(
            PyExc_TypeError,
            "unorderable types: %s() %s %s()",
            a->ob_type->tp_name,
            op_strings[ op ],
            b->ob_type->tp_name
        );
#else
        PyErr_Format(
            PyExc_TypeError,
            "'%s' not supported between instances of '%s' and '%s'",
            op_strings[ op ],
            a->ob_type->tp_name,
            b->ob_type->tp_name
        );
#endif
        return NULL;
    }
}

PyObject *MY_RICHCOMPARE_NORECURSE( PyObject *a, PyObject *b, int op )
{
    CHECK_OBJECT( a );
    CHECK_OBJECT( b );

    bool checked_reverse_op = false;
    PyObject *result = NULL;
    richcmpfunc f;

    if ( a->ob_type != b->ob_type && PyType_IsSubtype( b->ob_type, a->ob_type ) )
    {
        f = b->ob_type->tp_richcompare;
        if ( f != NULL )
        {
            checked_reverse_op = true;

            result = (*f)( b, a, swapped_op[ op ] );

            if (unlikely( result == NULL ))
            {
                return NULL;
            }

            if ( result == Py_NotImplemented )
            {
                Py_DECREF( result );
                result = NULL;
            }

        }
    }

    if ( result == NULL )
    {
        f = a->ob_type->tp_richcompare;

        if ( f != NULL )
        {
            result = (*f)( a, b, op );

            if (unlikely( result == NULL ))
            {
                return NULL;
            }

            if ( result == Py_NotImplemented )
            {
                Py_DECREF( result );
                result = NULL;
            }
        }
    }

    if ( result == NULL && checked_reverse_op == false )
    {
        f = b->ob_type->tp_richcompare;

        if ( f != NULL )
        {
            result = (*f)( b, a, swapped_op[ op ] );

            if (unlikely( result == NULL ))
            {
                return NULL;
            }

            if ( result == Py_NotImplemented )
            {
                Py_DECREF( result );
                result = NULL;
            }
        }
    }

    if ( result != NULL )
    {
        return result;
    }

    // If it is not implemented, do identify checks as "==" and "!=" and
    // otherwise give an error
    if ( op == Py_EQ )
    {
        return INCREASE_REFCOUNT( BOOL_FROM( a == b ) );
    }
    else if ( op == Py_NE )
    {
        return INCREASE_REFCOUNT( BOOL_FROM( a != b ) );
    }
    else
    {
#if PYTHON_VERSION < 360
        PyErr_Format(
            PyExc_TypeError,
            "unorderable types: %s() %s %s()",
            a->ob_type->tp_name,
            op_strings[ op ],
            b->ob_type->tp_name
        );
#else
        PyErr_Format(
            PyExc_TypeError,
            "'%s' not supported between instances of '%s' and '%s'",
            op_strings[ op ],
            a->ob_type->tp_name,
            b->ob_type->tp_name
        );
#endif

        return NULL;
    }
}


#endif

PyObject *DEEP_COPY( PyObject *value )
{
    if ( PyDict_Check( value ) )
    {
        // For Python3.3, this can be done much faster in the same way as it is
        // done in parameter parsing.

#if PYTHON_VERSION < 330
        PyObject *result = _PyDict_NewPresized( ((PyDictObject *)value)->ma_used  );

        for ( Py_ssize_t i = 0; i <= ((PyDictObject *)value)->ma_mask; i++ )
        {
            PyDictEntry *entry = &((PyDictObject *)value)->ma_table[ i ];

            if ( entry->me_value != NULL )
            {
                PyObject *deep_copy = DEEP_COPY( entry->me_value );

                int res = PyDict_SetItem(
                    result,
                    entry->me_key,
                    deep_copy
                );

                Py_DECREF( deep_copy );

                if (unlikely( res != 0 ))
                {
                    return NULL;
                }
            }
        }

        return result;
#else
        /* Python 3.3 or higher */
        if ( _PyDict_HasSplitTable( (PyDictObject *)value) )
        {
            PyDictObject *mp = (PyDictObject *)value;

            PyObject **newvalues = PyMem_NEW( PyObject *, mp->ma_keys->dk_size );
            assert( newvalues != NULL );

            PyDictObject *result = PyObject_GC_New( PyDictObject, &PyDict_Type );
            assert( result != NULL );

            result->ma_values = newvalues;
            result->ma_keys = mp->ma_keys;
            result->ma_used = mp->ma_used;

            mp->ma_keys->dk_refcnt += 1;

            Nuitka_GC_Track( result );

#if PYTHON_VERSION < 360
            Py_ssize_t size = mp->ma_keys->dk_size;
#else
            Py_ssize_t size = DK_USABLE_FRACTION(DK_SIZE(mp->ma_keys));
#endif
            for ( Py_ssize_t i = 0; i < size; i++ )
            {
                if ( mp->ma_values[ i ] )
                {
                    result->ma_values[ i ] = DEEP_COPY( mp->ma_values[ i ] );
                }
                else
                {
                    result->ma_values[ i ] = NULL;
                }
            }

            return (PyObject *)result;
        }
        else
        {
            PyObject *result = _PyDict_NewPresized( ((PyDictObject *)value)->ma_used  );

            PyDictObject *mp = (PyDictObject *)value;

#if PYTHON_VERSION < 360
            Py_ssize_t size = mp->ma_keys->dk_size;
#else
            Py_ssize_t size = mp->ma_keys->dk_nentries;
#endif
            for ( Py_ssize_t i = 0; i < size; i++ )
            {
#if PYTHON_VERSION < 360
                PyDictKeyEntry *entry = &mp->ma_keys->dk_entries[i];
#else
                PyDictKeyEntry *entry = &DK_ENTRIES( mp->ma_keys )[ i ];
#endif

                if ( entry->me_value != NULL )
                {
                    PyObject *deep_copy = DEEP_COPY( entry->me_value );

                    PyDict_SetItem(
                        result,
                        entry->me_key,
                        deep_copy
                    );

                    Py_DECREF( deep_copy );
                }
            }

            return result;
        }
#endif
    }
    else if ( PyTuple_Check( value ) )
    {
        Py_ssize_t n = PyTuple_Size( value );
        PyObject *result = PyTuple_New( n );

        for( Py_ssize_t i = 0; i < n; i++ )
        {
            PyTuple_SET_ITEM( result, i, DEEP_COPY( PyTuple_GET_ITEM( value, i ) ) );
        }

        return result;
    }
    else if ( PyList_Check( value ) )
    {
        Py_ssize_t n = PyList_GET_SIZE( value );
        PyObject *result = PyList_New( n );

        for( Py_ssize_t i = 0; i < n; i++ )
        {
            PyList_SET_ITEM( result, i, DEEP_COPY( PyList_GET_ITEM( value, i ) ) );
        }

        return result;
    }
    else if ( PySet_Check( value ) )
    {
        // Sets cannot contain unhashable types, so they must be immutable.
        return PySet_New( value );
    }
    else if (
#if PYTHON_VERSION < 300
        PyString_Check( value )  ||
#endif
        PyUnicode_Check( value ) ||
#if PYTHON_VERSION < 300
        PyInt_Check( value )     ||
#endif
        PyLong_Check( value )    ||
        value == Py_None         ||
        PyBool_Check( value )    ||
        PyFloat_Check( value )   ||
        PyBytes_Check( value )   ||
        PyRange_Check( value )   ||
        PyType_Check( value )    ||
        PySlice_Check( value )   ||
        PyComplex_Check( value ) ||
        value == Py_Ellipsis
        )
    {
        return INCREASE_REFCOUNT( value );
    }
    else
    {
        PyErr_Format(
            PyExc_TypeError,
            "DEEP_COPY does not implement: %s",
            value->ob_type->tp_name
        );

        return NULL;
    }
}

#ifndef __NUITKA_NO_ASSERT__

static Py_hash_t DEEP_HASH_INIT( PyObject *value )
{
    // To avoid warnings about reduced sizes, we put an intermediate value
    // that is size_t.
    size_t value2 = (size_t)value;
    Py_hash_t result = (Py_hash_t)( value2 );

    if ( Py_TYPE( value ) != &PyType_Type )
    {
        result ^= DEEP_HASH( (PyObject *)Py_TYPE( value ) );
    }

    return result;
}

static void DEEP_HASH_BLOB( Py_hash_t *hash, char const *s, Py_ssize_t size )
{
    while( size > 0 )
    {
        *hash = ( 1000003 * (*hash) ) ^ (Py_hash_t)( *s++ );
        size--;
    }
}

static void DEEP_HASH_CSTR( Py_hash_t *hash, char const *s )
{
    DEEP_HASH_BLOB( hash, s, strlen( s ) );
}

// Hash function that actually verifies things done to the bit level. Can be
// used to detect corruption.
Py_hash_t DEEP_HASH( PyObject *value )
{
    assert( value != NULL );

    if ( PyType_Check( value ) )
    {
        Py_hash_t result = DEEP_HASH_INIT( value );

        DEEP_HASH_CSTR( &result, ((PyTypeObject *)value)->tp_name );
        return result;
    }
    else if ( PyDict_Check( value ) )
    {
        Py_hash_t result = DEEP_HASH_INIT( value );

        Py_ssize_t ppos = 0;
        PyObject *key, *dict_value;

        while( PyDict_Next( value, &ppos, &key, &dict_value ) )
        {
            if ( key != NULL && value != NULL )
            {
                result ^= DEEP_HASH( key );
                result ^= DEEP_HASH( dict_value );
            }
        }

        return result;
    }
    else if ( PyTuple_Check( value ) )
    {
        Py_hash_t result = DEEP_HASH_INIT( value );

        Py_ssize_t n = PyTuple_Size( value );

        for( Py_ssize_t i = 0; i < n; i++ )
        {
            result ^= DEEP_HASH( PyTuple_GET_ITEM( value, i ) );
        }

        return result;
    }
    else if ( PyList_Check( value ) )
    {
        Py_hash_t result = DEEP_HASH_INIT( value );

        Py_ssize_t n = PyList_GET_SIZE( value );

        for( Py_ssize_t i = 0; i < n; i++ )
        {
            result ^= DEEP_HASH( PyList_GET_ITEM( value, i ) );
        }

        return result;
    }
    else if ( PySet_Check( value ) )
    {
        Py_hash_t result = DEEP_HASH_INIT( value );

        PyObject *iterator = PyObject_GetIter( value );
        CHECK_OBJECT( iterator );

        while( true )
        {
            PyObject *item = PyIter_Next( iterator );
            if (!item) break;

            CHECK_OBJECT( item );

            result ^= DEEP_HASH( item );

            Py_DECREF( item );
        }

        Py_DECREF( iterator );

        return result;
    }
    else if ( PyLong_Check( value ) )
    {
        Py_hash_t result = DEEP_HASH_INIT( value );

        PyObject *exception_type, *exception_value;
        PyTracebackObject *exception_tb;

        FETCH_ERROR_OCCURRED_UNTRACED( &exception_type, &exception_value, &exception_tb );

        // Use string to hash the long value, which relies on that to not
        // use the object address.
        PyObject *str = PyObject_Str( value );
        result ^= DEEP_HASH( str );
        Py_DECREF( str );

        RESTORE_ERROR_OCCURRED_UNTRACED( exception_type, exception_value, exception_tb );

        return result;
    }
    else if ( PyUnicode_Check( value ) )
    {
        Py_hash_t result = DEEP_HASH( (PyObject *)Py_TYPE( value ) );

        PyObject *exception_type, *exception_value;
        PyTracebackObject *exception_tb;

        FETCH_ERROR_OCCURRED_UNTRACED( &exception_type, &exception_value, &exception_tb );

#if PYTHON_PYTHON >= 330
        Py_ssize_t size;
        char *s = PyUnicode_AsUTF8AndSize( value, &size );

        if ( s != NULL )
        {
            DEEP_HASH_BLOB( &result, s, size );
        }
#elif PYTHON_VERSION >= 300
        // Not done for Python3.2 really yet.
#else
        PyObject *str = PyUnicode_AsUTF8String( value );

        if ( str )
        {
            result ^= DEEP_HASH( str );
        }

        Py_DECREF( str );
#endif
        RESTORE_ERROR_OCCURRED_UNTRACED( exception_type, exception_value, exception_tb );

        return result;
    }
#if PYTHON_VERSION < 300
    else if ( PyString_Check( value ) )
    {
        Py_hash_t result = DEEP_HASH( (PyObject *)Py_TYPE( value ) );

        Py_ssize_t size;
        char *s;

        int res = PyString_AsStringAndSize( value, &s, &size );
        assert( res != -1 );

        DEEP_HASH_BLOB( &result, s, size );

        return result;
    }
#else
    else if ( PyBytes_Check( value ) )
    {
        Py_hash_t result = DEEP_HASH_INIT( value );

        Py_ssize_t size;
        char *s;

        int res = PyBytes_AsStringAndSize( value, &s, &size );
        assert( res != -1 );

        DEEP_HASH_BLOB( &result, s, size );

        return result;
    }
#endif
    else if ( value == Py_None || value == Py_Ellipsis )
    {
        return DEEP_HASH_INIT( value );
    }
    else if ( PyComplex_Check( value ) )
    {
        Py_complex c = PyComplex_AsCComplex( value );

        Py_hash_t result = DEEP_HASH_INIT( value );

        Py_ssize_t size = sizeof(c);
        char *s = (char *)&c;

        DEEP_HASH_BLOB( &result, s, size );

        return result;
    }
    else if ( PyFloat_Check( value ) )
    {
        double f = PyFloat_AsDouble( value );

        Py_hash_t result = DEEP_HASH_INIT( value );

        Py_ssize_t size = sizeof(f);
        char *s = (char *)&f;

        DEEP_HASH_BLOB( &result, s, size );

        return result;
    }
    else if (
#if PYTHON_VERSION < 300
        PyInt_Check( value )     ||
#endif
        PyBool_Check( value )    ||
        PyRange_Check( value )   ||
        PySlice_Check( value )
        )
    {
        Py_hash_t result = DEEP_HASH_INIT( value );

#if 0
        printf("Too simple deep hash: %s\n", Py_TYPE( value )->tp_name );
#endif

        return result;
    }
    else
    {
        assert( false );

        return -1;
    }
}
#endif

#if _NUITKA_PROFILE

timespec diff(timespec start, timespec end);

static timespec getTimespecDiff( timespec start, timespec end )
{
    timespec temp;

    if ( ( end.tv_nsec - start.tv_nsec ) < 0 )
    {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    }
    else
    {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }

    return temp;
}

static FILE *tempfile_profile;
static PyObject *vmprof_module;

static timespec time1, time2;

void startProfiling( void )
{
    tempfile_profile = fopen("nuitka-performance.dat", "wb");

    // Might be necessary to import "site" module to find "vmprof", lets just
    // hope we don't suffer too much from that. If we do, what might be done
    // is to try and just have the "PYTHONPATH" from it from out user.
    PyImport_ImportModule( "site" );
    vmprof_module = PyImport_ImportModule( "vmprof" );

    // Abort if it's not there.
    if ( vmprof_module == NULL )
    {
        PyErr_Print();
        abort();
    }

    PyObject *result = CALL_FUNCTION_WITH_ARGS1(
        PyObject_GetAttrString( vmprof_module, "enable"),
         PyInt_FromLong( fileno( tempfile_profile ) )
    );

    if ( result == NULL ) {
        PyErr_Print();
        abort();
    }

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1);
}

void stopProfiling( void )
{
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time2);

    // Save the current exception, if any, we must preserve it.
    PyObject *save_exception_type, *save_exception_value;
    PyTracebackObject *save_exception_tb;
    FETCH_ERROR_OCCURRED( &save_exception_type, &save_exception_value, &save_exception_tb );

    PyObject *result = CALL_FUNCTION_NO_ARGS(
        PyObject_GetAttrString( vmprof_module, "disable")
    );

    if ( result == NULL ) PyErr_Clear();

    fclose( tempfile_profile );

    FILE *tempfile_times = fopen( "nuitka-times.dat", "wb" );

    timespec diff = getTimespecDiff( time1, time2 );

    long delta_ns = diff.tv_sec * 1000000000 + diff.tv_nsec;
    fprintf( tempfile_times, "%ld\n", delta_ns);

    fclose( tempfile_times );

    RESTORE_ERROR_OCCURRED( save_exception_type, save_exception_value, save_exception_tb );
}

#endif

