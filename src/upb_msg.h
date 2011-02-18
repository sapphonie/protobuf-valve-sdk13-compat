/*
 * upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2010-2011 Joshua Haberman.  See LICENSE for details.
 *
 * Data structure for storing a message of protobuf data.  Unlike Google's
 * protobuf, upb_msg and upb_array are reference counted instead of having
 * exclusive ownership of their fields.  This is a better match for dynamic
 * languages where statements like a.b = other_b are normal.
 *
 * upb's parsers and serializers could also be used to populate and serialize
 * other kinds of message objects (even one generated by Google's protobuf).
 */

#ifndef UPB_MSG_H
#define UPB_MSG_H

#include "upb.h"
#include "upb_def.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// A pointer to a .proto value.  The owner must have an out-of-band way of
// knowing the type, so it knows which union member to use.
typedef union {
  double *_double;
  float *_float;
  int32_t *int32;
  int64_t *int64;
  uint8_t *uint8;
  uint32_t *uint32;
  uint64_t *uint64;
  bool *_bool;
  upb_string **str;
  upb_msg **msg;
  upb_array **arr;
  void *_void;
} upb_valueptr;

INLINE upb_valueptr upb_value_addrof(upb_value *val) {
  upb_valueptr ptr = {&val->val._double};
  return ptr;
}

// Reads or writes a upb_value from an address represented by a upb_value_ptr.
// We need to know the value type to perform this operation, because we need to
// know how much memory to copy (and for big-endian machines, we need to know
// where in the upb_value the data goes).
//
// For little endian-machines where we didn't mind overreading, we could make
// upb_value_read simply use memcpy().
INLINE upb_value upb_value_read(upb_valueptr ptr, upb_fieldtype_t ft) {
  upb_value val;

#ifdef NDEBUG
#define CASE(t, member_name) \
  case UPB_TYPE(t): val.val.member_name = *ptr.member_name; break;
#else
#define CASE(t, member_name) \
  case UPB_TYPE(t): val.val.member_name = *ptr.member_name; val.type = upb_types[ft].inmemory_type; break;
#endif

  switch(ft) {
    CASE(DOUBLE,   _double)
    CASE(FLOAT,    _float)
    CASE(INT32,    int32)
    CASE(INT64,    int64)
    CASE(UINT32,   uint32)
    CASE(UINT64,   uint64)
    CASE(SINT32,   int32)
    CASE(SINT64,   int64)
    CASE(FIXED32,  uint32)
    CASE(FIXED64,  uint64)
    CASE(SFIXED32, int32)
    CASE(SFIXED64, int64)
    CASE(BOOL,     _bool)
    CASE(ENUM,     int32)
    CASE(STRING,   str)
    CASE(BYTES,    str)
    CASE(MESSAGE,  msg)
    CASE(GROUP,    msg)
    case UPB_VALUETYPE_ARRAY:
      val.val.arr = *ptr.arr;
#ifndef NDEBUG
      val.type = UPB_VALUETYPE_ARRAY;
#endif
      break;
    default: assert(false);
  }
  return val;

#undef CASE
}

INLINE void upb_value_write(upb_valueptr ptr, upb_value val,
                            upb_fieldtype_t ft) {
#ifndef NDEBUG
  if (ft == UPB_VALUETYPE_ARRAY) {
    assert(val.type == UPB_VALUETYPE_ARRAY);
  } else if (val.type != UPB_VALUETYPE_RAW) {
    assert(val.type == upb_types[ft].inmemory_type);
  }
#endif
#define CASE(t, member_name) \
  case UPB_TYPE(t): *ptr.member_name = val.val.member_name; break;

  switch(ft) {
    CASE(DOUBLE,   _double)
    CASE(FLOAT,    _float)
    CASE(INT32,    int32)
    CASE(INT64,    int64)
    CASE(UINT32,   uint32)
    CASE(UINT64,   uint64)
    CASE(SINT32,   int32)
    CASE(SINT64,   int64)
    CASE(FIXED32,  uint32)
    CASE(FIXED64,  uint64)
    CASE(SFIXED32, int32)
    CASE(SFIXED64, int64)
    CASE(BOOL,     _bool)
    CASE(ENUM,     int32)
    CASE(STRING,   str)
    CASE(BYTES,    str)
    CASE(MESSAGE,  msg)
    CASE(GROUP,    msg)
    case UPB_VALUETYPE_ARRAY:
      *ptr.arr = val.val.arr;
      break;
    default: assert(false);
  }

#undef CASE
}

/* upb_array ******************************************************************/

typedef uint32_t upb_arraylen_t;
struct _upb_array {
  upb_atomic_refcount_t refcount;
  // "len" and "size" are measured in elements, not bytes.
  upb_arraylen_t len;
  upb_arraylen_t size;
  char *ptr;
};

void _upb_array_free(upb_array *a, upb_fielddef *f);
INLINE upb_valueptr _upb_array_getptr(upb_array *a, upb_fielddef *f,
                                      uint32_t elem) {
  upb_valueptr p;
  p._void = &a->ptr[elem * upb_types[f->type].size];
  return p;
}

upb_array *upb_array_new(void);

INLINE void upb_array_unref(upb_array *a, upb_fielddef *f) {
  if (a && upb_atomic_unref(&a->refcount)) _upb_array_free(a, f);
}

void upb_array_recycle(upb_array **arr, upb_fielddef *f);

INLINE uint32_t upb_array_len(upb_array *a) {
  return a->len;
}

INLINE upb_value upb_array_get(upb_array *arr, upb_fielddef *f,
                               upb_arraylen_t i) {
  assert(i < upb_array_len(arr));
  return upb_value_read(_upb_array_getptr(arr, f, i), f->type);
}

/* upb_msg ********************************************************************/

struct _upb_msg {
  upb_atomic_refcount_t refcount;
  uint8_t data[4];  // We allocate the appropriate amount per message.
};

void _upb_msg_free(upb_msg *msg, upb_msgdef *md);

INLINE upb_valueptr _upb_msg_getptr(upb_msg *msg, upb_fielddef *f) {
  upb_valueptr p;
  p._void = &msg->data[f->byte_offset];
  return p;
}

// Creates a new msg of the given type.
upb_msg *upb_msg_new(upb_msgdef *md);

// Unrefs the given message.
INLINE void upb_msg_unref(upb_msg *msg, upb_msgdef *md) {
  if (msg && upb_atomic_unref(&msg->refcount)) _upb_msg_free(msg, md);
}

void upb_msg_recycle(upb_msg **msg, upb_msgdef *msgdef);

// Tests whether the given field is explicitly set, or whether it will return a
// default.
INLINE bool upb_msg_has(upb_msg *msg, upb_fielddef *f) {
  return (msg->data[f->set_bit_offset] & f->set_bit_mask) != 0;
}

INLINE upb_value upb_msg_get(upb_msg *msg, upb_fielddef *f) {
  return upb_value_read(_upb_msg_getptr(msg, f), upb_field_valuetype(f));
}

// Unsets all field values back to their defaults.
INLINE void upb_msg_clear(upb_msg *msg, upb_msgdef *md) {
  memset(msg->data, 0, md->set_flags_bytes);
}

typedef struct {
  upb_msg *msg;
  upb_msgdef *msgdef;
} upb_msgpopulator_frame;

typedef struct {
  upb_msgpopulator_frame stack[UPB_MAX_NESTING], *top, *limit;
  upb_status status;
} upb_msgpopulator;

void upb_msgpopulator_init(upb_msgpopulator *p);
void upb_msgpopulator_uninit(upb_msgpopulator *p);
void upb_msgpopulator_reset(upb_msgpopulator *p, upb_msg *m, upb_msgdef *md);
void upb_msgpopulator_register_handlers(upb_msgpopulator *p, upb_handlers *h);

#ifdef __cplusplus
}  /* extern "C" */
#endif

#endif
