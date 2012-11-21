#include <Python.h>
#include <structmember.h>

#include <db.h>


#if PY_MAJOR_VERSION >= 3
#define IS_PY3K
#endif

#define COLUMN_TYPE_FIXED 0
#define COLUMN_TYPE_VARIABLE 1 
    
#define ELEMENT_TYPE_CHAR  0
#define ELEMENT_TYPE_INT_1  1
#define ELEMENT_TYPE_INT_2  2
#define ELEMENT_TYPE_INT_4  3
#define ELEMENT_TYPE_INT_8  4
#define ELEMENT_TYPE_FLOAT  5
 

#define MODULE_DOC \
"Low level Berkeley DB interface for vcfdb"

static PyObject *BerkeleyDatabaseError;

typedef struct {
    PyObject_HEAD
    DB *primary_db;
    DB **secondary_dbs;
    unsigned int num_secondary_dbs;
} BerkeleyDatabase;

typedef struct {
    PyObject_HEAD
    BerkeleyDatabase *database;
    /* Record storage */
    u_int32_t max_record_size;
    unsigned char *record_buffer;
    u_int32_t record_buffer_size;
    int current_record_size;
    u_int32_t current_record_offset;
    /* Key storage */
    unsigned char *key_buffer;
    u_int32_t key_size;
    /* DBT management */
    u_int32_t num_records;
    u_int32_t max_num_records;
    DBT *keys; 
    DBT *records; 
    /* Used for Python buffer protocal compliance */
    int num_views; 
} RecordBuffer;

static void 
handle_bdb_error(int err)
{
    PyErr_SetString(BerkeleyDatabaseError, db_strerror(err));
}
/*==========================================================
 * BerkeleyDatabase object 
 *==========================================================
 */

static void
BerkeleyDatabase_dealloc(BerkeleyDatabase* self)
{
    int j;
    /* make sure that the DB handles are closed. We can ignore errors here. */
    for (j = 0; j < self->num_secondary_dbs; j++) {
        if (self->secondary_dbs[j] != NULL) {
            self->secondary_dbs[j]->close(self->secondary_dbs[j], 0); 
        }
    }
    if (self->primary_db != NULL) {
        self->primary_db->close(self->primary_db, 0); 
    }
    Py_TYPE(self)->tp_free((PyObject*)self);
}

static int
BerkeleyDatabase_init(BerkeleyDatabase *self, PyObject *args, PyObject *kwds)
{
    int ret = 0;
    int db_ret = 0;
    self->primary_db = NULL;
    self->secondary_dbs = NULL;
    self->num_secondary_dbs = 0;
    
    db_ret = db_create(&self->primary_db, NULL, 0);
    if (db_ret != 0) {
        handle_bdb_error(db_ret);
        ret = -1;
        goto out;    
    }

out:

    return ret;
}


static PyMemberDef BerkeleyDatabase_members[] = {
    {NULL}  /* Sentinel */
};


static PyObject *
BerkeleyDatabase_create(BerkeleyDatabase* self)
{
    PyObject *ret = NULL;
    int db_ret;
    u_int32_t flags;
    char *db_name = "tmp/primary.db";
    
    db_ret = self->primary_db->set_cachesize(self->primary_db, 0, 
            32 * 1024 * 1024, 1);
    if (db_ret != 0) {
        handle_bdb_error(db_ret);
        goto out;    
    }
    flags = DB_CREATE|DB_TRUNCATE;
    db_ret = self->primary_db->open(self->primary_db, NULL, db_name, NULL, 
            DB_BTREE,  flags,  0);         
    if (db_ret != 0) {
        handle_bdb_error(db_ret);
        goto out;    
    }

    ret = Py_BuildValue("");
out:
    return ret; 
}

static PyObject *
BerkeleyDatabase_open(BerkeleyDatabase* self)
{
    int db_ret;
    PyObject *ret = NULL;
    u_int32_t flags = DB_RDONLY;
    char *db_name = "tmp/primary.db";
    db_ret = self->primary_db->open(self->primary_db, NULL, db_name, NULL, 
            DB_UNKNOWN,  flags,  0);         
    if (db_ret != 0) {
        handle_bdb_error(db_ret);
        goto out;    
    }

    ret = Py_BuildValue("");
out:
    return ret; 
}


static PyObject *
BerkeleyDatabase_close(BerkeleyDatabase* self)
{
    PyObject *ret = NULL;
    int db_ret;
    if (self->primary_db != NULL) {
        db_ret = self->primary_db->close(self->primary_db, 0); 
        if (db_ret != 0) {
            handle_bdb_error(db_ret);
            goto out;    
        }
    }
    ret = Py_BuildValue("");
out:
    self->primary_db = NULL;
    return ret; 
}


static PyMethodDef BerkeleyDatabase_methods[] = {
    {"open", (PyCFunction) BerkeleyDatabase_open, METH_NOARGS, "Open the table" },
    {"create", (PyCFunction) BerkeleyDatabase_create, METH_NOARGS, "Open the table" },
    {"close", (PyCFunction) BerkeleyDatabase_close, METH_NOARGS, "Close the table" },
    {NULL}  /* Sentinel */
};


static PyTypeObject BerkeleyDatabaseType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_vcfdb.BerkeleyDatabase",             /* tp_name */
    sizeof(BerkeleyDatabase),             /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)BerkeleyDatabase_dealloc, /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    0,                         /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT |
        Py_TPFLAGS_BASETYPE,   /* tp_flags */
    "BerkeleyDatabase objects",           /* tp_doc */
    0,                     /* tp_traverse */
    0,                     /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    BerkeleyDatabase_methods,             /* tp_methods */
    BerkeleyDatabase_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)BerkeleyDatabase_init,      /* tp_init */
};
   


/*==========================================================
 * RecordBuffer object 
 *==========================================================
 */

static void
RecordBuffer_dealloc(RecordBuffer* self)
{
    /* Is this safe?? */
    PyMem_Free(self->record_buffer);
    PyMem_Free(self->key_buffer);
    PyMem_Free(self->keys);
    PyMem_Free(self->records);
    Py_XDECREF(self->database);
    Py_TYPE(self)->tp_free((PyObject*)self);
}

/* 
 * Resets this buffer so that it is ready to receive more records.
 */
static void
RecordBuffer_clear(RecordBuffer *self)
{
    memset(self->record_buffer, 0, self->current_record_offset);
    self->current_record_size = 0;
    self->current_record_offset = 0;
    self->num_records = 0;

}

static int
RecordBuffer_init(RecordBuffer *self, PyObject *args, PyObject *kwds)
{
    int ret = 0; 
    static char *kwlist[] = {"database", NULL};
    BerkeleyDatabase *database = NULL;
    u_int32_t n; 
    if (! PyArg_ParseTupleAndKeywords(args, kwds, "O!", kwlist, 
            &BerkeleyDatabaseType, &database)) {
        ret = -1;
        goto out;
    }
    Py_INCREF(database);
    self->database = database;

        
    /* TODO read params from args and error check */
    n = 1024;
    self->key_size = 8;
    self->max_num_records = n;
    self->max_record_size = 64 * 1024; /* TEMP 64K */
    self->record_buffer_size = 256 * 1024; /* TEMP 256K*/
    self->record_buffer = PyMem_Malloc(self->record_buffer_size);
    self->key_buffer = PyMem_Malloc(n * self->key_size);
    self->keys = PyMem_Malloc(n * sizeof(DBT));
    self->records = PyMem_Malloc(n * sizeof(DBT));
    memset(self->keys, 0, n * sizeof(DBT));
    memset(self->records, 0, n * sizeof(DBT));
    self->num_views = 0; 

out:
    return ret;
}

static PyMemberDef RecordBuffer_members[] = {
    {"database", T_OBJECT_EX, offsetof(RecordBuffer, database), READONLY, "database"},
    {"current_record_size", T_INT, offsetof(RecordBuffer, current_record_size), 0, "size of current record"},
    {NULL}  /* Sentinel */
};

static PyObject *
RecordBuffer_flush(RecordBuffer* self)
{
    int db_ret = 0;
    PyObject *ret = NULL;
    u_int32_t flags = 0;
    u_int32_t j;
    DB *db;
    
    db = self->database->primary_db;
    /* TODO error check or verify this can't be null */
    printf("Flushing buffer: %d records\n", self->num_records);
    for (j = 0; j < self->num_records; j++) {
        db_ret = db->put(db, NULL, &self->keys[j], &self->records[j], flags);
        if (db_ret != 0) {
            handle_bdb_error(db_ret); 
            goto out;
        } 
    }
    RecordBuffer_clear(self);
    
    ret = Py_BuildValue("");
out:
    return ret; 
}


static PyObject *
RecordBuffer_commit_record(RecordBuffer* self, PyObject *args)
{
    PyObject *ret = NULL;
    Py_buffer key_buff;
    unsigned char *key = &self->key_buffer[self->key_size * self->num_records];
    unsigned char *record = &self->record_buffer[self->current_record_offset];
    u_int32_t record_size = (u_int32_t) self->current_record_size;
    u_int32_t barrier = self->record_buffer_size - self->max_record_size;
    if (!PyArg_ParseTuple(args, "y*", &key_buff)) { 
        goto out; 
    }
    if (key_buff.len != self->key_size) {
        PyErr_SetString(PyExc_ValueError, "bad key size");
        PyBuffer_Release(&key_buff);
        goto out;
    }
    /* copy the key */
    memcpy(key, key_buff.buf, self->key_size);
    PyBuffer_Release(&key_buff);
    if (record_size > self->max_record_size) {
        PyErr_SetString(PyExc_ValueError, "record size too large");
        goto out;
    }
    self->keys[self->num_records].size = self->key_size;
    self->keys[self->num_records].data = key;
    self->records[self->num_records].size = record_size; 
    self->records[self->num_records].data = record;
    /* We are done setting up the record, so we can increment counters */ 
    self->current_record_offset += record_size;
    self->num_records++;
    if (self->current_record_offset >= barrier 
            || self->num_records == self->max_num_records) {
        ret = RecordBuffer_flush(self);
        if (ret == NULL) {
            goto out;
        }
    }

    ret = Py_BuildValue("");
out:
    return ret; 
}


/* Implementation of the Buffer Protocol so that we can write values 
 * directly into our record buffer memory from Python. This implementation 
 * copies the bytearray (Objects/bytearrayobject.c), and doesn't
 * strictly need to have the self->num_views, as the memory is always
 * valid until the object is freed.
 */
static int
RecordBuffer_getbuffer(RecordBuffer *self, Py_buffer *view, int flags)
{
    int ret = 0;
    void *ptr;
    if (view == NULL) {
        self->num_views++;
        goto out; 
    }
    ptr = &self->record_buffer[self->current_record_offset];
    ret = PyBuffer_FillInfo(view, (PyObject*) self, ptr, self->max_record_size,
            0, flags);
    if (ret >= 0) {
        self->num_views++;
    }
out:
    return ret;
}
static void
RecordBuffer_releasebuffer(RecordBuffer *self, Py_buffer *view)
{
    self->num_views--;
}

static PyMethodDef RecordBuffer_methods[] = {
    {"commit_record", (PyCFunction) RecordBuffer_commit_record, METH_VARARGS, "commit record" },
    {"flush", (PyCFunction) RecordBuffer_flush, METH_NOARGS, "flush" },
    {NULL}  /* Sentinel */
};

static PyBufferProcs RecordBuffer_as_buffer = {
    (getbufferproc)RecordBuffer_getbuffer,
    (releasebufferproc)RecordBuffer_releasebuffer,
};


static PyTypeObject RecordBufferType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "_vcfdb.RecordBuffer",             /* tp_name */
    sizeof(RecordBuffer),             /* tp_basicsize */
    0,                         /* tp_itemsize */
    (destructor)RecordBuffer_dealloc, /* tp_dealloc */
    0,                         /* tp_print */
    0,                         /* tp_getattr */
    0,                         /* tp_setattr */
    0,                         /* tp_reserved */
    0,                         /* tp_repr */
    0,                         /* tp_as_number */
    0,                         /* tp_as_sequence */
    0,                         /* tp_as_mapping */
    0,                         /* tp_hash  */
    0,                         /* tp_call */
    0,                         /* tp_str */
    0,                         /* tp_getattro */
    0,                         /* tp_setattro */
    &RecordBuffer_as_buffer,   /* tp_as_buffer */
    Py_TPFLAGS_DEFAULT,        /* tp_flags */
    "RecordBuffer objects",           /* tp_doc */
    0,                     /* tp_traverse */
    0,                     /* tp_clear */
    0,                     /* tp_richcompare */
    0,                     /* tp_weaklistoffset */
    0,                     /* tp_iter */
    0,                     /* tp_iternext */
    RecordBuffer_methods,             /* tp_methods */
    RecordBuffer_members,             /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)RecordBuffer_init,      /* tp_init */
};
 


/* Initialisation code supports Python 2.x and 3.x. The framework uses the 
 * recommended structure from http://docs.python.org/howto/cporting.html. 
 * I've ignored the point about storing state in globals, as the examples 
 * from the Python documentation still use this idiom. 
 */

#if PY_MAJOR_VERSION >= 3

static struct PyModuleDef vcfdbmodule = {
    PyModuleDef_HEAD_INIT,
    "_vcfdb",   /* name of module */
    MODULE_DOC, /* module documentation, may be NULL */
    -1,    
    NULL, NULL, NULL, NULL, NULL 
};

#define INITERROR return NULL

PyObject * 
PyInit__vcfdb(void)

#else
#define INITERROR return

void
init_vcfdb(void)
#endif
{
#if PY_MAJOR_VERSION >= 3
    PyObject *module = PyModule_Create(&vcfdbmodule);
#else
    PyObject *module = Py_InitModule3("_vcfdb", NULL, MODULE_DOC);
#endif
    if (module == NULL) {
        INITERROR;
    }
    BerkeleyDatabaseType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&BerkeleyDatabaseType) < 0) {
        INITERROR;
    }
    Py_INCREF(&BerkeleyDatabaseType);
    PyModule_AddObject(module, "BerkeleyDatabase", 
            (PyObject *) &BerkeleyDatabaseType);
    RecordBufferType.tp_new = PyType_GenericNew;
    if (PyType_Ready(&RecordBufferType) < 0) {
        INITERROR;
    }
    Py_INCREF(&RecordBufferType);
    PyModule_AddObject(module, "RecordBuffer", (PyObject *) &RecordBufferType);
    
    BerkeleyDatabaseError = PyErr_NewException("_vcfdb.BerkeleyDatabaseError", 
            NULL, NULL);
    Py_INCREF(BerkeleyDatabaseError);
    PyModule_AddObject(module, "BerkeleyDatabaseError", BerkeleyDatabaseError);

    PyModule_AddIntConstant(module, "COLUMN_TYPE_FIXED", COLUMN_TYPE_FIXED);
    PyModule_AddIntConstant(module, "COLUMN_TYPE_VARIABLE", COLUMN_TYPE_VARIABLE);
    
    PyModule_AddIntConstant(module, "ELEMENT_TYPE_CHAR", ELEMENT_TYPE_CHAR);
    PyModule_AddIntConstant(module, "ELEMENT_TYPE_INT_1", ELEMENT_TYPE_INT_1);
    PyModule_AddIntConstant(module, "ELEMENT_TYPE_INT_2", ELEMENT_TYPE_INT_2);
    PyModule_AddIntConstant(module, "ELEMENT_TYPE_INT_4", ELEMENT_TYPE_INT_4);
    PyModule_AddIntConstant(module, "ELEMENT_TYPE_INT_8", ELEMENT_TYPE_INT_8);
    PyModule_AddIntConstant(module, "ELEMENT_TYPE_FLOAT", ELEMENT_TYPE_FLOAT);

#if PY_MAJOR_VERSION >= 3
    return module;
#endif
}


