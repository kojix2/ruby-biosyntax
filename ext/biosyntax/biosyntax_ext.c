/*
 * Ruby native extension for libbiosyntax.
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include "ruby.h"
#include "ruby/encoding.h"
#include "biosyntax.h"

#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static VALUE mBioSyntax;
static VALUE mNative;
static VALUE cNativeState;
static VALUE cSpan;

typedef struct {
    biosyn_state_t *ptr;
} rb_biosyn_state_t;

typedef struct {
    biosyn_span_t local[64];
    biosyn_span_t *spans;
    uint64_t count;
    int heap_allocated;
} span_buffer_t;

typedef struct {
    biosyn_span_t *spans;
    uint64_t count;
} span_array_args_t;

/* Wrap the native parser state in a Ruby typed data object. */
static void state_free(void *data) {
    rb_biosyn_state_t *wrap = (rb_biosyn_state_t *)data;
    if (!wrap)
        return;

    if (wrap->ptr) {
        biosyn_state_free(wrap->ptr);
        wrap->ptr = NULL;
    }
    xfree(wrap);
}

static size_t state_memsize(const void *data) {
    const rb_biosyn_state_t *wrap = (const rb_biosyn_state_t *)data;
    return wrap ? sizeof(*wrap) + (wrap->ptr ? sizeof(*wrap->ptr) : 0) : 0;
}

static const rb_data_type_t state_type = {"BioSyntax::Native::State",
                                          {0, state_free, state_memsize, 0},
                                          0,
                                          0,
                                          RUBY_TYPED_FREE_IMMEDIATELY};

static rb_biosyn_state_t *get_state(VALUE self) {
    rb_biosyn_state_t *wrap = NULL;
    TypedData_Get_Struct(self, rb_biosyn_state_t, &state_type, wrap);
    if (!wrap || !wrap->ptr) {
        rb_raise(rb_eRuntimeError, "uninitialized BioSyntax native state");
    }
    return wrap;
}

static VALUE state_alloc(VALUE klass) {
    rb_biosyn_state_t *wrap = ALLOC(rb_biosyn_state_t);
    wrap->ptr = NULL;
    return TypedData_Wrap_Struct(klass, &state_type, wrap);
}

static biosyn_format_t format_id_from_value(VALUE value) {
    unsigned int id = NUM2UINT(value);
    if (id == BIOSYN_FORMAT_UNKNOWN || id >= biosyn_format_count()) {
        rb_raise(rb_eArgError, "unsupported libbiosyntax format id: %u", id);
    }
    return (biosyn_format_t)id;
}

static VALUE state_initialize(VALUE self, VALUE format_id_value) {
    rb_biosyn_state_t *wrap = NULL;
    biosyn_format_t format_id = format_id_from_value(format_id_value);

    TypedData_Get_Struct(self, rb_biosyn_state_t, &state_type, wrap);
    if (wrap->ptr) {
        biosyn_state_free(wrap->ptr);
        wrap->ptr = NULL;
    }

    wrap->ptr = biosyn_state_new(format_id);
    if (!wrap->ptr) {
        rb_raise(rb_eNoMemError, "could not allocate libbiosyntax state");
    }

    return self;
}

static VALUE state_reset(int argc, VALUE *argv, VALUE self) {
    rb_biosyn_state_t *wrap = get_state(self);
    biosyn_format_t format_id = wrap->ptr->format;

    if (argc > 1) {
        rb_error_arity(argc, 0, 1);
    }
    if (argc == 1 && !NIL_P(argv[0])) {
        format_id = format_id_from_value(argv[0]);
    }

    biosyn_state_init(wrap->ptr, format_id);
    return self;
}

static VALUE state_line_no(VALUE self) {
    rb_biosyn_state_t *wrap = get_state(self);
    return ULL2NUM((unsigned long long)wrap->ptr->line_no);
}

static VALUE state_format_id(VALUE self) {
    rb_biosyn_state_t *wrap = get_state(self);
    return UINT2NUM(wrap->ptr->format);
}

/* Collect spans for one line. Most lines fit in span_buffer_t.local; larger
 * results fall back to a heap allocation released through rb_ensure.
 */
static void highlight_into_buffer(biosyn_state_t *state, const char *ptr, uint64_t len,
                                  span_buffer_t *buf) {
    uint64_t needed;

    buf->spans = NULL;
    buf->count = 0;
    buf->heap_allocated = 0;

    needed = biosyn_highlight_next_line(state, ptr, len, buf->local, 64);
    if (needed <= 64) {
        if (needed == 0)
            return;

        buf->spans = buf->local;
        buf->count = needed;
        return;
    }

    if (needed > (uint64_t)(SIZE_MAX / sizeof(biosyn_span_t))) {
        rb_raise(rb_eNoMemError, "too many highlight spans");
    }

    buf->spans = ALLOC_N(biosyn_span_t, (size_t)needed);
    buf->heap_allocated = 1;
    buf->count = biosyn_highlight_next_line(state, ptr, len, buf->spans, needed);

    if (buf->count > needed) {
        xfree(buf->spans);
        buf->spans = NULL;
        buf->count = 0;
        rb_raise(rb_eRuntimeError, "libbiosyntax span count changed while highlighting");
    }
}

static VALUE free_span_buffer(VALUE arg) {
    span_buffer_t *buf = (span_buffer_t *)arg;
    if (buf && buf->heap_allocated && buf->spans) {
        xfree(buf->spans);
    }
    if (buf) {
        buf->spans = NULL;
        buf->count = 0;
        buf->heap_allocated = 0;
    }
    return Qnil;
}

static VALUE span_class(void) {
    if (NIL_P(cSpan)) {
        cSpan = rb_const_get(mBioSyntax, rb_intern("Span"));
        rb_gc_register_mark_object(cSpan);
    }

    return cSpan;
}

/* Convert native byte spans into BioSyntax::Span objects. */
static VALUE build_span_array(VALUE arg) {
    span_array_args_t *args = (span_array_args_t *)arg;
    VALUE klass = span_class();
    VALUE ary;
    uint64_t i;

    if (args->count > (uint64_t)LONG_MAX) {
        rb_raise(rb_eRuntimeError, "too many highlight spans for Ruby Array");
    }

    ary = rb_ary_new_capa((long)args->count);
    for (i = 0; i < args->count; i++) {
        VALUE argv[3];
        argv[0] = ULL2NUM((unsigned long long)args->spans[i].start);
        argv[1] = ULL2NUM((unsigned long long)args->spans[i].length);
        argv[2] = UINT2NUM(args->spans[i].class_id);
        rb_ary_push(ary, rb_class_new_instance(3, argv, klass));
    }

    return ary;
}

static VALUE state_highlight_body(VALUE arg) {
    span_buffer_t *buf = (span_buffer_t *)arg;
    span_array_args_t args;
    args.spans = buf->spans;
    args.count = buf->count;
    return build_span_array((VALUE)&args);
}

static VALUE state_highlight(VALUE self, VALUE line_value) {
    rb_biosyn_state_t *wrap = get_state(self);
    span_buffer_t buf;
    VALUE line = StringValue(line_value);
    long len = RSTRING_LEN(line);

    if (len < 0) {
        rb_raise(rb_eArgError, "line is too large");
    }

    highlight_into_buffer(wrap->ptr, RSTRING_PTR(line), (uint64_t)len, &buf);
    return rb_ensure(state_highlight_body, (VALUE)&buf, free_span_buffer, (VALUE)&buf);
}

typedef struct {
    VALUE line;
    span_buffer_t *buf;
} colorize_args_t;

/* Render ANSI output after spans have already been calculated. */
static VALUE state_colorize_body(VALUE arg) {
    colorize_args_t *args = (colorize_args_t *)arg;
    VALUE line = args->line;
    const char *ptr = RSTRING_PTR(line);
    uint64_t len = (uint64_t)RSTRING_LEN(line);
    uint64_t needed;
    VALUE out;

    if (args->buf->count == 0) {
        return rb_str_dup(line);
    }

    needed = biosyn_render_ansi_line(ptr, len, args->buf->spans, args->buf->count, NULL, 0);
    if (needed > (uint64_t)(LONG_MAX - 1)) {
        rb_raise(rb_eNoMemError, "ANSI output is too large");
    }

    out = rb_str_new(NULL, (long)needed + 1);
    biosyn_render_ansi_line(ptr, len, args->buf->spans, args->buf->count, RSTRING_PTR(out),
                            needed + 1);
    rb_str_set_len(out, (long)needed);
    rb_enc_copy(out, line);
    return out;
}

static VALUE state_colorize(VALUE self, VALUE line_value) {
    rb_biosyn_state_t *wrap = get_state(self);
    span_buffer_t buf;
    colorize_args_t args;
    VALUE line = StringValue(line_value);
    long len = RSTRING_LEN(line);

    if (len < 0) {
        rb_raise(rb_eArgError, "line is too large");
    }

    highlight_into_buffer(wrap->ptr, RSTRING_PTR(line), (uint64_t)len, &buf);
    args.line = line;
    args.buf = &buf;
    return rb_ensure(state_colorize_body, (VALUE)&args, free_span_buffer, (VALUE)&buf);
}

static VALUE native_libbiosyntax_version(VALUE self) {
    (void)self;
    return rb_str_new_cstr(biosyn_version());
}

static VALUE native_abi_version(VALUE self) {
    (void)self;
    return UINT2NUM(biosyn_abi_version());
}

static VALUE native_format_id_from_name(VALUE self, VALUE name_value) {
    VALUE name = StringValue(name_value);
    biosyn_format_t format_id;
    (void)self;

    format_id = biosyn_format_from_name(StringValueCStr(name));
    return UINT2NUM(format_id);
}

static VALUE native_guess_format_id(VALUE self, VALUE path_value) {
    VALUE path = StringValue(path_value);
    biosyn_format_t format_id;
    (void)self;

    format_id = biosyn_guess_format_from_path(StringValueCStr(path));
    return UINT2NUM(format_id);
}

static VALUE str_or_nil(const char *s) { return s ? rb_str_new_cstr(s) : Qnil; }

/* Format and kind metadata are generated from libbiosyntax at load time, so the
 * Ruby layer does not need to maintain parallel tables.
 */
static VALUE native_formats_raw(VALUE self) {
    VALUE ary = rb_ary_new();
    uint32_t count = biosyn_format_count();
    uint32_t i;
    (void)self;

    for (i = 0; i < count; i++) {
        biosyn_format_info_t info;
        if (biosyn_format_info((biosyn_format_t)i, &info)) {
            VALUE h = rb_hash_new();
            rb_hash_aset(h, ID2SYM(rb_intern("id")), UINT2NUM(i));
            rb_hash_aset(h, ID2SYM(rb_intern("name")), str_or_nil(info.name));
            rb_hash_aset(h, ID2SYM(rb_intern("description")), str_or_nil(info.description));
            rb_hash_aset(h, ID2SYM(rb_intern("stateful")), info.stateful ? Qtrue : Qfalse);
            rb_ary_push(ary, rb_obj_freeze(h));
        }
    }

    return rb_obj_freeze(ary);
}

/* Return raw token kind metadata for Ruby-side value objects. */
static VALUE native_kinds_raw(VALUE self) {
    VALUE ary = rb_ary_new();
    uint32_t count = biosyn_class_count();
    uint32_t i;
    (void)self;

    for (i = 0; i < count; i++) {
        biosyn_class_info_t info;
        if (biosyn_class_info((biosyn_class_t)i, &info)) {
            VALUE h = rb_hash_new();
            rb_hash_aset(h, ID2SYM(rb_intern("id")), UINT2NUM(i));
            rb_hash_aset(h, ID2SYM(rb_intern("name")), str_or_nil(info.name));
            rb_hash_aset(h, ID2SYM(rb_intern("scope")), str_or_nil(info.scope));
            rb_hash_aset(h, ID2SYM(rb_intern("foreground")), str_or_nil(info.foreground));
            rb_hash_aset(h, ID2SYM(rb_intern("background")), str_or_nil(info.background));
            rb_hash_aset(h, ID2SYM(rb_intern("font_style")), str_or_nil(info.font_style));
            rb_hash_aset(h, ID2SYM(rb_intern("ansi_sgr")), str_or_nil(info.ansi_sgr));
            rb_ary_push(ary, rb_obj_freeze(h));
        }
    }

    return rb_obj_freeze(ary);
}

void Init_biosyntax_ext(void) {
    mBioSyntax = rb_define_module("BioSyntax");
    cSpan = Qnil;

    /* BioSyntax::Native is intentionally an internal bridge. Public Ruby APIs
     * are defined in lib/biosyntax.rb.
     */
    mNative = rb_define_module_under(mBioSyntax, "Native");
    rb_define_singleton_method(mNative, "libbiosyntax_version", native_libbiosyntax_version, 0);
    rb_define_singleton_method(mNative, "abi_version", native_abi_version, 0);
    rb_define_singleton_method(mNative, "formats_raw", native_formats_raw, 0);
    rb_define_singleton_method(mNative, "kinds_raw", native_kinds_raw, 0);
    rb_define_singleton_method(mNative, "format_id_from_name", native_format_id_from_name, 1);
    rb_define_singleton_method(mNative, "guess_format_id", native_guess_format_id, 1);

    cNativeState = rb_define_class_under(mNative, "State", rb_cObject);
    rb_define_alloc_func(cNativeState, state_alloc);
    rb_define_method(cNativeState, "initialize", state_initialize, 1);
    rb_define_method(cNativeState, "reset", state_reset, -1);
    rb_define_method(cNativeState, "line_no", state_line_no, 0);
    rb_define_method(cNativeState, "format_id", state_format_id, 0);
    rb_define_method(cNativeState, "highlight", state_highlight, 1);
    rb_define_method(cNativeState, "colorize", state_colorize, 1);
}
