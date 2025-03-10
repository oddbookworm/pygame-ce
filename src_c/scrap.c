/*
    pygame-ce - Python Game Library
    Copyright (C) 2006, 2007 Rene Dudfield, Marcus von Appen

    Originally written and put in the public domain by Sam Lantinga.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* Handle clipboard text and data in arbitrary formats */
#include <limits.h>
#include <stdio.h>

#ifdef PG_SDL3
#include <SDL3/SDL.h>
#else
#include <SDL.h>
#include "SDL_syswm.h"
#endif

#include "pygame.h"

#include "pgcompat.h"

#include "doc/scrap_doc.h"

#include "scrap.h"

/**
 * Indicates, whether pygame.scrap was initialized or not.
 */
static int _scrapinitialized = 0;

/**
 * Currently active Clipboard object.
 */
static ScrapClipType _currentmode;
static PyObject *_selectiondata = NULL;
static PyObject *_clipdata = NULL;

/* Forward declarations. */
static PyObject *
_scrap_get_types(PyObject *self, PyObject *args);
static PyObject *
_scrap_contains(PyObject *self, PyObject *args);
static PyObject *
_scrap_get_scrap(PyObject *self, PyObject *args);
static PyObject *
_scrap_put_scrap(PyObject *self, PyObject *args);
static PyObject *
_scrap_lost_scrap(PyObject *self, PyObject *args);
static PyObject *
_scrap_set_mode(PyObject *self, PyObject *args);

static PyObject *
_scrap_get_text(PyObject *self, PyObject *args);
static PyObject *
_scrap_put_text(PyObject *self, PyObject *args);
static PyObject *
_scrap_has_text(PyObject *self, PyObject *args);

/* Determine what type of clipboard we are using */
#if !defined(__WIN32__)
#define SDL2_SCRAP
#include "scrap_sdl2.c"

#elif defined(__WIN32__)
#define WIN_SCRAP
#include "scrap_win.c"

#else
#error Unknown window manager for clipboard handling
#endif /* scrap type */

/**
 * \brief Indicates whether the scrap module is already initialized.
 *
 * \return 0 if the module is not initialized, 1, if it is.
 */
int
pygame_scrap_initialized(void)
{
    return _scrapinitialized;
}

/*
 * Initializes the pygame scrap module.
 */
static PyObject *
_scrap_init(PyObject *self, PyObject *args)
{
    VIDEO_INIT_CHECK();

    if (PyErr_WarnEx(PyExc_DeprecationWarning,
                     "pygame.scrap.init deprecated since 2.2.0", 1) == -1) {
        return NULL;
    }

    if (!pygame_scrap_initialized()) {
        Py_XDECREF(_clipdata);
        Py_XDECREF(_selectiondata);
        _clipdata = PyDict_New();
        _selectiondata = PyDict_New();
    }

    /* In case we've got not video surface, we won't initialize
     * anything.
     * Here is old SDL1 code for future reference
     * if (!SDL_GetVideoSurface())
     *     return RAISE(pgExc_SDLError, "No display mode is set");
     */
    if (!pygame_scrap_init())
        return RAISE(pgExc_SDLError, SDL_GetError());

    Py_RETURN_NONE;
}

/*
 * Indicates whether the scrap module is currently initialized.
 *
 * Note: All platforms supported here.
 */
static PyObject *
_scrap_get_init(PyObject *self, PyObject *_null)
{
    if (PyErr_WarnEx(PyExc_DeprecationWarning,
                     "pygame.scrap.get_init deprecated since 2.2.0",
                     1) == -1) {
        return NULL;
    }

    return PyBool_FromLong(pygame_scrap_initialized());
}

/*
 * Gets the currently available types from the active clipboard.
 */
static PyObject *
_scrap_get_types(PyObject *self, PyObject *_null)
{
    int i = 0;
    char **types;
    char *type;
    PyObject *list;
    PyObject *tmp;

    if (PyErr_WarnEx(PyExc_DeprecationWarning,
                     "pygame.scrap.get_types deprecated since 2.2.0",
                     1) == -1) {
        return NULL;
    }

    PYGAME_SCRAP_INIT_CHECK();
    if (!pygame_scrap_lost()) {
        switch (_currentmode) {
            case SCRAP_SELECTION:
                return PyDict_Keys(_selectiondata);
            case SCRAP_CLIPBOARD:
            default:
                return PyDict_Keys(_clipdata);
        }
    }

    list = PyList_New(0);
    types = pygame_scrap_get_types();
    if (!types)
        return list;
    while (types[i] != NULL) {
        type = types[i];
        tmp = PyUnicode_DecodeASCII(type, strlen(type), 0);
        if (!tmp) {
            Py_DECREF(list);
            return 0;
        }
        if (PyList_Append(list, tmp)) {
            Py_DECREF(list);
            Py_DECREF(tmp);
            return 0;
        }
        Py_DECREF(tmp);
        i++;
    }
    return list;
}

/*
 * Checks whether the active clipboard contains a certain type.
 */
static PyObject *
_scrap_contains(PyObject *self, PyObject *args)
{
    char *type = NULL;

    if (PyErr_WarnEx(PyExc_DeprecationWarning,
                     "pygame.scrap.contains deprecated since 2.2.0",
                     1) == -1) {
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "s", &type))
        return NULL;
    if (pygame_scrap_contains(type))
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

/*
 * Gets the content for a certain type from the active clipboard.
 */
static PyObject *
_scrap_get_scrap(PyObject *self, PyObject *args)
{
    char *scrap = NULL;
    PyObject *retval;
    char *scrap_type;
    size_t count;

    if (PyErr_WarnEx(PyExc_DeprecationWarning,
                     "pygame.scrap.get deprecated since 2.2.0. Consider using"
                     " pygame.scrap.get_text instead.",
                     1) == -1) {
        return NULL;
    }

    PYGAME_SCRAP_INIT_CHECK();

    if (!PyArg_ParseTuple(args, "s", &scrap_type))
        return NULL;

    if (!pygame_scrap_lost()) {
        /* Still own the clipboard. */
        PyObject *scrap_dict = NULL;
        PyObject *key = NULL;
        PyObject *val = NULL;

        switch (_currentmode) {
            case SCRAP_SELECTION:
                scrap_dict = _selectiondata;
                break;

            case SCRAP_CLIPBOARD:
            default:
                scrap_dict = _clipdata;
                break;
        }

        key = PyUnicode_FromString(scrap_type);
        if (NULL == key) {
            return PyErr_Format(PyExc_ValueError,
                                "invalid scrap data type identifier (%s)",
                                scrap_type);
        }

        val = PyDict_GetItemWithError(scrap_dict, key);
        Py_DECREF(key);

        if (NULL == val) {
            if (PyErr_Occurred()) {
                return PyErr_Format(PyExc_SystemError,
                                    "pygame.scrap internal error (key=%s)",
                                    scrap_type);
            }

            Py_RETURN_NONE;
        }

        Py_INCREF(val);
        return val;
    }

    /* pygame_get_scrap() only returns NULL or !NULL, but won't set any
     * errors. */
    scrap = pygame_scrap_get(scrap_type, &count);
    if (!scrap)
        Py_RETURN_NONE;

    retval = PyBytes_FromStringAndSize(scrap, count);
#if defined(PYGAME_SCRAP_FREE_STRING)
    free(scrap);
#endif

    return retval;
}

/*
 * This will put a python string into the clipboard.
 */
static PyObject *
_scrap_put_scrap(PyObject *self, PyObject *args)
{
    Py_ssize_t scraplen;
    char *scrap = NULL;
    char *scrap_type;
    PyObject *tmp;
    static const char argfmt[] = "sy#";

    if (PyErr_WarnEx(PyExc_DeprecationWarning,
                     "pygame.scrap.put deprecated since 2.2.0. Consider using"
                     " pygame.scrap.put_text instead.",
                     1) == -1) {
        return NULL;
    }

    PYGAME_SCRAP_INIT_CHECK();

    if (!PyArg_ParseTuple(args, argfmt, &scrap_type, &scrap, &scraplen)) {
        return NULL;
    }

    /* Set it in the clipboard. */
    if (!pygame_scrap_put(scrap_type, scraplen, scrap))
        return RAISE(pgExc_SDLError,
                     "content could not be placed in clipboard.");

    /* Add or replace the set value. */
    switch (_currentmode) {
        case SCRAP_SELECTION: {
            tmp = PyBytes_FromStringAndSize(scrap, scraplen);
            PyDict_SetItemString(_selectiondata, scrap_type, tmp);
            Py_DECREF(tmp);
            break;
        }
        case SCRAP_CLIPBOARD:
        default: {
            tmp = PyBytes_FromStringAndSize(scrap, scraplen);
            PyDict_SetItemString(_clipdata, scrap_type, tmp);
            Py_DECREF(tmp);
            break;
        }
    }

    Py_RETURN_NONE;
}

/*
 * Checks whether the pygame window has lost the clipboard.
 */
static PyObject *
_scrap_lost_scrap(PyObject *self, PyObject *_null)
{
    PYGAME_SCRAP_INIT_CHECK();

    if (PyErr_WarnEx(PyExc_DeprecationWarning,
                     "pygame.scrap.lost deprecated since 2.2.0", 1) == -1) {
        return NULL;
    }

    if (pygame_scrap_lost())
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

/*
 * Sets the clipboard mode. This only works for the X11 environment, which
 * diverses between mouse selections and the clipboard.
 */
static PyObject *
_scrap_set_mode(PyObject *self, PyObject *args)
{
    PYGAME_SCRAP_INIT_CHECK();

    if (PyErr_WarnEx(PyExc_DeprecationWarning,
                     "pygame.scrap.set_mode deprecated since 2.2.0",
                     1) == -1) {
        return NULL;
    }

    if (!PyArg_ParseTuple(args, "i", &_currentmode))
        return NULL;

    if (_currentmode != SCRAP_CLIPBOARD && _currentmode != SCRAP_SELECTION)
        return RAISE(PyExc_ValueError, "invalid clipboard mode");

    /* Force the clipboard, if not in a X11 environment. */
    _currentmode = SCRAP_CLIPBOARD;
    Py_RETURN_NONE;
}

/**
 * @brief Fetches a python string from the SDL clipboard. If
 *        there is nothing in the clipboard, it will return empty
 *
 * @return PyObject*
 */
static PyObject *
_scrap_get_text(PyObject *self, PyObject *args)
{
    const SDL_bool hasText = SDL_HasClipboardText();

    char *text = SDL_GetClipboardText();

    // if SDL_GetClipboardText fails, it returns an empty string
    // hasText helps determine if an actual error occurred
    // vs just an empty string in the clipboard
    if (*text == '\0' && hasText == SDL_TRUE) {
        SDL_free(text);
        return RAISE(pgExc_SDLError, SDL_GetError());
    }

    PyObject *returnValue = PyUnicode_FromString(text);
    SDL_free(text);

    return returnValue;
}

/**
 * @brief Puts a python string into the SDL clipboard
 *
 * @param args A python string to be put into the clipboard
 *
 * @return PyObject*
 */
static PyObject *
_scrap_put_text(PyObject *self, PyObject *args)
{
    char *text;

    if (!PyArg_ParseTuple(args, "s", &text)) {
        return NULL;
    }

    if (SDL_SetClipboardText(text)) {
        return RAISE(pgExc_SDLError, SDL_GetError());
    }

    Py_RETURN_NONE;
}

/**
 * @brief If the SDL clipboard has something in it, will return True.
 *          Else it returns False.
 *
 * @return PyObject*
 */
static PyObject *
_scrap_has_text(PyObject *self, PyObject *args)
{
    const SDL_bool hasText = SDL_HasClipboardText();

    if (hasText) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyMethodDef scrap_builtins[] = {
/*
 * Only initialise these functions for ones we know about.
 *
 * Note, the macosx stuff is done in sdlosx_main.m
 */
#if (defined(WIN_SCRAP) || defined(SDL2_SCRAP))

    {"init", _scrap_init, 1, DOC_SCRAP_INIT},
    {"get_init", _scrap_get_init, METH_NOARGS, DOC_SCRAP_GETINIT},
    {"contains", _scrap_contains, METH_VARARGS, DOC_SCRAP_CONTAINS},
    {"get", _scrap_get_scrap, METH_VARARGS, DOC_SCRAP_GET},
    {"get_types", _scrap_get_types, METH_NOARGS, DOC_SCRAP_GETTYPES},
    {"put", _scrap_put_scrap, METH_VARARGS, DOC_SCRAP_PUT},
    {"lost", _scrap_lost_scrap, METH_NOARGS, DOC_SCRAP_LOST},
    {"set_mode", _scrap_set_mode, METH_VARARGS, DOC_SCRAP_SETMODE},

#endif
    {"get_text", _scrap_get_text, METH_NOARGS, DOC_SCRAP_GETTEXT},
    {"has_text", _scrap_has_text, METH_NOARGS, DOC_SCRAP_HASTEXT},
    {"put_text", _scrap_put_text, METH_VARARGS, DOC_SCRAP_PUTTEXT},
    {NULL, NULL, 0, NULL}};

MODINIT_DEFINE(scrap)
{
    static struct PyModuleDef _module = {PyModuleDef_HEAD_INIT,
                                         "scrap",
                                         DOC_SCRAP,
                                         -1,
                                         scrap_builtins,
                                         NULL,
                                         NULL,
                                         NULL,
                                         NULL};

    /* imported needed apis; Do this first so if there is an error
       the module is not loaded.
    */
    import_pygame_base();
    if (PyErr_Occurred()) {
        return NULL;
    }

    /* create the module */
    return PyModule_Create(&_module);
}
