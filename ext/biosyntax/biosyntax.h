#ifndef BIOSYNTAX_H
#define BIOSYNTAX_H

/*
 * libbiosyntax: dependency-free C tokenizer/highlighter core for biological files.
 * The core performs no IO; callers pass one already-read text line at a time.
 * SPDX-License-Identifier: GPL-3.0-only
 */

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(BIOSYN_STATIC)
#  define BIOSYN_API
#elif defined(_WIN32) || defined(__CYGWIN__)
#  if defined(BIOSYN_BUILDING_LIBRARY)
#    define BIOSYN_API __declspec(dllexport)
#  else
#    define BIOSYN_API __declspec(dllimport)
#  endif
#else
#  if defined(BIOSYN_BUILDING_LIBRARY) && defined(__GNUC__)
#    define BIOSYN_API __attribute__((visibility("default")))
#  else
#    define BIOSYN_API
#  endif
#endif

#define BIOSYN_VERSION_MAJOR 0
#define BIOSYN_VERSION_MINOR 1
#define BIOSYN_VERSION_PATCH 0
#define BIOSYN_ABI_VERSION   1u

#define BIOSYN_STRINGIFY_DETAIL(x) #x
#define BIOSYN_STRINGIFY(x) BIOSYN_STRINGIFY_DETAIL(x)
#define BIOSYN_VERSION_STRING \
    BIOSYN_STRINGIFY(BIOSYN_VERSION_MAJOR) "." \
    BIOSYN_STRINGIFY(BIOSYN_VERSION_MINOR) "." \
    BIOSYN_STRINGIFY(BIOSYN_VERSION_PATCH)

typedef uint32_t biosyn_format_t;
typedef uint32_t biosyn_class_t;

enum {
    BIOSYN_FORMAT_UNKNOWN = 0,
    BIOSYN_FORMAT_FASTA = 1,
    BIOSYN_FORMAT_FASTQ = 2,
    BIOSYN_FORMAT_SAM = 3,
    BIOSYN_FORMAT_VCF = 4,
    BIOSYN_FORMAT_BED = 5,
    BIOSYN_FORMAT_GTF = 6,
    BIOSYN_FORMAT_GFF = 7,
    BIOSYN_FORMAT_PDB = 8,
    BIOSYN_FORMAT_CLUSTAL = 9,
    BIOSYN_FORMAT_FAIDX = 10,
    BIOSYN_FORMAT_FLAGSTAT = 11,
    BIOSYN_FORMAT_WIG = 12,
    BIOSYN_FORMAT_FASTA_NT = 13,
    BIOSYN_FORMAT_FASTA_HC = 14,
    BIOSYN_FORMAT_FASTA_CLUSTAL = 15,
    BIOSYN_FORMAT_FASTA_HYDRO = 16,
    BIOSYN_FORMAT_FASTA_TAYLOR = 17,
    BIOSYN_FORMAT_FASTA_ZAPPO = 18,
    BIOSYN_FORMAT_FASTA_ORF = 19,
    BIOSYN_FORMAT__COUNT
};

enum {
    BIOSYN_CLASS_PLAIN = 0,
    BIOSYN_CLASS_HEADER = 1,
    BIOSYN_CLASS_COMMENT = 2,
    BIOSYN_CLASS_CHROM = 3,
    BIOSYN_CLASS_POSITION = 4,
    BIOSYN_CLASS_NAME = 5,
    BIOSYN_CLASS_SAMPLE = 6,
    BIOSYN_CLASS_SOFTWARE = 7,
    BIOSYN_CLASS_COMMANDLINE = 8,
    BIOSYN_CLASS_STRING = 9,
    BIOSYN_CLASS_QUOTED_STRING = 10,
    BIOSYN_CLASS_NUMBER = 11,
    BIOSYN_CLASS_NUMBER_ALT = 12,
    BIOSYN_CLASS_URL = 13,
    BIOSYN_CLASS_GOOD = 14,
    BIOSYN_CLASS_BAD = 15,
    BIOSYN_CLASS_KEYWORD = 16,
    BIOSYN_CLASS_KEYWORD2 = 17,
    BIOSYN_CLASS_KEYWORD3 = 18,
    BIOSYN_CLASS_KEYWORD4 = 19,
    BIOSYN_CLASS_KEYWORD5 = 20,
    BIOSYN_CLASS_KEYWORD6 = 21,
    BIOSYN_CLASS_STRAND_PLUS = 22,
    BIOSYN_CLASS_STRAND_MINUS = 23,
    BIOSYN_CLASS_STRAND_NONE = 24,
    BIOSYN_CLASS_FEATURE_GENE = 25,
    BIOSYN_CLASS_FEATURE_TRANSCRIPT = 26,
    BIOSYN_CLASS_FEATURE_EXON = 27,
    BIOSYN_CLASS_FEATURE_CDS = 28,
    BIOSYN_CLASS_FEATURE_START_CODON = 29,
    BIOSYN_CLASS_FEATURE_STOP_CODON = 30,
    BIOSYN_CLASS_FEATURE_UTR = 31,
    BIOSYN_CLASS_FEATURE_INTER = 32,
    BIOSYN_CLASS_FEATURE_INTRON_CNS = 33,
    BIOSYN_CLASS_SEQUENCE_LENGTH = 34,
    BIOSYN_CLASS_FILE_OFFSET = 35,
    BIOSYN_CLASS_LINE_BASES = 36,
    BIOSYN_CLASS_LINE_WIDTH = 37,
    BIOSYN_CLASS_QC_PASSED = 38,
    BIOSYN_CLASS_QC_FAILED = 39,
    BIOSYN_CLASS_PERCENT = 40,
    BIOSYN_CLASS_NT_A = 41,
    BIOSYN_CLASS_NT_C = 42,
    BIOSYN_CLASS_NT_G = 43,
    BIOSYN_CLASS_NT_T = 44,
    BIOSYN_CLASS_NT_U = 45,
    BIOSYN_CLASS_NT_N = 46,
    BIOSYN_CLASS_NT_R = 47,
    BIOSYN_CLASS_NT_Y = 48,
    BIOSYN_CLASS_NT_S = 49,
    BIOSYN_CLASS_NT_W = 50,
    BIOSYN_CLASS_NT_M = 51,
    BIOSYN_CLASS_NT_K = 52,
    BIOSYN_CLASS_NT_D = 53,
    BIOSYN_CLASS_NT_B = 54,
    BIOSYN_CLASS_NT_V = 55,
    BIOSYN_CLASS_NT_H = 56,
    BIOSYN_CLASS_NT_X = 57,
    BIOSYN_CLASS_AMINO_HYDRO = 58,
    BIOSYN_CLASS_AMINO_POS = 59,
    BIOSYN_CLASS_AMINO_NEG = 60,
    BIOSYN_CLASS_AMINO_POLAR = 61,
    BIOSYN_CLASS_AMINO_CYS = 62,
    BIOSYN_CLASS_AMINO_GLY = 63,
    BIOSYN_CLASS_AMINO_PRO = 64,
    BIOSYN_CLASS_AMINO_ARO = 65,
    BIOSYN_CLASS_AA_A = 66,
    BIOSYN_CLASS_AA_R = 67,
    BIOSYN_CLASS_AA_N = 68,
    BIOSYN_CLASS_AA_D = 69,
    BIOSYN_CLASS_AA_C = 70,
    BIOSYN_CLASS_AA_Q = 71,
    BIOSYN_CLASS_AA_E = 72,
    BIOSYN_CLASS_AA_G = 73,
    BIOSYN_CLASS_AA_H = 74,
    BIOSYN_CLASS_AA_I = 75,
    BIOSYN_CLASS_AA_L = 76,
    BIOSYN_CLASS_AA_K = 77,
    BIOSYN_CLASS_AA_M = 78,
    BIOSYN_CLASS_AA_F = 79,
    BIOSYN_CLASS_AA_P = 80,
    BIOSYN_CLASS_AA_S = 81,
    BIOSYN_CLASS_AA_T = 82,
    BIOSYN_CLASS_AA_W = 83,
    BIOSYN_CLASS_AA_Y = 84,
    BIOSYN_CLASS_AA_V = 85,
    BIOSYN_CLASS_AA_B = 86,
    BIOSYN_CLASS_AA_X = 87,
    BIOSYN_CLASS_AA_Z = 88,
    BIOSYN_CLASS_ZAPPO_A = 89,
    BIOSYN_CLASS_ZAPPO_R = 90,
    BIOSYN_CLASS_ZAPPO_N = 91,
    BIOSYN_CLASS_ZAPPO_D = 92,
    BIOSYN_CLASS_ZAPPO_C = 93,
    BIOSYN_CLASS_ZAPPO_Q = 94,
    BIOSYN_CLASS_ZAPPO_E = 95,
    BIOSYN_CLASS_ZAPPO_G = 96,
    BIOSYN_CLASS_ZAPPO_H = 97,
    BIOSYN_CLASS_ZAPPO_I = 98,
    BIOSYN_CLASS_ZAPPO_L = 99,
    BIOSYN_CLASS_ZAPPO_K = 100,
    BIOSYN_CLASS_ZAPPO_M = 101,
    BIOSYN_CLASS_ZAPPO_F = 102,
    BIOSYN_CLASS_ZAPPO_P = 103,
    BIOSYN_CLASS_ZAPPO_S = 104,
    BIOSYN_CLASS_ZAPPO_T = 105,
    BIOSYN_CLASS_ZAPPO_W = 106,
    BIOSYN_CLASS_ZAPPO_Y = 107,
    BIOSYN_CLASS_ZAPPO_V = 108,
    BIOSYN_CLASS_ZAPPO_B = 109,
    BIOSYN_CLASS_ZAPPO_X = 110,
    BIOSYN_CLASS_ZAPPO_Z = 111,
    BIOSYN_CLASS_TAYLOR_A = 112,
    BIOSYN_CLASS_TAYLOR_R = 113,
    BIOSYN_CLASS_TAYLOR_N = 114,
    BIOSYN_CLASS_TAYLOR_D = 115,
    BIOSYN_CLASS_TAYLOR_C = 116,
    BIOSYN_CLASS_TAYLOR_Q = 117,
    BIOSYN_CLASS_TAYLOR_E = 118,
    BIOSYN_CLASS_TAYLOR_G = 119,
    BIOSYN_CLASS_TAYLOR_H = 120,
    BIOSYN_CLASS_TAYLOR_I = 121,
    BIOSYN_CLASS_TAYLOR_L = 122,
    BIOSYN_CLASS_TAYLOR_K = 123,
    BIOSYN_CLASS_TAYLOR_M = 124,
    BIOSYN_CLASS_TAYLOR_F = 125,
    BIOSYN_CLASS_TAYLOR_P = 126,
    BIOSYN_CLASS_TAYLOR_S = 127,
    BIOSYN_CLASS_TAYLOR_T = 128,
    BIOSYN_CLASS_TAYLOR_W = 129,
    BIOSYN_CLASS_TAYLOR_Y = 130,
    BIOSYN_CLASS_TAYLOR_V = 131,
    BIOSYN_CLASS_TAYLOR_B = 132,
    BIOSYN_CLASS_TAYLOR_X = 133,
    BIOSYN_CLASS_TAYLOR_Z = 134,
    BIOSYN_CLASS_HYDRO_A = 135,
    BIOSYN_CLASS_HYDRO_R = 136,
    BIOSYN_CLASS_HYDRO_N = 137,
    BIOSYN_CLASS_HYDRO_D = 138,
    BIOSYN_CLASS_HYDRO_C = 139,
    BIOSYN_CLASS_HYDRO_Q = 140,
    BIOSYN_CLASS_HYDRO_E = 141,
    BIOSYN_CLASS_HYDRO_G = 142,
    BIOSYN_CLASS_HYDRO_H = 143,
    BIOSYN_CLASS_HYDRO_I = 144,
    BIOSYN_CLASS_HYDRO_L = 145,
    BIOSYN_CLASS_HYDRO_K = 146,
    BIOSYN_CLASS_HYDRO_M = 147,
    BIOSYN_CLASS_HYDRO_F = 148,
    BIOSYN_CLASS_HYDRO_P = 149,
    BIOSYN_CLASS_HYDRO_S = 150,
    BIOSYN_CLASS_HYDRO_T = 151,
    BIOSYN_CLASS_HYDRO_W = 152,
    BIOSYN_CLASS_HYDRO_Y = 153,
    BIOSYN_CLASS_HYDRO_V = 154,
    BIOSYN_CLASS_HYDRO_B = 155,
    BIOSYN_CLASS_HYDRO_X = 156,
    BIOSYN_CLASS_HYDRO_Z = 157,
    BIOSYN_CLASS_HC_A = 158,
    BIOSYN_CLASS_HC_T = 159,
    BIOSYN_CLASS_HC_G = 160,
    BIOSYN_CLASS_HC_C = 161,
    BIOSYN_CLASS_HC_U = 162,
    BIOSYN_CLASS_HC_R = 163,
    BIOSYN_CLASS_HC_Y = 164,
    BIOSYN_CLASS_HC_S = 165,
    BIOSYN_CLASS_HC_W = 166,
    BIOSYN_CLASS_HC_M = 167,
    BIOSYN_CLASS_HC_K = 168,
    BIOSYN_CLASS_HC_D = 169,
    BIOSYN_CLASS_HC_B = 170,
    BIOSYN_CLASS_HC_V = 171,
    BIOSYN_CLASS_HC_H = 172,
    BIOSYN_CLASS_HC_N = 173,
    BIOSYN_CLASS_HC_X = 174,
    BIOSYN_CLASS_HC_GAP = 175,
    BIOSYN_CLASS_CIGAR_MATCH = 176,
    BIOSYN_CLASS_CIGAR_MISMATCH = 177,
    BIOSYN_CLASS_CIGAR_CLIP = 178,
    BIOSYN_CLASS_CIGAR_INSERTION = 179,
    BIOSYN_CLASS_CIGAR_DELETION = 180,
    BIOSYN_CLASS_QUAL_1 = 181,
    BIOSYN_CLASS_QUAL_3 = 182,
    BIOSYN_CLASS_QUAL_6 = 183,
    BIOSYN_CLASS_QUAL_8 = 184,
    BIOSYN_CLASS_QUAL_10 = 185,
    BIOSYN_CLASS_QUAL_10B = 186,
    BIOSYN_CLASS_QUAL_10I = 187,
    BIOSYN_CLASS_GRAD_0 = 188,
    BIOSYN_CLASS_GRAD_1 = 189,
    BIOSYN_CLASS_GRAD_2 = 190,
    BIOSYN_CLASS_GRAD_3 = 191,
    BIOSYN_CLASS_GRAD_4 = 192,
    BIOSYN_CLASS_GRAD_5 = 193,
    BIOSYN_CLASS_GRAD_6 = 194,
    BIOSYN_CLASS_GRAD_7 = 195,
    BIOSYN_CLASS_GRAD_8 = 196,
    BIOSYN_CLASS_GRAD_9 = 197,
    BIOSYN_CLASS_GRAD_10 = 198,
    BIOSYN_CLASS_GRADBW_0 = 199,
    BIOSYN_CLASS_GRADBW_1 = 200,
    BIOSYN_CLASS_GRADBW_2 = 201,
    BIOSYN_CLASS_GRADBW_3 = 202,
    BIOSYN_CLASS_GRADBW_4 = 203,
    BIOSYN_CLASS_GRADBW_5 = 204,
    BIOSYN_CLASS_GRADBW_6 = 205,
    BIOSYN_CLASS_GRADBW_7 = 206,
    BIOSYN_CLASS_GRADBW_8 = 207,
    BIOSYN_CLASS_GRADBW_9 = 208,
    BIOSYN_CLASS_GRADBW_10 = 209,
    BIOSYN_CLASS_GAP = 210,
    BIOSYN_CLASS_NULL = 211,
    BIOSYN_CLASS_ERROR = 212,
    BIOSYN_CLASS_ORF_START = 213,
    BIOSYN_CLASS_ORF_CODING = 214,
    BIOSYN_CLASS_ORF_STOP = 215,
    BIOSYN_CLASS__COUNT
};

typedef struct biosyn_span {
    uint64_t start;
    uint64_t length;
    biosyn_class_t class_id;
    uint32_t reserved;
} biosyn_span_t;

typedef struct biosyn_state {
    uint32_t abi_version;
    biosyn_format_t format;
    uint64_t line_no;
    uint32_t fastq_phase;
    uint32_t wig_mode;
    uint32_t reserved[7];
} biosyn_state_t;

typedef struct biosyn_class_info {
    const char *name;
    const char *scope;
    const char *foreground;
    const char *background;
    const char *font_style;
    const char *ansi_sgr;
} biosyn_class_info_t;

typedef struct biosyn_format_info {
    const char *name;
    const char *description;
    uint32_t stateful;
    uint32_t reserved;
} biosyn_format_info_t;

typedef struct biosyn_ansi_style {
    biosyn_class_t class_id;
    uint32_t reserved;
    const char *ansi_sgr;
} biosyn_ansi_style_t;

BIOSYN_API uint32_t biosyn_abi_version(void);
BIOSYN_API const char *biosyn_version(void);

BIOSYN_API biosyn_format_t biosyn_format_from_name(const char *name);
BIOSYN_API biosyn_format_t biosyn_guess_format_from_path(const char *path_or_extension);
BIOSYN_API const char *biosyn_format_name(biosyn_format_t format);
BIOSYN_API uint32_t biosyn_format_count(void);
BIOSYN_API int biosyn_format_info(biosyn_format_t format, biosyn_format_info_t *out);

BIOSYN_API const char *biosyn_class_name(biosyn_class_t class_id);
BIOSYN_API const char *biosyn_class_scope(biosyn_class_t class_id);
BIOSYN_API const char *biosyn_class_ansi_sgr(biosyn_class_t class_id);
BIOSYN_API const char *biosyn_class_default_foreground(biosyn_class_t class_id);
BIOSYN_API const char *biosyn_class_default_background(biosyn_class_t class_id);
BIOSYN_API const char *biosyn_class_default_font_style(biosyn_class_t class_id);
BIOSYN_API uint32_t biosyn_class_count(void);
BIOSYN_API int biosyn_class_info(biosyn_class_t class_id, biosyn_class_info_t *out);

BIOSYN_API void biosyn_state_init(biosyn_state_t *state, biosyn_format_t format);
BIOSYN_API biosyn_state_t *biosyn_state_new(biosyn_format_t format);
BIOSYN_API void biosyn_state_free(biosyn_state_t *state);

/*
 * Returns the number of spans required. If out_cap is smaller than the return
 * value, only the first out_cap spans are written. The line need not be NUL
 * terminated. Newline/CRLF terminators are ignored for tokenization.
 *
 * Format IDs describe text syntax families. Compressed or binary containers
 * such as BAM, CRAM, and BCF must be decoded by the caller before highlighting.
 */
BIOSYN_API uint64_t biosyn_highlight_line(
    biosyn_format_t format,
    const char *line,
    uint64_t len,
    uint64_t zero_based_line_no,
    biosyn_span_t *out,
    uint64_t out_cap
);

/*
 * Stateful API. Prefer this for FASTQ and WIG streams. If out_cap is too small,
 * the required span count is returned and state is left unchanged so callers can
 * retry with a larger buffer. State advances only when the returned count is
 * less than or equal to out_cap.
 */
BIOSYN_API uint64_t biosyn_highlight_next_line(
    biosyn_state_t *state,
    const char *line,
    uint64_t len,
    biosyn_span_t *out,
    uint64_t out_cap
);

/* ANSI renderer for examples/CLI. Returns bytes required, excluding NUL. */
BIOSYN_API uint64_t biosyn_render_ansi_line(
    const char *line,
    uint64_t len,
    const biosyn_span_t *spans,
    uint64_t span_count,
    char *out,
    uint64_t out_cap
);

/*
 * ANSI renderer with optional style overrides. Each style entry maps class_id
 * to an ANSI SGR fragment such as "38;2;255;0;0" or "1;31". Unknown classes
 * and missing overrides use the built-in default style.
 */
BIOSYN_API uint64_t biosyn_render_ansi_line_with_styles(
    const char *line,
    uint64_t len,
    const biosyn_span_t *spans,
    uint64_t span_count,
    const biosyn_ansi_style_t *styles,
    uint64_t style_count,
    char *out,
    uint64_t out_cap
);

#ifdef __cplusplus
}
#endif

#endif /* BIOSYNTAX_H */
