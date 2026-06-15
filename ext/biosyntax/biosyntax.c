
/*
 * libbiosyntax: dependency-free C tokenizer/highlighter core for biological files.
 * SPDX-License-Identifier: GPL-3.0-only
 */
#define BIOSYN_BUILDING_LIBRARY
#include "biosyntax.h"

#include <ctype.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct writer { biosyn_span_t *out; size_t cap; size_t count; uint32_t last_cls; uint64_t last_end; int has_last; } writer_t;
typedef struct field { size_t start; size_t len; } field_t;
typedef struct class_record { const char *name; const char *scope; const char *fg; const char *bg; const char *font; const char *ansi; } class_record_t;
typedef struct format_record { const char *name; const char *description; uint32_t stateful; } format_record_t;

static const format_record_t g_format_info[] = {
    {"unknown", "unknown format", 0},
    {"fasta", "FASTA sequence", 0},
    {"fastq", "FASTQ reads", 1},
    {"sam", "SAM/BAM/CRAM alignment", 0},
    {"vcf", "VCF/BCF variants", 0},
    {"bed", "BED intervals", 0},
    {"gtf", "GTF annotations", 0},
    {"gff", "GFF annotations", 0},
    {"pdb", "PDB structure", 0},
    {"clustal", "CLUSTAL alignment", 0},
    {"faidx", "FASTA index", 0},
    {"flagstat", "samtools flagstat summary", 0},
    {"wig", "WIG/bedGraph signal", 1},
    {"fasta-nt", "FASTA nucleotide sequence", 0},
    {"fasta-hc", "FASTA high-contrast nucleotide sequence", 0},
    {"fasta-clustal", "FASTA amino-acid sequence using CLUSTAL colors", 0},
    {"fasta-hydro", "FASTA amino-acid sequence using hydrophobicity colors", 0},
    {"fasta-taylor", "FASTA amino-acid sequence using Taylor colors", 0},
    {"fasta-zappo", "FASTA amino-acid sequence using Zappo colors", 0},
    {"fasta-orf", "FASTA open reading frame view", 0},
};
typedef char format_info_count_must_match[(sizeof(g_format_info) / sizeof(g_format_info[0]) == BIOSYN_FORMAT__COUNT) ? 1 : -1];

static const class_record_t g_class_info[] = {
    {"plain", "biosyntax.plain", "", "", "", "0"},
    {"header", "biosyntax.header", "#E6DB74", "#3E3D32", "bold", "01;38;2;230;219;116;48;2;62;61;50"},
    {"comment", "biosyntax.comment", "#B1B1AF", "", "italic", "03;38;2;177;177;175"},
    {"chrom", "biosyntax.chrom", "#1E8449", "", "bold", "01;38;2;30;132;73"},
    {"position", "biosyntax.position", "#1E8449", "", "", "38;2;30;132;73"},
    {"name", "biosyntax.name", "#CE9178", "", "", "38;2;206;145;120"},
    {"sample", "biosyntax.sample", "#66D9EF", "", "", "38;2;102;217;239"},
    {"software", "biosyntax.software", "#FD971F", "", "italic", "03;38;2;253;151;31"},
    {"commandline", "biosyntax.commandline", "#FD971F", "", "", "38;2;253;151;31"},
    {"string", "biosyntax.string", "#FF4641", "", "", "38;2;255;70;65"},
    {"quoted_string", "biosyntax.quoted.string", "#FF4641", "", "italic", "03;38;2;255;70;65"},
    {"number", "biosyntax.number", "#0087AF", "", "", "38;2;0;135;175"},
    {"number_alt", "biosyntax.number.alt", "#AE81FF", "", "", "38;2;174;129;255"},
    {"url", "biosyntax.url", "#66D9EF", "", "underline", "04;38;2;102;217;239"},
    {"good", "biosyntax.good", "#4192FF", "#000000", "", "38;2;65;146;255;48;2;0;0;0"},
    {"bad", "biosyntax.bad", "#FF4641", "#000000", "", "38;2;255;70;65;48;2;0;0;0"},
    {"keyword", "biosyntax.keyword", "#F92672", "", "", "38;2;249;38;114"},
    {"keyword2", "biosyntax.keyword2", "#A6E22E", "", "", "38;2;166;226;46"},
    {"keyword3", "biosyntax.keyword3", "#66D9EF", "", "", "38;2;102;217;239"},
    {"keyword4", "biosyntax.keyword4", "#FD971F", "", "", "38;2;253;151;31"},
    {"keyword5", "biosyntax.keyword5", "#AE81FF", "", "", "38;2;174;129;255"},
    {"keyword6", "biosyntax.keyword6", "#E6DB74", "", "", "38;2;230;219;116"},
    {"strand_plus", "biosyntax.strand.plus", "#4192FF", "#000000", "", "38;2;65;146;255;48;2;0;0;0"},
    {"strand_minus", "biosyntax.strand.minus", "#F09000", "#000000", "", "38;2;240;144;0;48;2;0;0;0"},
    {"strand_none", "biosyntax.strand.none", "#B1B1AF", "", "", "38;2;177;177;175"},
    {"feature_gene", "biosyntax.feature.gene", "#1E8449", "", "bold", "01;38;2;30;132;73"},
    {"feature_transcript", "biosyntax.feature.transcript", "#4192FF", "", "", "38;2;65;146;255"},
    {"feature_exon", "biosyntax.feature.exon", "#AE81FF", "", "", "38;2;174;129;255"},
    {"feature_cds", "biosyntax.feature.cds", "#A6E22E", "#000000", "", "38;2;166;226;46;48;2;0;0;0"},
    {"feature_start_codon", "biosyntax.feature.start.codon", "#A6E22E", "#000000", "", "38;2;166;226;46;48;2;0;0;0"},
    {"feature_stop_codon", "biosyntax.feature.stop.codon", "#FF4641", "#000000", "", "38;2;255;70;65;48;2;0;0;0"},
    {"feature_utr", "biosyntax.feature.utr", "#B1B1AF", "#FFFFFF", "", "38;2;177;177;175;48;2;255;255;255"},
    {"feature_inter", "biosyntax.feature.inter", "#B1B1AF", "", "italic", "03;38;2;177;177;175"},
    {"feature_intron_cns", "biosyntax.feature.intron.cns", "#B1B1AF", "", "italic", "03;38;2;177;177;175"},
    {"sequence_length", "biosyntax.sequence.length", "#1E8449", "", "", "38;2;30;132;73"},
    {"file_offset", "biosyntax.file.offset", "#0087AF", "", "", "38;2;0;135;175"},
    {"line_bases", "biosyntax.line.bases", "#B1B1AF", "", "italic", "03;38;2;177;177;175"},
    {"line_width", "biosyntax.line.width", "#B1B1AF", "", "italic", "03;38;2;177;177;175"},
    {"qc_passed", "biosyntax.qc.passed", "#0087AF", "", "", "38;2;0;135;175"},
    {"qc_failed", "biosyntax.qc.failed", "#F92672", "", "", "38;2;249;38;114"},
    {"percent", "biosyntax.percent", "#AE81FF", "", "", "38;2;174;129;255"},
    {"nt_a", "biosyntax.nt.a", "#47FF19", "#000000", "", "38;2;71;255;25;48;2;0;0;0"},
    {"nt_c", "biosyntax.nt.c", "#FF4641", "#000000", "", "38;2;255;70;65;48;2;0;0;0"},
    {"nt_g", "biosyntax.nt.g", "#F09000", "#000000", "", "38;2;240;144;0;48;2;0;0;0"},
    {"nt_t", "biosyntax.nt.t", "#4192FF", "#000000", "", "38;2;65;146;255;48;2;0;0;0"},
    {"nt_u", "biosyntax.nt.u", "#8A89FF", "#000000", "", "38;2;138;137;255;48;2;0;0;0"},
    {"nt_n", "biosyntax.nt.n", "#FFFFFF", "#000000", "", "38;2;255;255;255;48;2;0;0;0"},
    {"nt_r", "biosyntax.nt.r", "#FFFE80", "#000000", "", "38;2;255;254;128;48;2;0;0;0"},
    {"nt_y", "biosyntax.nt.y", "#E180FF", "#000000", "", "38;2;225;128;255;48;2;0;0;0"},
    {"nt_s", "biosyntax.nt.s", "#FF9B80", "#000000", "", "38;2;255;155;128;48;2;0;0;0"},
    {"nt_w", "biosyntax.nt.w", "#80FFF2", "#000000", "", "38;2;128;255;242;48;2;0;0;0"},
    {"nt_m", "biosyntax.nt.m", "#CE8834", "#000000", "", "38;2;206;136;52;48;2;0;0;0"},
    {"nt_k", "biosyntax.nt.k", "#90B82C", "#000000", "", "38;2;144;184;44;48;2;0;0;0"},
    {"nt_d", "biosyntax.nt.d", "#C7FFB9", "#000000", "", "38;2;199;255;185;48;2;0;0;0"},
    {"nt_b", "biosyntax.nt.b", "#F8C1C0", "#000000", "", "38;2;248;193;192;48;2;0;0;0"},
    {"nt_v", "biosyntax.nt.v", "#FFE3B9", "#000000", "", "38;2;255;227;185;48;2;0;0;0"},
    {"nt_h", "biosyntax.nt.h", "#BFD8F9", "#000000", "", "38;2;191;216;249;48;2;0;0;0"},
    {"nt_x", "biosyntax.nt.x", "#E6E6E6", "", "", "38;2;230;230;230"},
    {"amino_hydro", "biosyntax.amino.hydro", "#80A0F0", "#000000", "", "38;2;128;160;240;48;2;0;0;0"},
    {"amino_pos", "biosyntax.amino.pos", "#F01505", "#000000", "", "38;2;240;21;5;48;2;0;0;0"},
    {"amino_neg", "biosyntax.amino.neg", "#C048C0", "#000000", "", "38;2;192;72;192;48;2;0;0;0"},
    {"amino_polar", "biosyntax.amino.polar", "#00FF00", "#000000", "", "38;2;0;255;0;48;2;0;0;0"},
    {"amino_cys", "biosyntax.amino.cys", "#F08080", "#000000", "", "38;2;240;128;128;48;2;0;0;0"},
    {"amino_gly", "biosyntax.amino.gly", "#F09048", "#000000", "", "38;2;240;144;72;48;2;0;0;0"},
    {"amino_pro", "biosyntax.amino.pro", "#FFFF00", "#000000", "", "38;2;255;255;0;48;2;0;0;0"},
    {"amino_aro", "biosyntax.amino.aro", "#15A4A4", "#000000", "", "38;2;21;164;164;48;2;0;0;0"},
    {"aa_a", "biosyntax.aa.a", "#80A0F0", "#000000", "", "38;2;128;160;240;48;2;0;0;0"},
    {"aa_r", "biosyntax.aa.r", "#F01505", "#000000", "", "38;2;240;21;5;48;2;0;0;0"},
    {"aa_n", "biosyntax.aa.n", "#00FF00", "#000000", "", "38;2;0;255;0;48;2;0;0;0"},
    {"aa_d", "biosyntax.aa.d", "#C048C0", "#000000", "", "38;2;192;72;192;48;2;0;0;0"},
    {"aa_c", "biosyntax.aa.c", "#F08080", "#000000", "", "38;2;240;128;128;48;2;0;0;0"},
    {"aa_q", "biosyntax.aa.q", "#00FF00", "#000000", "", "38;2;0;255;0;48;2;0;0;0"},
    {"aa_e", "biosyntax.aa.e", "#C048C0", "#000000", "", "38;2;192;72;192;48;2;0;0;0"},
    {"aa_g", "biosyntax.aa.g", "#F09048", "#000000", "", "38;2;240;144;72;48;2;0;0;0"},
    {"aa_h", "biosyntax.aa.h", "#15A4A4", "#000000", "", "38;2;21;164;164;48;2;0;0;0"},
    {"aa_i", "biosyntax.aa.i", "#80A0F0", "#000000", "", "38;2;128;160;240;48;2;0;0;0"},
    {"aa_l", "biosyntax.aa.l", "#80A0F0", "#000000", "", "38;2;128;160;240;48;2;0;0;0"},
    {"aa_k", "biosyntax.aa.k", "#F01505", "#000000", "", "38;2;240;21;5;48;2;0;0;0"},
    {"aa_m", "biosyntax.aa.m", "#80A0F0", "#000000", "", "38;2;128;160;240;48;2;0;0;0"},
    {"aa_f", "biosyntax.aa.f", "#80A0F0", "#000000", "", "38;2;128;160;240;48;2;0;0;0"},
    {"aa_p", "biosyntax.aa.p", "#FFFF00", "#000000", "", "38;2;255;255;0;48;2;0;0;0"},
    {"aa_s", "biosyntax.aa.s", "#00FF00", "#000000", "", "38;2;0;255;0;48;2;0;0;0"},
    {"aa_t", "biosyntax.aa.t", "#00FF00", "#000000", "", "38;2;0;255;0;48;2;0;0;0"},
    {"aa_w", "biosyntax.aa.w", "#80A0F0", "#000000", "", "38;2;128;160;240;48;2;0;0;0"},
    {"aa_y", "biosyntax.aa.y", "#15A4A4", "#000000", "", "38;2;21;164;164;48;2;0;0;0"},
    {"aa_v", "biosyntax.aa.v", "#80A0F0", "#000000", "", "38;2;128;160;240;48;2;0;0;0"},
    {"aa_b", "biosyntax.aa.b", "#FFFFFF", "#000000", "", "38;2;255;255;255;48;2;0;0;0"},
    {"aa_x", "biosyntax.aa.x", "#FFFFFF", "#000000", "", "38;2;255;255;255;48;2;0;0;0"},
    {"aa_z", "biosyntax.aa.z", "#FFFFFF", "#000000", "", "38;2;255;255;255;48;2;0;0;0"},
    {"zappo_a", "biosyntax.zappo.a", "#FFAFAF", "#000000", "", "38;2;255;175;175;48;2;0;0;0"},
    {"zappo_r", "biosyntax.zappo.r", "#6464FF", "#000000", "", "38;2;100;100;255;48;2;0;0;0"},
    {"zappo_n", "biosyntax.zappo.n", "#00FF00", "#000000", "", "38;2;0;255;0;48;2;0;0;0"},
    {"zappo_d", "biosyntax.zappo.d", "#FF0000", "#000000", "", "38;2;255;0;0;48;2;0;0;0"},
    {"zappo_c", "biosyntax.zappo.c", "#FFFF00", "#000000", "", "38;2;255;255;0;48;2;0;0;0"},
    {"zappo_q", "biosyntax.zappo.q", "#00FF00", "#000000", "", "38;2;0;255;0;48;2;0;0;0"},
    {"zappo_e", "biosyntax.zappo.e", "#FF0000", "#000000", "", "38;2;255;0;0;48;2;0;0;0"},
    {"zappo_g", "biosyntax.zappo.g", "#FF00FF", "#000000", "", "38;2;255;0;255;48;2;0;0;0"},
    {"zappo_h", "biosyntax.zappo.h", "#6464FF", "#000000", "", "38;2;100;100;255;48;2;0;0;0"},
    {"zappo_i", "biosyntax.zappo.i", "#FFAFAF", "#000000", "", "38;2;255;175;175;48;2;0;0;0"},
    {"zappo_l", "biosyntax.zappo.l", "#FFAFAF", "#000000", "", "38;2;255;175;175;48;2;0;0;0"},
    {"zappo_k", "biosyntax.zappo.k", "#6464FF", "#000000", "", "38;2;100;100;255;48;2;0;0;0"},
    {"zappo_m", "biosyntax.zappo.m", "#FFAFAF", "#000000", "", "38;2;255;175;175;48;2;0;0;0"},
    {"zappo_f", "biosyntax.zappo.f", "#FFC800", "#000000", "", "38;2;255;200;0;48;2;0;0;0"},
    {"zappo_p", "biosyntax.zappo.p", "#FF00FF", "#000000", "", "38;2;255;0;255;48;2;0;0;0"},
    {"zappo_s", "biosyntax.zappo.s", "#00FF00", "#000000", "", "38;2;0;255;0;48;2;0;0;0"},
    {"zappo_t", "biosyntax.zappo.t", "#00FF00", "#000000", "", "38;2;0;255;0;48;2;0;0;0"},
    {"zappo_w", "biosyntax.zappo.w", "#FFC800", "#000000", "", "38;2;255;200;0;48;2;0;0;0"},
    {"zappo_y", "biosyntax.zappo.y", "#FFC800", "#000000", "", "38;2;255;200;0;48;2;0;0;0"},
    {"zappo_v", "biosyntax.zappo.v", "#FFAFAF", "#000000", "", "38;2;255;175;175;48;2;0;0;0"},
    {"zappo_b", "biosyntax.zappo.b", "#FFFFFF", "#000000", "", "38;2;255;255;255;48;2;0;0;0"},
    {"zappo_x", "biosyntax.zappo.x", "#FFFFFF", "#000000", "", "38;2;255;255;255;48;2;0;0;0"},
    {"zappo_z", "biosyntax.zappo.z", "#FFFFFF", "#000000", "", "38;2;255;255;255;48;2;0;0;0"},
    {"taylor_a", "biosyntax.taylor.a", "#CCFF00", "#000000", "", "38;2;204;255;0;48;2;0;0;0"},
    {"taylor_r", "biosyntax.taylor.r", "#0000FF", "#000000", "", "38;2;0;0;255;48;2;0;0;0"},
    {"taylor_n", "biosyntax.taylor.n", "#CC00FF", "#000000", "", "38;2;204;0;255;48;2;0;0;0"},
    {"taylor_d", "biosyntax.taylor.d", "#FF0000", "#000000", "", "38;2;255;0;0;48;2;0;0;0"},
    {"taylor_c", "biosyntax.taylor.c", "#FFFF00", "#000000", "", "38;2;255;255;0;48;2;0;0;0"},
    {"taylor_q", "biosyntax.taylor.q", "#FF00CC", "#000000", "", "38;2;255;0;204;48;2;0;0;0"},
    {"taylor_e", "biosyntax.taylor.e", "#FF0066", "#000000", "", "38;2;255;0;102;48;2;0;0;0"},
    {"taylor_g", "biosyntax.taylor.g", "#FF9900", "#000000", "", "38;2;255;153;0;48;2;0;0;0"},
    {"taylor_h", "biosyntax.taylor.h", "#0066FF", "#000000", "", "38;2;0;102;255;48;2;0;0;0"},
    {"taylor_i", "biosyntax.taylor.i", "#66FF00", "#000000", "", "38;2;102;255;0;48;2;0;0;0"},
    {"taylor_l", "biosyntax.taylor.l", "#33FF00", "#000000", "", "38;2;51;255;0;48;2;0;0;0"},
    {"taylor_k", "biosyntax.taylor.k", "#6600FF", "#000000", "", "38;2;102;0;255;48;2;0;0;0"},
    {"taylor_m", "biosyntax.taylor.m", "#00FF00", "#000000", "", "38;2;0;255;0;48;2;0;0;0"},
    {"taylor_f", "biosyntax.taylor.f", "#00FF66", "#000000", "", "38;2;0;255;102;48;2;0;0;0"},
    {"taylor_p", "biosyntax.taylor.p", "#FFCC00", "#000000", "", "38;2;255;204;0;48;2;0;0;0"},
    {"taylor_s", "biosyntax.taylor.s", "#FF3300", "#000000", "", "38;2;255;51;0;48;2;0;0;0"},
    {"taylor_t", "biosyntax.taylor.t", "#FF6600", "#000000", "", "38;2;255;102;0;48;2;0;0;0"},
    {"taylor_w", "biosyntax.taylor.w", "#00CCFF", "#000000", "", "38;2;0;204;255;48;2;0;0;0"},
    {"taylor_y", "biosyntax.taylor.y", "#000000", "#00FFCC", "", "38;2;0;0;0;48;2;0;255;204"},
    {"taylor_v", "biosyntax.taylor.v", "#99FF00", "#000000", "", "38;2;153;255;0;48;2;0;0;0"},
    {"taylor_b", "biosyntax.taylor.b", "#FFFFFF", "#000000", "", "38;2;255;255;255;48;2;0;0;0"},
    {"taylor_x", "biosyntax.taylor.x", "#FFFFFF", "#000000", "", "38;2;255;255;255;48;2;0;0;0"},
    {"taylor_z", "biosyntax.taylor.z", "#FFFFFF", "#000000", "", "38;2;255;255;255;48;2;0;0;0"},
    {"hydro_a", "biosyntax.hydro.a", "#AD0052", "#000000", "", "38;2;173;0;82;48;2;0;0;0"},
    {"hydro_r", "biosyntax.hydro.r", "#0000FF", "#000000", "", "38;2;0;0;255;48;2;0;0;0"},
    {"hydro_n", "biosyntax.hydro.n", "#0C00F3", "#000000", "", "38;2;12;0;243;48;2;0;0;0"},
    {"hydro_d", "biosyntax.hydro.d", "#0C00F3", "#000000", "", "38;2;12;0;243;48;2;0;0;0"},
    {"hydro_c", "biosyntax.hydro.c", "#C2003D", "#000000", "", "38;2;194;0;61;48;2;0;0;0"},
    {"hydro_q", "biosyntax.hydro.q", "#0C00F3", "#000000", "", "38;2;12;0;243;48;2;0;0;0"},
    {"hydro_e", "biosyntax.hydro.e", "#0C00F3", "#000000", "", "38;2;12;0;243;48;2;0;0;0"},
    {"hydro_g", "biosyntax.hydro.g", "#6A0095", "#000000", "", "38;2;106;0;149;48;2;0;0;0"},
    {"hydro_h", "biosyntax.hydro.h", "#1500EA", "#000000", "", "38;2;21;0;234;48;2;0;0;0"},
    {"hydro_i", "biosyntax.hydro.i", "#FF0000", "#000000", "", "38;2;255;0;0;48;2;0;0;0"},
    {"hydro_l", "biosyntax.hydro.l", "#EA0015", "#000000", "", "38;2;234;0;21;48;2;0;0;0"},
    {"hydro_k", "biosyntax.hydro.k", "#0000FF", "#000000", "", "38;2;0;0;255;48;2;0;0;0"},
    {"hydro_m", "biosyntax.hydro.m", "#B0004F", "#000000", "", "38;2;176;0;79;48;2;0;0;0"},
    {"hydro_f", "biosyntax.hydro.f", "#CB0034", "#000000", "", "38;2;203;0;52;48;2;0;0;0"},
    {"hydro_p", "biosyntax.hydro.p", "#4600B9", "#000000", "", "38;2;70;0;185;48;2;0;0;0"},
    {"hydro_s", "biosyntax.hydro.s", "#5E00A1", "#000000", "", "38;2;94;0;161;48;2;0;0;0"},
    {"hydro_t", "biosyntax.hydro.t", "#61009E", "#000000", "", "38;2;97;0;158;48;2;0;0;0"},
    {"hydro_w", "biosyntax.hydro.w", "#5B00A4", "#000000", "", "38;2;91;0;164;48;2;0;0;0"},
    {"hydro_y", "biosyntax.hydro.y", "#4F00B0", "#000000", "", "38;2;79;0;176;48;2;0;0;0"},
    {"hydro_v", "biosyntax.hydro.v", "#F60009", "#000000", "", "38;2;246;0;9;48;2;0;0;0"},
    {"hydro_b", "biosyntax.hydro.b", "#0C00F3", "#000000", "", "38;2;12;0;243;48;2;0;0;0"},
    {"hydro_x", "biosyntax.hydro.x", "#680097", "#000000", "", "38;2;104;0;151;48;2;0;0;0"},
    {"hydro_z", "biosyntax.hydro.z", "#0C00F3", "#000000", "", "38;2;12;0;243;48;2;0;0;0"},
    {"hc_a", "biosyntax.hc.a", "#D2D2D2", "#000000", "", "38;2;210;210;210;48;2;0;0;0"},
    {"hc_t", "biosyntax.hc.t", "#4192FF", "#FFFFFF", "", "38;2;65;146;255;48;2;255;255;255"},
    {"hc_g", "biosyntax.hc.g", "#FF3200", "#000000", "", "38;2;255;50;0;48;2;0;0;0"},
    {"hc_c", "biosyntax.hc.c", "#FFFFFF", "#000000", "", "38;2;255;255;255;48;2;0;0;0"},
    {"hc_u", "biosyntax.hc.u", "#4A5CFF", "#FFFFFF", "", "38;2;74;92;255;48;2;255;255;255"},
    {"hc_r", "biosyntax.hc.r", "#F49ACE", "#000000", "", "38;2;244;154;206;48;2;0;0;0"},
    {"hc_y", "biosyntax.hc.y", "#00007D", "#FFFFFF", "", "38;2;0;0;125;48;2;255;255;255"},
    {"hc_s", "biosyntax.hc.s", "#7D0000", "#FFFFFF", "", "38;2;125;0;0;48;2;255;255;255"},
    {"hc_w", "biosyntax.hc.w", "#7DC8FF", "#000000", "", "38;2;125;200;255;48;2;0;0;0"},
    {"hc_m", "biosyntax.hc.m", "#FFFE80", "#000000", "", "38;2;255;254;128;48;2;0;0;0"},
    {"hc_k", "biosyntax.hc.k", "#9309A1", "#000000", "", "38;2;147;9;161;48;2;0;0;0"},
    {"hc_d", "biosyntax.hc.d", "#EEB88E", "#000000", "", "38;2;238;184;142;48;2;0;0;0"},
    {"hc_b", "biosyntax.hc.b", "#EB4915", "#FFFFFF", "", "38;2;235;73;21;48;2;255;255;255"},
    {"hc_v", "biosyntax.hc.v", "#FF6432", "#000000", "", "38;2;255;100;50;48;2;0;0;0"},
    {"hc_h", "biosyntax.hc.h", "#BFD8F9", "#000000", "", "38;2;191;216;249;48;2;0;0;0"},
    {"hc_n", "biosyntax.hc.n", "#838383", "#FFFFFF", "", "38;2;131;131;131;48;2;255;255;255"},
    {"hc_x", "biosyntax.hc.x", "#838383", "#FFFFFF", "", "38;2;131;131;131;48;2;255;255;255"},
    {"hc_gap", "biosyntax.hc.gap", "#838383", "", "", "38;2;131;131;131"},
    {"cigar_match", "biosyntax.cigar.match", "#D8D8D7", "", "", "38;2;216;216;215"},
    {"cigar_mismatch", "biosyntax.cigar.mismatch", "#FF4641", "", "", "38;2;255;70;65"},
    {"cigar_clip", "biosyntax.cigar.clip", "#4F504B", "", "italic", "03;38;2;79;80;75"},
    {"cigar_insertion", "biosyntax.cigar.insertion", "#4192FF", "", "", "38;2;65;146;255"},
    {"cigar_deletion", "biosyntax.cigar.deletion", "#F09000", "", "", "38;2;240;144;0"},
    {"qual_1", "biosyntax.qual.1", "#4F504B", "", "", "38;2;79;80;75"},
    {"qual_3", "biosyntax.qual.3", "#767773", "", "", "38;2;118;119;115"},
    {"qual_6", "biosyntax.qual.6", "#B1B1AF", "", "", "38;2;177;177;175"},
    {"qual_8", "biosyntax.qual.8", "#D8D8D7", "", "", "38;2;216;216;215"},
    {"qual_10", "biosyntax.qual.10", "#FFFFFF", "", "", "38;2;255;255;255"},
    {"qual_10b", "biosyntax.qual.10b", "#FFFFFF", "", "bold", "01;38;2;255;255;255"},
    {"qual_10i", "biosyntax.qual.10i", "#FFFFFF", "", "italic", "03;38;2;255;255;255"},
    {"grad_0", "biosyntax.grad.0", "#FFFFFF", "", "", "38;2;255;255;255"},
    {"grad_1", "biosyntax.grad.1", "#FFE7FD", "", "", "38;2;255;231;253"},
    {"grad_2", "biosyntax.grad.2", "#FFCFFB", "", "", "38;2;255;207;251"},
    {"grad_3", "biosyntax.grad.3", "#FFB7F9", "", "", "38;2;255;183;249"},
    {"grad_4", "biosyntax.grad.4", "#FF9FF7", "", "", "38;2;255;159;247"},
    {"grad_5", "biosyntax.grad.5", "#FF88F6", "", "", "38;2;255;136;246"},
    {"grad_6", "biosyntax.grad.6", "#FF70F4", "", "", "38;2;255;112;244"},
    {"grad_7", "biosyntax.grad.7", "#FF58F2", "", "", "38;2;255;88;242"},
    {"grad_8", "biosyntax.grad.8", "#FF40F0", "", "", "38;2;255;64;240"},
    {"grad_9", "biosyntax.grad.9", "#FF29EF", "", "", "38;2;255;41;239"},
    {"grad_10", "biosyntax.grad.10", "#FF00EB", "", "", "38;2;255;0;235"},
    {"gradbw_0", "biosyntax.gradbw.0", "#3C3D38", "", "", "38;2;60;61;56"},
    {"gradbw_1", "biosyntax.gradbw.1", "#4F504B", "", "", "38;2;79;80;75"},
    {"gradbw_2", "biosyntax.gradbw.2", "#63635F", "", "", "38;2;99;99;95"},
    {"gradbw_3", "biosyntax.gradbw.3", "#767773", "", "", "38;2;118;119;115"},
    {"gradbw_4", "biosyntax.gradbw.4", "#8A8A87", "", "", "38;2;138;138;135"},
    {"gradbw_5", "biosyntax.gradbw.5", "#9D9E9B", "", "", "38;2;157;158;155"},
    {"gradbw_6", "biosyntax.gradbw.6", "#B1B1AF", "", "", "38;2;177;177;175"},
    {"gradbw_7", "biosyntax.gradbw.7", "#C4C4C3", "", "", "38;2;196;196;195"},
    {"gradbw_8", "biosyntax.gradbw.8", "#D8D8D7", "", "", "38;2;216;216;215"},
    {"gradbw_9", "biosyntax.gradbw.9", "#EBEBEB", "", "", "38;2;235;235;235"},
    {"gradbw_10", "biosyntax.gradbw.10", "#FFFFFF", "", "", "38;2;255;255;255"},
    {"gap", "biosyntax.gap", "#E6E6E6", "", "", "38;2;230;230;230"},
    {"null", "biosyntax.null", "#B1B1AF", "", "", "38;2;177;177;175"},
    {"error", "biosyntax.error", "#FFFFFF", "#FF4641", "bold", "01;38;2;255;255;255;48;2;255;70;65"},
    {"orf_start", "biosyntax.orf.start", "#A6E22E", "#000000", "bold", "01;38;2;166;226;46;48;2;0;0;0"},
    {"orf_coding", "biosyntax.orf.coding", "#E6DB74", "#000000", "", "38;2;230;219;116;48;2;0;0;0"},
    {"orf_stop", "biosyntax.orf.stop", "#FF4641", "#000000", "bold", "01;38;2;255;70;65;48;2;0;0;0"},
};
typedef char class_info_count_must_match[(sizeof(g_class_info) / sizeof(g_class_info[0]) == BIOSYN_CLASS__COUNT) ? 1 : -1];

static size_t trim_eol(const char *s, size_t n) { while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) n--; return n; }
static int lo(int c) { return (c >= 'A' && c <= 'Z') ? c + 32 : c; }
static int up(int c) { return (c >= 'a' && c <= 'z') ? c - 32 : c; }
static int streq_ci(const char *a, const char *b) { if (!a || !b) return 0; while (*a && *b) { if (lo((unsigned char)*a) != lo((unsigned char)*b)) return 0; a++; b++; } return *a == 0 && *b == 0; }
static int memeq_ci(const char *s, size_t n, const char *lit) { size_t i; for (i = 0; i < n; i++) if (!lit[i] || lo((unsigned char)s[i]) != lo((unsigned char)lit[i])) return 0; return lit[n] == 0; }
static int starts_ci(const char *s, size_t n, const char *lit) { size_t m = strlen(lit); return n >= m && memeq_ci(s, m, lit); }
static const char *find_ci_bounded(const char *s, size_t n, const char *lit) { size_t m = strlen(lit), i; if (!s || !lit || m == 0 || n < m) return NULL; for (i = 0; i + m <= n; i++) if (memeq_ci(s + i, m, lit)) return s + i; return NULL; }
static void emit(writer_t *w, size_t start, size_t len, uint32_t cls) { if (!w || len == 0 || cls == BIOSYN_CLASS_PLAIN) return; if (w->has_last && w->last_cls == cls && w->last_end == (uint64_t)start) { w->last_end += (uint64_t)len; if (w->out && w->count > 0 && w->count <= w->cap) w->out[w->count - 1].length += (uint64_t)len; return; } if (w->out && w->count < w->cap) { w->out[w->count].start = (uint64_t)start; w->out[w->count].length = (uint64_t)len; w->out[w->count].class_id = cls; w->out[w->count].reserved = 0; } w->count++; w->has_last = 1; w->last_cls = cls; w->last_end = (uint64_t)start + (uint64_t)len; }
static size_t split_tabs(const char *s, size_t n, field_t *f, size_t cap) { size_t i, start = 0, count = 0; for (i = 0; i <= n; i++) if (i == n || s[i] == '\t') { if (count < cap) { f[count].start = start; f[count].len = i - start; } count++; start = i + 1; } return count; }
static size_t split_ws(const char *s, size_t n, field_t *f, size_t cap) { size_t i = 0, count = 0; while (i < n) { while (i < n && isspace((unsigned char)s[i])) i++; if (i >= n) break; if (count < cap) f[count].start = i; while (i < n && !isspace((unsigned char)s[i])) i++; if (count < cap) f[count].len = i - f[count].start; count++; } return count; }
static int parse_i64_field(const char *s, size_t n, int64_t *out) {
    size_t i = 0;
    uint64_t v = 0;
    uint64_t limit = (uint64_t)INT64_MAX;
    int neg = 0;
    if (n == 0) return 0;
    if (s[0] == '-') {
        neg = 1;
        limit = (uint64_t)INT64_MAX + 1u;
        i = 1;
        if (i == n) return 0;
    }
    for (; i < n; i++) {
        unsigned int digit;
        if (!isdigit((unsigned char)s[i])) return 0;
        digit = (unsigned int)(s[i] - '0');
        if (v > (limit - digit) / 10u) return 0;
        v = v * 10u + digit;
    }
    if (out) {
        if (neg && v == (uint64_t)INT64_MAX + 1u)
            *out = INT64_MIN;
        else
            *out = neg ? -(int64_t)v : (int64_t)v;
    }
    return 1;
}
static int is_numish(const char *s, size_t n) { size_t i; int digit = 0; if (n == 0) return 0; for (i = 0; i < n; i++) { unsigned char c = (unsigned char)s[i]; if (isdigit(c)) digit = 1; else if (c == '.' || c == '-' || c == '+' || c == 'e' || c == 'E' || c == ':' || c == '_' || c == '|') {} else return 0; } return digit; }
static int looks_url(const char *s, size_t n) { return starts_ci(s, n, "http://") || starts_ci(s, n, "https://") || starts_ci(s, n, "ftp://") || starts_ci(s, n, "file://"); }
static void emit_num_or_str(writer_t *w, const char *s, size_t start, size_t n) { if (n == 0) return; emit(w, start, n, looks_url(s + start, n) ? BIOSYN_CLASS_URL : (is_numish(s + start, n) ? BIOSYN_CLASS_NUMBER : BIOSYN_CLASS_STRING)); }
static void num_runs(writer_t *w, const char *s, size_t start, size_t n, uint32_t cls) { size_t i = start, end = start + n; while (i < end) { if (isdigit((unsigned char)s[i]) || s[i] == '-' || s[i] == '+') { size_t j = i + 1; while (j < end && (isdigit((unsigned char)s[j]) || s[j] == '.' || s[j] == 'e' || s[j] == 'E' || s[j] == '-' || s[j] == '+')) j++; emit(w, i, j - i, cls); i = j; } else i++; } }

uint32_t biosyn_abi_version(void) { return BIOSYN_ABI_VERSION; }
const char *biosyn_version(void) { return BIOSYN_VERSION_STRING; }
const char *biosyn_class_name(biosyn_class_t c) { return c < BIOSYN_CLASS__COUNT ? g_class_info[c].name : "unknown"; }
const char *biosyn_class_scope(biosyn_class_t c) { return c < BIOSYN_CLASS__COUNT ? g_class_info[c].scope : "unknown"; }
const char *biosyn_class_ansi_sgr(biosyn_class_t c) { return c < BIOSYN_CLASS__COUNT ? g_class_info[c].ansi : "0"; }
const char *biosyn_class_default_foreground(biosyn_class_t c) { return c < BIOSYN_CLASS__COUNT ? g_class_info[c].fg : ""; }
const char *biosyn_class_default_background(biosyn_class_t c) { return c < BIOSYN_CLASS__COUNT ? g_class_info[c].bg : ""; }
const char *biosyn_class_default_font_style(biosyn_class_t c) { return c < BIOSYN_CLASS__COUNT ? g_class_info[c].font : ""; }
uint32_t biosyn_format_count(void) { return BIOSYN_FORMAT__COUNT; }
int biosyn_format_info(biosyn_format_t f, biosyn_format_info_t *out) {
    if (!out || f >= BIOSYN_FORMAT__COUNT) return 0;
    out->name = g_format_info[f].name;
    out->description = g_format_info[f].description;
    out->stateful = g_format_info[f].stateful;
    out->reserved = 0;
    return 1;
}
uint32_t biosyn_class_count(void) { return BIOSYN_CLASS__COUNT; }
int biosyn_class_info(biosyn_class_t c, biosyn_class_info_t *out) {
    if (!out || c >= BIOSYN_CLASS__COUNT) return 0;
    out->name = g_class_info[c].name;
    out->scope = g_class_info[c].scope;
    out->foreground = g_class_info[c].fg;
    out->background = g_class_info[c].bg;
    out->font_style = g_class_info[c].font;
    out->ansi_sgr = g_class_info[c].ansi;
    return 1;
}

biosyn_format_t biosyn_format_from_name(const char *name) {
    if (!name) return BIOSYN_FORMAT_UNKNOWN;
    while (*name == '.') name++;
    if (streq_ci(name, "fasta") || streq_ci(name, "fa") || streq_ci(name, "fna")) return BIOSYN_FORMAT_FASTA;
    if (streq_ci(name, "fasta-nt") || streq_ci(name, "fasta_nt") || streq_ci(name, "fa-nt")) return BIOSYN_FORMAT_FASTA_NT;
    if (streq_ci(name, "fasta-hc") || streq_ci(name, "fasta_hc") || streq_ci(name, "fasta-high-contrast")) return BIOSYN_FORMAT_FASTA_HC;
    if (streq_ci(name, "fasta-clustal") || streq_ci(name, "fasta_clustal") || streq_ci(name, "clustal-aa") || streq_ci(name, "fastaa") || streq_ci(name, "faa")) return BIOSYN_FORMAT_FASTA_CLUSTAL;
    if (streq_ci(name, "fasta-hydro") || streq_ci(name, "fasta-hydrophobicity") || streq_ci(name, "fasta_hydro") || streq_ci(name, "fastahydro")) return BIOSYN_FORMAT_FASTA_HYDRO;
    if (streq_ci(name, "fasta-taylor") || streq_ci(name, "fasta_taylor") || streq_ci(name, "fastaylor")) return BIOSYN_FORMAT_FASTA_TAYLOR;
    if (streq_ci(name, "fasta-zappo") || streq_ci(name, "fasta_zappo") || streq_ci(name, "fastza")) return BIOSYN_FORMAT_FASTA_ZAPPO;
    if (streq_ci(name, "fas") || streq_ci(name, "mfa")) return BIOSYN_FORMAT_FASTA_HC;
    if (streq_ci(name, "fasta-orf") || streq_ci(name, "fasta_orf") || streq_ci(name, "orf")) return BIOSYN_FORMAT_FASTA_ORF;
    if (streq_ci(name, "fastq") || streq_ci(name, "fq")) return BIOSYN_FORMAT_FASTQ;
    if (streq_ci(name, "sam") || streq_ci(name, "bam") || streq_ci(name, "cram")) return BIOSYN_FORMAT_SAM;
    if (streq_ci(name, "vcf") || streq_ci(name, "bcf")) return BIOSYN_FORMAT_VCF;
    if (streq_ci(name, "bed") || streq_ci(name, "bedpe") || streq_ci(name, "narrowpeak") || streq_ci(name, "broadpeak") || streq_ci(name, "bedgraph")) return BIOSYN_FORMAT_BED;
    if (streq_ci(name, "gtf")) return BIOSYN_FORMAT_GTF;
    if (streq_ci(name, "gff") || streq_ci(name, "gff3")) return BIOSYN_FORMAT_GFF;
    if (streq_ci(name, "pdb") || streq_ci(name, "ent")) return BIOSYN_FORMAT_PDB;
    if (streq_ci(name, "clustal") || streq_ci(name, "aln")) return BIOSYN_FORMAT_CLUSTAL;
    if (streq_ci(name, "faidx") || streq_ci(name, "fai")) return BIOSYN_FORMAT_FAIDX;
    if (streq_ci(name, "flagstat")) return BIOSYN_FORMAT_FLAGSTAT;
    if (streq_ci(name, "wig") || streq_ci(name, "wiggle")) return BIOSYN_FORMAT_WIG;
    return BIOSYN_FORMAT_UNKNOWN;
}

biosyn_format_t biosyn_guess_format_from_path(const char *path) {
    const char *base, *dot = NULL; size_t n, i; char ext[64];
    if (!path || !*path) return BIOSYN_FORMAT_UNKNOWN;
    base = strrchr(path, '/');
#if defined(_WIN32)
    { const char *b = strrchr(path, '\\'); if (!base || (b && b > base)) base = b; }
#endif
    base = base ? base + 1 : path; n = strlen(base);
    if (n > 3 && (memeq_ci(base + n - 3, 3, ".gz") || memeq_ci(base + n - 3, 3, ".xz"))) n -= 3;
    else if (n > 4 && (memeq_ci(base + n - 4, 4, ".bgz") || memeq_ci(base + n - 4, 4, ".bz2"))) n -= 4;
    if (n >= 9 && memeq_ci(base + n - 9, 9, ".flagstat")) return BIOSYN_FORMAT_FLAGSTAT;
    for (i = 0; i < n; i++) if (base[i] == '.') dot = base + i;
    if (!dot || dot >= base + n - 1) return biosyn_format_from_name(base);
    n = (size_t)(base + n - (dot + 1)); if (n >= sizeof(ext)) n = sizeof(ext) - 1; memcpy(ext, dot + 1, n); ext[n] = 0; return biosyn_format_from_name(ext);
}

const char *biosyn_format_name(biosyn_format_t f) {
    return f < BIOSYN_FORMAT__COUNT ? g_format_info[f].name : "unknown";
}

void biosyn_state_init(biosyn_state_t *st, biosyn_format_t format) { if (!st) return; memset(st, 0, sizeof(*st)); st->abi_version = BIOSYN_ABI_VERSION; st->format = format; }
biosyn_state_t *biosyn_state_new(biosyn_format_t format) { biosyn_state_t *st = (biosyn_state_t *)calloc(1u, sizeof(*st)); if (st) biosyn_state_init(st, format); return st; }
void biosyn_state_free(biosyn_state_t *st) { free(st); }

#define SEQ_AUTO 0u
#define SEQ_NT 1u
#define SEQ_NT_HC 2u
#define SEQ_AA_GENERIC 3u
#define SEQ_AA_CLUSTAL 4u
#define SEQ_AA_ZAPPO 5u
#define SEQ_AA_TAYLOR 6u
#define SEQ_AA_HYDRO 7u

static uint32_t nt_cls(unsigned char c) {
    switch (up(c)) {
        case 'A': return BIOSYN_CLASS_NT_A; case 'C': return BIOSYN_CLASS_NT_C; case 'G': return BIOSYN_CLASS_NT_G; case 'T': return BIOSYN_CLASS_NT_T; case 'U': return BIOSYN_CLASS_NT_U; case 'N': return BIOSYN_CLASS_NT_N; case 'R': return BIOSYN_CLASS_NT_R; case 'Y': return BIOSYN_CLASS_NT_Y; case 'S': return BIOSYN_CLASS_NT_S; case 'W': return BIOSYN_CLASS_NT_W; case 'M': return BIOSYN_CLASS_NT_M; case 'K': return BIOSYN_CLASS_NT_K; case 'D': return BIOSYN_CLASS_NT_D; case 'B': return BIOSYN_CLASS_NT_B; case 'V': return BIOSYN_CLASS_NT_V; case 'H': return BIOSYN_CLASS_NT_H; case 'X': return BIOSYN_CLASS_NT_X; case '-': case '.': case '*': return BIOSYN_CLASS_GAP; default: return BIOSYN_CLASS_PLAIN;
    }
}
static uint32_t hc_nt_cls(unsigned char c) {
    switch (up(c)) {
        case 'A': return BIOSYN_CLASS_HC_A; case 'C': return BIOSYN_CLASS_HC_C; case 'G': return BIOSYN_CLASS_HC_G; case 'T': return BIOSYN_CLASS_HC_T; case 'U': return BIOSYN_CLASS_HC_U; case 'N': return BIOSYN_CLASS_HC_N; case 'R': return BIOSYN_CLASS_HC_R; case 'Y': return BIOSYN_CLASS_HC_Y; case 'S': return BIOSYN_CLASS_HC_S; case 'W': return BIOSYN_CLASS_HC_W; case 'M': return BIOSYN_CLASS_HC_M; case 'K': return BIOSYN_CLASS_HC_K; case 'D': return BIOSYN_CLASS_HC_D; case 'B': return BIOSYN_CLASS_HC_B; case 'V': return BIOSYN_CLASS_HC_V; case 'H': return BIOSYN_CLASS_HC_H; case 'X': return BIOSYN_CLASS_HC_X; case '-': case '.': case '*': return BIOSYN_CLASS_HC_GAP; default: return BIOSYN_CLASS_PLAIN;
    }
}
static uint32_t aa_generic_cls(unsigned char c) {
    switch (up(c)) {
        case 'A': case 'I': case 'L': case 'M': case 'F': case 'W': case 'V': return BIOSYN_CLASS_AMINO_HYDRO;
        case 'K': case 'R': case 'H': return BIOSYN_CLASS_AMINO_POS;
        case 'D': case 'E': return BIOSYN_CLASS_AMINO_NEG;
        case 'N': case 'Q': case 'S': case 'T': return BIOSYN_CLASS_AMINO_POLAR;
        case 'C': return BIOSYN_CLASS_AMINO_CYS; case 'G': return BIOSYN_CLASS_AMINO_GLY; case 'P': return BIOSYN_CLASS_AMINO_PRO; case 'Y': return BIOSYN_CLASS_AMINO_ARO;
        case 'B': case 'Z': case 'X': return BIOSYN_CLASS_NT_N; case '-': case '.': case '*': return BIOSYN_CLASS_GAP; default: return BIOSYN_CLASS_PLAIN;
    }
}
static uint32_t aa_letter_offset(uint32_t scheme, unsigned char c) {
    if (c == '-' || c == '.' || c == '*') return BIOSYN_CLASS_GAP;
    int idx = -1; switch (up(c)) { case 'A': idx=0; break; case 'R': idx=1; break; case 'N': idx=2; break; case 'D': idx=3; break; case 'C': idx=4; break; case 'Q': idx=5; break; case 'E': idx=6; break; case 'G': idx=7; break; case 'H': idx=8; break; case 'I': idx=9; break; case 'L': idx=10; break; case 'K': idx=11; break; case 'M': idx=12; break; case 'F': idx=13; break; case 'P': idx=14; break; case 'S': idx=15; break; case 'T': idx=16; break; case 'W': idx=17; break; case 'Y': idx=18; break; case 'V': idx=19; break; case 'B': idx=20; break; case 'X': idx=21; break; case 'Z': idx=22; break; default: return BIOSYN_CLASS_PLAIN; }
    if (scheme == SEQ_AA_CLUSTAL) return BIOSYN_CLASS_AA_A + (uint32_t)idx;
    if (scheme == SEQ_AA_ZAPPO) return BIOSYN_CLASS_ZAPPO_A + (uint32_t)idx;
    if (scheme == SEQ_AA_TAYLOR) return BIOSYN_CLASS_TAYLOR_A + (uint32_t)idx;
    if (scheme == SEQ_AA_HYDRO) return BIOSYN_CLASS_HYDRO_A + (uint32_t)idx;
    return aa_generic_cls(c);
}
static int looks_protein(const char *s, size_t start, size_t n) { size_t i; for (i = start; i < start + n; i++) { switch (up((unsigned char)s[i])) { case 'E': case 'F': case 'I': case 'L': case 'P': case 'Q': case 'Z': return 1; default: break; } } return 0; }
static uint32_t seq_char_cls(unsigned char c, uint32_t scheme) { if (scheme == SEQ_NT) return nt_cls(c); if (scheme == SEQ_NT_HC) return hc_nt_cls(c); if (scheme == SEQ_AA_GENERIC) return aa_generic_cls(c); return aa_letter_offset(scheme, c); }
static void seq_runs_scheme(writer_t *w, const char *s, size_t start, size_t n, uint32_t scheme) { size_t i = start, end = start + n; if (scheme == SEQ_AUTO) scheme = looks_protein(s, start, n) ? SEQ_AA_GENERIC : SEQ_NT; while (i < end) { uint32_t c = seq_char_cls((unsigned char)s[i], scheme); size_t j; if (c == BIOSYN_CLASS_PLAIN) { i++; continue; } j = i + 1; while (j < end && seq_char_cls((unsigned char)s[j], scheme) == c) j++; emit(w, i, j - i, c); i = j; } }

static uint32_t q_cls(unsigned char c) { if (c >= '!' && c <= '%') return BIOSYN_CLASS_QUAL_1; if (c >= '&' && c <= '(') return BIOSYN_CLASS_QUAL_3; if (c >= ')' && c <= '0') return BIOSYN_CLASS_QUAL_6; if (c >= '1' && c <= '8') return BIOSYN_CLASS_QUAL_8; if (c >= '9' && c <= '@') return BIOSYN_CLASS_QUAL_10; if (c >= 'A' && c <= 'D') return BIOSYN_CLASS_QUAL_10B; if (c >= 'E' && c <= '~') return BIOSYN_CLASS_QUAL_10I; return BIOSYN_CLASS_ERROR; }
static void q_runs(writer_t *w, const char *s, size_t start, size_t n) { size_t i = start, end = start + n; while (i < end) { uint32_t c = q_cls((unsigned char)s[i]); size_t j = i + 1; while (j < end && q_cls((unsigned char)s[j]) == c) j++; emit(w, i, j - i, c); i = j; } }
static uint32_t bed_score(int64_t v) { if (v <= 0) return BIOSYN_CLASS_GRADBW_0; if (v < 100) return BIOSYN_CLASS_GRADBW_1; if (v < 200) return BIOSYN_CLASS_GRADBW_2; if (v < 300) return BIOSYN_CLASS_GRADBW_3; if (v < 400) return BIOSYN_CLASS_GRADBW_4; if (v < 500) return BIOSYN_CLASS_GRADBW_5; if (v < 600) return BIOSYN_CLASS_GRADBW_6; if (v < 700) return BIOSYN_CLASS_GRADBW_7; if (v < 800) return BIOSYN_CLASS_GRADBW_8; if (v < 900) return BIOSYN_CLASS_GRADBW_9; return BIOSYN_CLASS_GRADBW_10; }
static uint32_t value_grad(const char *s, size_t n) { int64_t v; if (!parse_i64_field(s, n, &v)) return BIOSYN_CLASS_NUMBER; if (v <= 0) return BIOSYN_CLASS_GRAD_0; if (v < 100) return BIOSYN_CLASS_GRAD_1; if (v < 200) return BIOSYN_CLASS_GRAD_2; if (v < 300) return BIOSYN_CLASS_GRAD_3; if (v < 400) return BIOSYN_CLASS_GRAD_4; if (v < 500) return BIOSYN_CLASS_GRAD_5; if (v < 600) return BIOSYN_CLASS_GRAD_6; if (v < 700) return BIOSYN_CLASS_GRAD_7; if (v < 800) return BIOSYN_CLASS_GRAD_8; if (v < 900) return BIOSYN_CLASS_GRAD_9; return BIOSYN_CLASS_GRAD_10; }
static uint32_t mapq_score(int64_t v) { if (v == 255 || v < 10) return BIOSYN_CLASS_GRADBW_1; if (v < 20) return BIOSYN_CLASS_GRADBW_6; if (v < 30) return BIOSYN_CLASS_GRADBW_8; if (v < 40) return BIOSYN_CLASS_GRADBW_10; return BIOSYN_CLASS_QUAL_10I; }

static void hi_fasta_scheme(writer_t *w, const char *s, size_t n, uint32_t scheme) { n = trim_eol(s, n); if (!n) return; if (s[0] == '>') emit(w, 0, n, BIOSYN_CLASS_HEADER); else if (s[0] == ';' || s[0] == '#') emit(w, 0, n, BIOSYN_CLASS_COMMENT); else seq_runs_scheme(w, s, 0, n, scheme); }
static int is_start_codon(const char *s, size_t i, size_t n) {
    unsigned char a, b, c;
    if (i + 3 > n) return 0;
    a = up((unsigned char)s[i]); b = up((unsigned char)s[i + 1]); c = up((unsigned char)s[i + 2]);
    return a == 'A' && b == 'T' && c == 'G';
}
static int is_stop_codon(const char *s, size_t i, size_t n) {
    unsigned char a, b, c;
    if (i + 3 > n) return 0;
    a = up((unsigned char)s[i]); b = up((unsigned char)s[i + 1]); c = up((unsigned char)s[i + 2]);
    return a == 'T' && ((b == 'A' && (c == 'A' || c == 'G')) || (b == 'G' && c == 'A'));
}
static int is_rna_start_codon(const char *s, size_t i, size_t n) {
    unsigned char a, b, c;
    if (i + 3 > n) return 0;
    a = up((unsigned char)s[i]); b = up((unsigned char)s[i + 1]); c = up((unsigned char)s[i + 2]);
    return a == 'A' && b == 'U' && c == 'G';
}
static int is_rna_stop_codon(const char *s, size_t i, size_t n) {
    unsigned char a, b, c;
    if (i + 3 > n) return 0;
    a = up((unsigned char)s[i]); b = up((unsigned char)s[i + 1]); c = up((unsigned char)s[i + 2]);
    return a == 'U' && ((b == 'A' && (c == 'A' || c == 'G')) || (b == 'G' && c == 'A'));
}
static void hi_fasta_orf(writer_t *w, const char *s, size_t n) {
    uint8_t *mark;
    size_t frame, i;
    n = trim_eol(s, n);
    if (!n) return;
    if (s[0] == '>') { emit(w, 0, n, BIOSYN_CLASS_HEADER); return; }
    if (s[0] == ';' || s[0] == '#') { emit(w, 0, n, BIOSYN_CLASS_COMMENT); return; }
    mark = (uint8_t *)calloc(n ? n : 1, 1u);
    if (!mark) { seq_runs_scheme(w, s, 0, n, SEQ_NT); return; }
    for (frame = 0; frame < 3 && frame < n; frame++) {
        for (i = frame; i + 3 <= n; i += 3) {
            int is_start = is_start_codon(s, i, n) || is_rna_start_codon(s, i, n);
            if (is_start) {
                size_t j;
                for (j = i + 3; j + 3 <= n; j += 3) {
                    if (is_stop_codon(s, j, n) || is_rna_stop_codon(s, j, n)) {
                        size_t k;
                        int clear = 1;
                        for (k = i; k < j + 3; k++) if (mark[k]) { clear = 0; break; }
                        if (clear) {
                            for (k = i; k < i + 3; k++) mark[k] = 1;
                            for (k = i + 3; k < j; k++) mark[k] = 2;
                            for (k = j; k < j + 3; k++) mark[k] = 3;
                        }
                        i = j;
                        break;
                    }
                }
            }
        }
    }
    i = 0;
    while (i < n) {
        uint32_t cls;
        size_t j;
        if (mark[i] == 1) cls = BIOSYN_CLASS_ORF_START;
        else if (mark[i] == 2) cls = BIOSYN_CLASS_ORF_CODING;
        else if (mark[i] == 3) cls = BIOSYN_CLASS_ORF_STOP;
        else cls = nt_cls((unsigned char)s[i]);
        if (cls == BIOSYN_CLASS_PLAIN) { i++; continue; }
        j = i + 1;
        while (j < n) {
            uint32_t ncls;
            if (mark[j] == 1) ncls = BIOSYN_CLASS_ORF_START;
            else if (mark[j] == 2) ncls = BIOSYN_CLASS_ORF_CODING;
            else if (mark[j] == 3) ncls = BIOSYN_CLASS_ORF_STOP;
            else ncls = nt_cls((unsigned char)s[j]);
            if (ncls != cls) break;
            j++;
        }
        emit(w, i, j - i, cls);
        i = j;
    }
    free(mark);
}
static void hi_fastq(writer_t *w, const char *s, size_t n, uint32_t phase, uint32_t *next) { n = trim_eol(s, n); if (!n) { if (next) *next = (phase + 1u) & 3u; return; } if (phase == 0) emit(w, 0, n, s[0] == '@' ? BIOSYN_CLASS_HEADER : BIOSYN_CLASS_ERROR); else if (phase == 1) seq_runs_scheme(w, s, 0, n, SEQ_NT); else if (phase == 2) emit(w, 0, n, s[0] == '+' ? BIOSYN_CLASS_HEADER : BIOSYN_CLASS_ERROR); else q_runs(w, s, 0, n); if (next) *next = (phase + 1u) & 3u; }

static void cigar(writer_t *w, const char *s, size_t start, size_t n) { size_t i = start, end = start + n; while (i < end) { if (isdigit((unsigned char)s[i])) { size_t j = i + 1; while (j < end && isdigit((unsigned char)s[j])) j++; emit(w, i, j - i, BIOSYN_CLASS_NUMBER); i = j; } else { uint32_t c = BIOSYN_CLASS_ERROR; if (s[i] == 'M' || s[i] == '=') c = BIOSYN_CLASS_CIGAR_MATCH; else if (s[i] == 'X') c = BIOSYN_CLASS_CIGAR_MISMATCH; else if (s[i] == 'S' || s[i] == 'H' || s[i] == 'P') c = BIOSYN_CLASS_CIGAR_CLIP; else if (s[i] == 'I') c = BIOSYN_CLASS_CIGAR_INSERTION; else if (s[i] == 'D' || s[i] == 'N') c = BIOSYN_CLASS_CIGAR_DELETION; emit(w, i, 1, c); i++; } } }
static void sam_optional_tag(writer_t *w, const char *s, size_t st, size_t le) { if (le >= 5 && s[st+2] == ':' && s[st+4] == ':') { char t = s[st+3]; emit(w, st, 2, BIOSYN_CLASS_KEYWORD3); emit(w, st+3, 1, BIOSYN_CLASS_KEYWORD6); if (t == 'i' || t == 'f') emit_num_or_str(w, s, st+5, le-5); else if (t == 'H') emit(w, st+5, le-5, BIOSYN_CLASS_NUMBER_ALT); else if (t == 'A' || t == 'Z') emit(w, st+5, le-5, BIOSYN_CLASS_STRING); else if (t == 'B') { size_t p = st + 5, end = st + le; while (p < end) { size_t q = p; while (q < end && s[q] != ',') q++; if (q > p) emit(w, p, q-p, (p == st + 5) ? BIOSYN_CLASS_KEYWORD6 : BIOSYN_CLASS_NUMBER); p = q + 1; } } else emit_num_or_str(w, s, st+5, le-5); } else emit_num_or_str(w, s, st, le); }
static uint32_t sam_header_value_class(const char *rec, const char *tag, const char *value, size_t n) {
    if (streq_ci(tag, "VN") || streq_ci(tag, "DT") || streq_ci(tag, "PI") || streq_ci(tag, "LN")) return BIOSYN_CLASS_NUMBER;
    if (streq_ci(tag, "SO")) return (memeq_ci(value, n, "coordinate") || memeq_ci(value, n, "queryname")) ? BIOSYN_CLASS_GOOD : BIOSYN_CLASS_BAD;
    if (streq_ci(tag, "GO")) return (memeq_ci(value, n, "query") || memeq_ci(value, n, "reference")) ? BIOSYN_CLASS_GOOD : BIOSYN_CLASS_BAD;
    if (streq_ci(rec, "@SQ") && (streq_ci(tag, "SN") || streq_ci(tag, "AH") || streq_ci(tag, "AN") || streq_ci(tag, "AS") || streq_ci(tag, "SP"))) return BIOSYN_CLASS_CHROM;
    if (streq_ci(tag, "UR")) return looks_url(value, n) ? BIOSYN_CLASS_URL : BIOSYN_CLASS_STRING;
    if (streq_ci(rec, "@PG") && (streq_ci(tag, "ID") || streq_ci(tag, "PN") || streq_ci(tag, "PP"))) return BIOSYN_CLASS_SOFTWARE;
    if (streq_ci(rec, "@PG") && streq_ci(tag, "CL")) return BIOSYN_CLASS_COMMANDLINE;
    if (streq_ci(rec, "@RG") && (streq_ci(tag, "ID") || streq_ci(tag, "LB") || streq_ci(tag, "SM"))) return BIOSYN_CLASS_SAMPLE;
    if (streq_ci(rec, "@RG") && streq_ci(tag, "PG")) return BIOSYN_CLASS_SOFTWARE;
    return is_numish(value, n) ? BIOSYN_CLASS_NUMBER : BIOSYN_CLASS_STRING;
}
static void hi_sam_header(writer_t *w, const char *s, size_t n) { field_t f[128]; size_t c, i; char rec[4] = {0,0,0,0}; if (n >= 3) { rec[0] = s[0]; rec[1] = s[1]; rec[2] = s[2]; } c = split_tabs(s, n, f, 128); if (c > 0) emit(w, f[0].start, f[0].len, BIOSYN_CLASS_HEADER); if (memeq_ci(rec, 3, "@CO")) { if (c > 1) emit(w, f[1].start, n - f[1].start, BIOSYN_CLASS_COMMENT); return; } for (i = 1; i < c && i < 128; i++) { size_t st = f[i].start, le = f[i].len; if (le >= 4 && s[st+2] == ':') { char tagbuf[3]; tagbuf[0] = s[st]; tagbuf[1] = s[st+1]; tagbuf[2] = 0; emit(w, st, 2, BIOSYN_CLASS_KEYWORD6); emit(w, st + 3, le - 3, sam_header_value_class(rec, tagbuf, s + st + 3, le - 3)); } else if (le) emit_num_or_str(w, s, st, le); } }
static void hi_sam(writer_t *w, const char *s, size_t n) { field_t f[256]; size_t c, i; int64_t v; n = trim_eol(s, n); if (!n) return; if (s[0] == '@') { hi_sam_header(w, s, n); return; } c = split_tabs(s, n, f, 256); for (i = 0; i < c && i < 256; i++) { size_t st = f[i].start, le = f[i].len; if (!le) continue; if (i == 0) emit(w, st, le, BIOSYN_CLASS_NAME); else if (i == 1) { if (parse_i64_field(s + st, le, &v) && v >= 512) emit(w, st, le, BIOSYN_CLASS_BAD); else emit(w, st, le, BIOSYN_CLASS_NUMBER); } else if (i == 2 || i == 6) emit(w, st, le, (le == 1 && (s[st] == '*' || s[st] == '=')) ? BIOSYN_CLASS_NULL : BIOSYN_CLASS_CHROM); else if (i == 3 || i == 7) emit(w, st, le, BIOSYN_CLASS_POSITION); else if (i == 8) emit(w, st, le, BIOSYN_CLASS_NUMBER); else if (i == 4 && parse_i64_field(s + st, le, &v)) emit(w, st, le, mapq_score(v)); else if (i == 5) cigar(w, s, st, le); else if (i == 9) seq_runs_scheme(w, s, st, le, SEQ_NT); else if (i == 10) q_runs(w, s, st, le); else if (i >= 11) sam_optional_tag(w, s, st, le); else emit_num_or_str(w, s, st, le); } }

static void vcf_angle_attrs(writer_t *w, const char *s, size_t start, size_t n, const char *record_key) { size_t p = start, end = start + n; while (p < end) { size_t ks, ke, vs, ve; while (p < end && (s[p] == '<' || s[p] == '>' || s[p] == ',' || isspace((unsigned char)s[p]))) p++; ks = p; while (p < end && s[p] != '=' && s[p] != ',' && s[p] != '>') p++; ke = p; if (ke > ks) emit(w, ks, ke - ks, BIOSYN_CLASS_KEYWORD6); if (p < end && s[p] == '=') { p++; vs = p; if (p < end && s[p] == '"') { p++; while (p < end && s[p] != '"') p++; if (p < end) p++; ve = p; emit(w, vs, ve-vs, BIOSYN_CLASS_QUOTED_STRING); } else { while (p < end && s[p] != ',' && s[p] != '>') p++; ve = p; if (ve > vs) { uint32_t cls = BIOSYN_CLASS_STRING; if (memeq_ci(s + ks, ke - ks, "URL")) cls = BIOSYN_CLASS_URL; else if (memeq_ci(s + ks, ke - ks, "ID") && record_key && streq_ci(record_key, "contig")) cls = BIOSYN_CLASS_CHROM; else if (memeq_ci(s + ks, ke - ks, "Number") || memeq_ci(s + ks, ke - ks, "Type") || memeq_ci(s + ks, ke - ks, "Date") || memeq_ci(s + ks, ke - ks, "Version") || is_numish(s + vs, ve - vs)) cls = BIOSYN_CLASS_NUMBER; emit(w, vs, ve - vs, cls); } } } else { while (p < end && s[p] != ',') p++; } } }
static uint32_t vcf_meta_key_class(const char *s, size_t n) { if (memeq_ci(s,n,"INFO")) return BIOSYN_CLASS_KEYWORD6; if (memeq_ci(s,n,"FORMAT")) return BIOSYN_CLASS_KEYWORD5; if (memeq_ci(s,n,"FILTER")) return BIOSYN_CLASS_KEYWORD4; if (memeq_ci(s,n,"ALT") || memeq_ci(s,n,"SAMPLE") || memeq_ci(s,n,"PEDIGREE")) return BIOSYN_CLASS_KEYWORD3; if (memeq_ci(s,n,"contig")) return BIOSYN_CLASS_CHROM; if (memeq_ci(s,n,"fileformat")) return BIOSYN_CLASS_HEADER; return BIOSYN_CLASS_KEYWORD6; }
static void hi_vcf_meta(writer_t *w, const char *s, size_t n) { size_t p = 2, ks, ke, vs; char keybuf[32]; emit(w, 0, 2, BIOSYN_CLASS_COMMENT); ks = p; while (p < n && s[p] != '=') p++; ke = p; if (ke <= ks) { emit(w, 2, n-2, BIOSYN_CLASS_COMMENT); return; } emit(w, ks, ke-ks, vcf_meta_key_class(s+ks, ke-ks)); { size_t klen = ke-ks < sizeof(keybuf)-1 ? ke-ks : sizeof(keybuf)-1; memcpy(keybuf, s+ks, klen); keybuf[klen]=0; } if (p >= n) return; vs = p + 1; if (vs < n && s[vs] == '<') { emit(w, vs, 1, vcf_meta_key_class(s+ks, ke-ks)); vcf_angle_attrs(w, s, vs+1, n-vs-1, keybuf); if (n > vs && s[n-1] == '>') emit(w, n-1, 1, vcf_meta_key_class(s+ks, ke-ks)); } else { uint32_t cls = BIOSYN_CLASS_STRING; if (memeq_ci(s+ks, ke-ks, "fileformat")) cls = BIOSYN_CLASS_HEADER; else if (memeq_ci(s+ks, ke-ks, "fileDate")) cls = BIOSYN_CLASS_NUMBER; else if (memeq_ci(s+ks, ke-ks, "source")) cls = BIOSYN_CLASS_SOFTWARE; else if (memeq_ci(s+ks, ke-ks, "reference") || memeq_ci(s+ks, ke-ks, "assembly")) cls = looks_url(s+vs, n-vs) ? BIOSYN_CLASS_URL : BIOSYN_CLASS_KEYWORD3; emit(w, vs, n-vs, cls); } }
static void kv_list(writer_t *w, const char *s, size_t start, size_t n) { size_t p = start, end = start + n; while (p < end) { size_t ks, ke, vs, ve; while (p < end && (s[p] == ';' || s[p] == ',' || isspace((unsigned char)s[p]))) { emit(w, p, 1, BIOSYN_CLASS_NULL); p++; } ks = p; while (p < end && s[p] != '=' && s[p] != ';' && s[p] != ',' && !isspace((unsigned char)s[p])) p++; ke = p; if (ke > ks) emit(w, ks, ke-ks, BIOSYN_CLASS_KEYWORD6); while (p < end && isspace((unsigned char)s[p])) p++; if (p < end && s[p] == '=') { p++; vs = p; while (p < end && s[p] != ';') p++; ve = p; if (ve > vs) emit_num_or_str(w, s, vs, ve-vs); } else { while (p < end && s[p] != ';') p++; } } }
static void colon_keys(writer_t *w, const char *s, size_t start, size_t n) { size_t p = start, end = start+n; while (p < end) { size_t st = p; while (p < end && s[p] != ':') p++; if (p > st) emit(w, st, p-st, BIOSYN_CLASS_KEYWORD5); if (p < end) { emit(w, p, 1, BIOSYN_CLASS_NULL); p++; } } }
static void vcf_sample(writer_t *w, const char *s, size_t start, size_t n) { size_t p = start, end = start+n; while (p < end) { if (s[p] == '.' && p + 2 < end && (s[p+1] == '/' || s[p+1] == '|') && s[p+2] == '.') { emit(w, p, 3, BIOSYN_CLASS_NULL); p += 3; } else if (s[p] == '/' ) { emit(w, p, 1, BIOSYN_CLASS_STRING); p++; } else if (s[p] == '|') { emit(w, p, 1, BIOSYN_CLASS_KEYWORD3); p++; } else if (s[p] == ':' || s[p] == ',') { emit(w, p, 1, BIOSYN_CLASS_NULL); p++; } else if (isdigit((unsigned char)s[p]) || s[p] == '-' || s[p] == '.') { size_t q = p + 1; while (q < end && (isdigit((unsigned char)s[q]) || s[q] == '.' || s[q] == '-' || s[q] == 'e' || s[q] == 'E')) q++; emit(w, p, q-p, BIOSYN_CLASS_NUMBER); p = q; } else { size_t q = p + 1; while (q < end && s[q] != ':' && s[q] != ',' && s[q] != '/' && s[q] != '|') q++; emit(w, p, q-p, BIOSYN_CLASS_SAMPLE); p = q; } } }
static void vcf_allele(writer_t *w, const char *s, size_t start, size_t n) { size_t p = start, end = start+n; while (p < end) { if (s[p] == ',' || s[p] == ';') { emit(w,p,1,BIOSYN_CLASS_NULL); p++; } else if (s[p] == '<') { size_t q = p+1; while (q < end && s[q] != '>') q++; if (q < end) q++; emit(w,p,q-p,BIOSYN_CLASS_KEYWORD3); p=q; } else { uint32_t cls = nt_cls((unsigned char)s[p]); size_t q = p+1; if (cls == BIOSYN_CLASS_PLAIN) { while (q < end && s[q] != ',' && s[q] != ';' && s[q] != '<') q++; emit(w,p,q-p,BIOSYN_CLASS_STRING); } else { while (q < end && nt_cls((unsigned char)s[q]) == cls) q++; emit(w,p,q-p,cls); } p=q; } } }
static void hi_vcf_header_line(writer_t *w, const char *s, size_t n) { field_t f[256]; size_t c,i; c=split_tabs(s,n,f,256); for(i=0;i<c&&i<256;i++) emit(w,f[i].start,f[i].len,i<9?BIOSYN_CLASS_HEADER:BIOSYN_CLASS_SAMPLE); }
static void hi_vcf(writer_t *w, const char *s, size_t n) { field_t f[512]; size_t c, i; n = trim_eol(s, n); if (!n) return; if (s[0] == '#') { if (n > 1 && s[1] == '#') hi_vcf_meta(w, s, n); else hi_vcf_header_line(w, s, n); return; } c = split_tabs(s, n, f, 512); for (i = 0; i < c && i < 512; i++) { size_t st=f[i].start, le=f[i].len; if (!le) continue; if (i==0) emit(w,st,le,BIOSYN_CLASS_CHROM); else if (i==1) emit(w,st,le,BIOSYN_CLASS_POSITION); else if (i==2) emit(w,st,le,(le==1&&s[st]=='.')?BIOSYN_CLASS_NULL:BIOSYN_CLASS_NAME); else if (i==3 || i==4) vcf_allele(w,s,st,le); else if (i==5) emit(w,st,le,(le==1&&s[st]=='.')?BIOSYN_CLASS_NULL:BIOSYN_CLASS_NUMBER_ALT); else if (i==6) emit(w,st,le,memeq_ci(s+st,le,"PASS")?BIOSYN_CLASS_GOOD:(le==1&&s[st]=='.')?BIOSYN_CLASS_NULL:BIOSYN_CLASS_BAD); else if (i==7) kv_list(w,s,st,le); else if (i==8) colon_keys(w,s,st,le); else vcf_sample(w,s,st,le); } }

static void strand(writer_t *w, const char *s, size_t start, size_t n) { if (!n) return; emit(w, start, n, s[start] == '+' ? BIOSYN_CLASS_STRAND_PLUS : s[start] == '-' ? BIOSYN_CLASS_STRAND_MINUS : BIOSYN_CLASS_STRAND_NONE); }
static void track_kv(writer_t *w, const char *s, size_t start, size_t n) { field_t f[64]; size_t c=split_ws(s+start,n,f,64), i; for(i=0;i<c&&i<64;i++){size_t st=start+f[i].start, le=f[i].len, p=st, end=st+le; while(p<end&&s[p]!='=')p++; if(p<end){emit(w,st,p-st,BIOSYN_CLASS_KEYWORD6); if(p+1<end) emit(w,p+1,end-p-1,looks_url(s+p+1,end-p-1)?BIOSYN_CLASS_URL:(is_numish(s+p+1,end-p-1)?BIOSYN_CLASS_NUMBER:BIOSYN_CLASS_STRING));}} }
static void hi_bed(writer_t *w, const char *s, size_t n) { size_t col = 0, start = 0, i; n = trim_eol(s, n); if (!n) return; if (s[0] == '#') { emit(w,0,n,BIOSYN_CLASS_COMMENT); return; } if (starts_ci(s,n,"track") || starts_ci(s,n,"browser")) { field_t f[4]; size_t c=split_ws(s,n,f,4); if(c) emit(w,f[0].start,f[0].len,BIOSYN_CLASS_HEADER); if(c>1) track_kv(w,s,f[1].start,n-f[1].start); return; } for (i=0;i<=n;i++) if (i==n || s[i]=='\t') { size_t m=i-start; int64_t v; if (m) { if (col==0) emit(w,start,m,BIOSYN_CLASS_CHROM); else if (col==1 || col==2 || col==6 || col==7) emit(w,start,m,BIOSYN_CLASS_POSITION); else if (col==3) emit(w,start,m,BIOSYN_CLASS_NAME); else if (col==4 && parse_i64_field(s+start,m,&v)) emit(w,start,m,bed_score(v)); else if (col==5) strand(w,s,start,m); else emit_num_or_str(w,s,start,m); } col++; start=i+1; } }
static uint32_t feat_cls(const char *s, size_t n) { if (memeq_ci(s,n,"gene")) return BIOSYN_CLASS_FEATURE_GENE; if (memeq_ci(s,n,"transcript")) return BIOSYN_CLASS_FEATURE_TRANSCRIPT; if (memeq_ci(s,n,"exon")) return BIOSYN_CLASS_FEATURE_EXON; if (memeq_ci(s,n,"CDS")) return BIOSYN_CLASS_FEATURE_CDS; if (memeq_ci(s,n,"start_codon")) return BIOSYN_CLASS_FEATURE_START_CODON; if (memeq_ci(s,n,"stop_codon")) return BIOSYN_CLASS_FEATURE_STOP_CODON; if (memeq_ci(s,n,"5UTR") || memeq_ci(s,n,"3UTR") || memeq_ci(s,n,"UTR")) return BIOSYN_CLASS_FEATURE_UTR; if (memeq_ci(s,n,"inter") || memeq_ci(s,n,"inter_CNS")) return BIOSYN_CLASS_FEATURE_INTER; if (memeq_ci(s,n,"intron_CNS")) return BIOSYN_CLASS_FEATURE_INTRON_CNS; return BIOSYN_CLASS_COMMENT; }
static void attrs(writer_t *w, const char *s, size_t start, size_t n) { size_t p=start,end=start+n; while(p<end){ size_t ks,ke,vs,ve; while(p<end&&(isspace((unsigned char)s[p])||s[p]==';'))p++; if(p>=end)break; ks=p; while(p<end&&s[p]!=' '&&s[p]!='='&&s[p]!=';'&&s[p]!='\t')p++; ke=p; if(ke<=ks){p++;continue;} emit(w,ks,ke-ks,BIOSYN_CLASS_KEYWORD6); while(p<end&&(isspace((unsigned char)s[p])||s[p]=='='))p++; vs=p; if(p<end&&s[p]=='"'){p++; while(p<end&&s[p]!='"')p++; if(p<end)p++; ve=p;} else {while(p<end&&s[p]!=';'&&s[p]!='\t')p++; ve=p; while(ve>vs&&isspace((unsigned char)s[ve-1]))ve--;} if(ve>vs){uint32_t c=BIOSYN_CLASS_STRING; size_t inner_start=vs, inner_len=ve-vs; if(s[vs]=='"'&&ve>vs+1){inner_start=vs+1; inner_len=ve-vs-2; c=BIOSYN_CLASS_QUOTED_STRING;} if(memeq_ci(s+ks,ke-ks,"gene_id")||memeq_ci(s+ks,ke-ks,"gene_name"))c=BIOSYN_CLASS_FEATURE_GENE; else if(memeq_ci(s+ks,ke-ks,"transcript_id"))c=BIOSYN_CLASS_FEATURE_TRANSCRIPT; else if(is_numish(s+inner_start,inner_len))c=BIOSYN_CLASS_NUMBER; else if(looks_url(s+inner_start,inner_len))c=BIOSYN_CLASS_URL; emit(w,vs,ve-vs,c);} while(p<end&&s[p]!=';')p++; } }
static void hi_gtf(writer_t *w, const char *s, size_t n) { size_t col=0,start=0,i; n=trim_eol(s,n); if(!n)return; if(s[0]=='#'){emit(w,0,n,BIOSYN_CLASS_COMMENT);return;} for(i=0;i<=n;i++) if(i==n||s[i]=='\t'){size_t m=i-start; if(m){if(col==0)emit(w,start,m,BIOSYN_CLASS_CHROM); else if(col==1)emit(w,start,m,BIOSYN_CLASS_SOFTWARE); else if(col==2)emit(w,start,m,feat_cls(s+start,m)); else if(col==3||col==4)emit(w,start,m,BIOSYN_CLASS_POSITION); else if(col==5)emit(w,start,m,(m==1&&s[start]=='.')?BIOSYN_CLASS_NULL:value_grad(s+start,m)); else if(col==6)strand(w,s,start,m); else if(col==7)emit(w,start,m,(m==1&&s[start]=='.')?BIOSYN_CLASS_NULL:(m==1&&s[start]=='1')?BIOSYN_CLASS_KEYWORD4:(m==1&&s[start]=='2')?BIOSYN_CLASS_KEYWORD:BIOSYN_CLASS_KEYWORD6); else if(col==8)attrs(w,s,start,m); else emit_num_or_str(w,s,start,m);} col++; start=i+1;} }
static void hi_faidx(writer_t *w, const char *s, size_t n) { field_t f[8]; size_t c,i; static const uint32_t cols[5] = {BIOSYN_CLASS_CHROM, BIOSYN_CLASS_SEQUENCE_LENGTH, BIOSYN_CLASS_FILE_OFFSET, BIOSYN_CLASS_LINE_BASES, BIOSYN_CLASS_LINE_WIDTH}; n=trim_eol(s,n); if(!n)return; c=split_tabs(s,n,f,8); for(i=0;i<c&&i<8;i++) emit(w,f[i].start,f[i].len,i<5?cols[i]:BIOSYN_CLASS_NUMBER); }
static void hi_clustal(writer_t *w, const char *s, size_t n) { field_t f[8]; size_t c; n=trim_eol(s,n); if(!n)return; if(starts_ci(s,n,"CLUSTAL")){emit(w,0,n,BIOSYN_CLASS_HEADER);return;} c=split_ws(s,n,f,8); if(c==0)return; if(s[f[0].start]=='*'||s[f[0].start]==':'||s[f[0].start]=='.'){emit(w,f[0].start,f[0].len,BIOSYN_CLASS_COMMENT);return;} emit(w,f[0].start,f[0].len,BIOSYN_CLASS_NAME); if(c>1)seq_runs_scheme(w,s,f[1].start,f[1].len,SEQ_AUTO); if(c>2)emit(w,f[2].start,f[2].len,BIOSYN_CLASS_NUMBER); }
static void hi_flagstat(writer_t *w, const char *s, size_t n) { const char *p; size_t i=0; n=trim_eol(s,n); while(i<n&&isspace((unsigned char)s[i]))i++; if(i<n&&isdigit((unsigned char)s[i])){size_t j=i+1; while(j<n&&isdigit((unsigned char)s[j]))j++; emit(w,i,j-i,BIOSYN_CLASS_QC_PASSED); i=j; while(i<n&&isspace((unsigned char)s[i]))i++; if(i<n&&s[i]=='+'){i++; while(i<n&&isspace((unsigned char)s[i]))i++; if(i<n&&isdigit((unsigned char)s[i])){j=i+1; while(j<n&&isdigit((unsigned char)s[j]))j++; emit(w,i,j-i,BIOSYN_CLASS_QC_FAILED);}}} p=find_ci_bounded(s,n,"QC-passed reads"); if(p) emit(w,(size_t)(p-s),15,BIOSYN_CLASS_QC_PASSED); p=find_ci_bounded(s,n,"QC-failed reads"); if(p) emit(w,(size_t)(p-s),15,BIOSYN_CLASS_QC_FAILED); p=find_ci_bounded(s,n,"properly"); if(p) emit(w,(size_t)(p-s),8,BIOSYN_CLASS_GOOD); p=find_ci_bounded(s,n,"failed"); if(p) emit(w,(size_t)(p-s),6,BIOSYN_CLASS_BAD); for(i=0;i<n;i++){ if(s[i]=='('||s[i]==':'){size_t j=i+1; while(j<n&&s[j]!='%'&&s[j]!=')'&&s[j]!=' '&&s[j]!='\t')j++; if(j<n&&s[j]=='%'){emit(w,i+1,j-i,find_ci_bounded(s+i,j-i,"nan")?BIOSYN_CLASS_NULL:BIOSYN_CLASS_PERCENT); i=j;}} } }
#define WIG_NONE 0u
#define WIG_FIXED 1u
#define WIG_VARIABLE 2u
static uint32_t hi_wig_mode(writer_t *w, const char *s, size_t n, uint32_t mode, uint32_t *next_mode) { field_t f[64]; size_t c,i; n=trim_eol(s,n); if(next_mode)*next_mode=mode; if(!n)return mode; if(s[0]=='#'){emit(w,0,n,BIOSYN_CLASS_COMMENT);return mode;} c=split_ws(s,n,f,64); if(c==0)return mode; if(starts_ci(s,n,"track")||starts_ci(s,n,"browser")||starts_ci(s,n,"fixedStep")||starts_ci(s,n,"variableStep")){uint32_t m=mode; emit(w,f[0].start,f[0].len,BIOSYN_CLASS_HEADER); if(starts_ci(s,n,"fixedStep"))m=WIG_FIXED; else if(starts_ci(s,n,"variableStep"))m=WIG_VARIABLE; for(i=1;i<c&&i<64;i++){size_t p=f[i].start,end=p+f[i].len; while(p<end&&s[p]!='=')p++; if(p<end){emit(w,f[i].start,p-f[i].start,BIOSYN_CLASS_KEYWORD6); emit(w,p+1,end-p-1,memeq_ci(s+f[i].start,p-f[i].start,"chrom")?BIOSYN_CLASS_CHROM:(memeq_ci(s+f[i].start,p-f[i].start,"start")?BIOSYN_CLASS_POSITION:(is_numish(s+p+1,end-p-1)?BIOSYN_CLASS_NUMBER:BIOSYN_CLASS_STRING)));}} if(next_mode)*next_mode=m; return m; } if(mode==WIG_FIXED || c==1){ for(i=0;i<c&&i<64;i++) emit(w,f[i].start,f[i].len,value_grad(s+f[i].start,f[i].len)); } else { for(i=0;i<c&&i<64;i++) emit(w,f[i].start,f[i].len,i==0?BIOSYN_CLASS_POSITION:value_grad(s+f[i].start,f[i].len)); } return mode; }
static void hi_wig(writer_t *w, const char *s, size_t n) { uint32_t dummy; hi_wig_mode(w,s,n,WIG_NONE,&dummy); }

static void fixed_trim(writer_t *w, const char *s, size_t n, size_t a, size_t b, uint32_t c) { if(a>=n)return; if(b>n)b=n; while(a<b&&isspace((unsigned char)s[a]))a++; while(b>a&&isspace((unsigned char)s[b-1]))b--; if(b>a)emit(w,a,b-a,c); }
static uint32_t chain_cls(unsigned char c) { switch(up(c)){case 'A':case 'G':case 'M':case 'S':case 'Y':return BIOSYN_CLASS_KEYWORD; case 'B':case 'H':case 'N':case 'T':case 'Z':return BIOSYN_CLASS_KEYWORD2; case 'C':case 'I':case 'O':case 'U':return BIOSYN_CLASS_KEYWORD3; case 'D':case 'J':case 'P':case 'V':return BIOSYN_CLASS_KEYWORD4; case 'E':case 'K':case 'Q':case 'W':return BIOSYN_CLASS_KEYWORD5; case 'F':case 'L':case 'R':case 'X':return BIOSYN_CLASS_KEYWORD6; default:return BIOSYN_CLASS_SAMPLE;} }
static uint32_t res3_cls(const char *s, size_t n) { if(n<3)return BIOSYN_CLASS_STRING; if(memeq_ci(s,n,"ALA")||memeq_ci(s,n,"ILE")||memeq_ci(s,n,"LEU")||memeq_ci(s,n,"MET")||memeq_ci(s,n,"PHE")||memeq_ci(s,n,"TRP")||memeq_ci(s,n,"VAL"))return BIOSYN_CLASS_AMINO_HYDRO; if(memeq_ci(s,n,"LYS")||memeq_ci(s,n,"ARG"))return BIOSYN_CLASS_AMINO_POS; if(memeq_ci(s,n,"GLU")||memeq_ci(s,n,"ASP"))return BIOSYN_CLASS_AMINO_NEG; if(memeq_ci(s,n,"ASN")||memeq_ci(s,n,"GLN")||memeq_ci(s,n,"SER")||memeq_ci(s,n,"THR"))return BIOSYN_CLASS_AMINO_POLAR; if(memeq_ci(s,n,"CYS"))return BIOSYN_CLASS_AMINO_CYS; if(memeq_ci(s,n,"GLY"))return BIOSYN_CLASS_AMINO_GLY; if(memeq_ci(s,n,"PRO"))return BIOSYN_CLASS_AMINO_PRO; if(memeq_ci(s,n,"TYR")||memeq_ci(s,n,"HIS"))return BIOSYN_CLASS_AMINO_ARO; return BIOSYN_CLASS_STRING; }
static void pdb_residue_words(writer_t *w, const char *s, size_t start, size_t n) { size_t p=start,end=start+n; while(p<end){ while(p<end&&isspace((unsigned char)s[p]))p++; if(p>=end)break; if(isdigit((unsigned char)s[p])){size_t q=p+1; while(q<end&&isdigit((unsigned char)s[q]))q++; emit(w,p,q-p,BIOSYN_CLASS_NUMBER); p=q;} else {size_t q=p+1; while(q<end&&!isspace((unsigned char)s[q]))q++; emit(w,p,q-p,(q-p)==1?chain_cls((unsigned char)s[p]):res3_cls(s+p,q-p)); p=q;} } }
static void hi_pdb(writer_t *w, const char *s, size_t n) { n=trim_eol(s,n); if(!n)return; if(starts_ci(s,n,"HEADER")){fixed_trim(w,s,n,0,6,BIOSYN_CLASS_HEADER); if(n>6)emit(w,6,n-6,BIOSYN_CLASS_STRING);}
    else if(starts_ci(s,n,"TITLE")||starts_ci(s,n,"SOURCE")){fixed_trim(w,s,n,0,6,BIOSYN_CLASS_KEYWORD5); if(n>6)emit(w,6,n-6,BIOSYN_CLASS_STRING);}
    else if(starts_ci(s,n,"COMPND")){fixed_trim(w,s,n,0,6,BIOSYN_CLASS_STRING); if(n>6)emit(w,6,n-6,BIOSYN_CLASS_STRING);}
    else if(starts_ci(s,n,"KEYWDS")||starts_ci(s,n,"EXPDTA")||starts_ci(s,n,"AUTHOR")||starts_ci(s,n,"REVDAT")){fixed_trim(w,s,n,0,6,BIOSYN_CLASS_KEYWORD); if(n>6)num_runs(w,s,6,n-6,BIOSYN_CLASS_NUMBER);}
    else if(starts_ci(s,n,"JRNL")){fixed_trim(w,s,n,0,6,BIOSYN_CLASS_KEYWORD6); if(find_ci_bounded(s,n,"TITL")) emit(w,(size_t)(find_ci_bounded(s,n,"TITL")-s),4,BIOSYN_CLASS_KEYWORD4); if(n>6)num_runs(w,s,6,n-6,BIOSYN_CLASS_NUMBER);}
    else if(starts_ci(s,n,"REMARK")){fixed_trim(w,s,n,0,6,BIOSYN_CLASS_KEYWORD5); if(find_ci_bounded(s,n,"RESOLUTION")) emit(w,(size_t)(find_ci_bounded(s,n,"RESOLUTION")-s),10,BIOSYN_CLASS_KEYWORD6); if(find_ci_bounded(s,n,"NULL")) emit(w,(size_t)(find_ci_bounded(s,n,"NULL")-s),4,BIOSYN_CLASS_NULL); num_runs(w,s,6,n-6,BIOSYN_CLASS_NUMBER);}
    else if(starts_ci(s,n,"SEQRES")){fixed_trim(w,s,n,0,6,BIOSYN_CLASS_KEYWORD5); if(n>6)pdb_residue_words(w,s,6,n-6);}
    else if(starts_ci(s,n,"HELIX")||starts_ci(s,n,"SITE")){fixed_trim(w,s,n,0,6,BIOSYN_CLASS_KEYWORD); if(n>6)pdb_residue_words(w,s,6,n-6);}
    else if(starts_ci(s,n,"SHEET")){fixed_trim(w,s,n,0,6,BIOSYN_CLASS_STRING); if(n>6)pdb_residue_words(w,s,6,n-6);}
    else if(starts_ci(s,n,"ATOM")||starts_ci(s,n,"HETATM")||starts_ci(s,n,"ANISOU")){fixed_trim(w,s,n,0,6,BIOSYN_CLASS_KEYWORD5); fixed_trim(w,s,n,6,11,BIOSYN_CLASS_NUMBER); fixed_trim(w,s,n,12,16,BIOSYN_CLASS_KEYWORD5); fixed_trim(w,s,n,17,20,res3_cls((n>17?s+17:s), n>20?3:(n>17?n-17:0))); fixed_trim(w,s,n,21,22,n>21?chain_cls((unsigned char)s[21]):BIOSYN_CLASS_SAMPLE); fixed_trim(w,s,n,22,26,BIOSYN_CLASS_POSITION); fixed_trim(w,s,n,30,38,BIOSYN_CLASS_NUMBER); fixed_trim(w,s,n,38,46,BIOSYN_CLASS_NUMBER); fixed_trim(w,s,n,46,54,BIOSYN_CLASS_NUMBER); fixed_trim(w,s,n,54,60,BIOSYN_CLASS_NUMBER_ALT); fixed_trim(w,s,n,60,66,BIOSYN_CLASS_NUMBER_ALT); fixed_trim(w,s,n,76,78,BIOSYN_CLASS_KEYWORD6); fixed_trim(w,s,n,78,80,BIOSYN_CLASS_STRING);} else {fixed_trim(w,s,n,0,6,BIOSYN_CLASS_KEYWORD5); if(n>6)num_runs(w,s,6,n-6,BIOSYN_CLASS_NUMBER);} }

static void dispatch(writer_t *w, biosyn_format_t fmt, const char *line, size_t len, uint32_t phase, uint32_t *next_phase) {
    switch(fmt){ case BIOSYN_FORMAT_FASTA: hi_fasta_scheme(w,line,len,SEQ_AUTO); break; case BIOSYN_FORMAT_FASTA_NT: hi_fasta_scheme(w,line,len,SEQ_NT); break; case BIOSYN_FORMAT_FASTA_HC: hi_fasta_scheme(w,line,len,SEQ_NT_HC); break; case BIOSYN_FORMAT_FASTA_CLUSTAL: hi_fasta_scheme(w,line,len,SEQ_AA_CLUSTAL); break; case BIOSYN_FORMAT_FASTA_HYDRO: hi_fasta_scheme(w,line,len,SEQ_AA_HYDRO); break; case BIOSYN_FORMAT_FASTA_TAYLOR: hi_fasta_scheme(w,line,len,SEQ_AA_TAYLOR); break; case BIOSYN_FORMAT_FASTA_ZAPPO: hi_fasta_scheme(w,line,len,SEQ_AA_ZAPPO); break; case BIOSYN_FORMAT_FASTA_ORF: hi_fasta_orf(w,line,len); break; case BIOSYN_FORMAT_FASTQ: hi_fastq(w,line,len,phase,next_phase); break; case BIOSYN_FORMAT_SAM: hi_sam(w,line,len); break; case BIOSYN_FORMAT_VCF: hi_vcf(w,line,len); break; case BIOSYN_FORMAT_BED: hi_bed(w,line,len); break; case BIOSYN_FORMAT_GTF: case BIOSYN_FORMAT_GFF: hi_gtf(w,line,len); break; case BIOSYN_FORMAT_PDB: hi_pdb(w,line,len); break; case BIOSYN_FORMAT_CLUSTAL: hi_clustal(w,line,len); break; case BIOSYN_FORMAT_FAIDX: hi_faidx(w,line,len); break; case BIOSYN_FORMAT_FLAGSTAT: hi_flagstat(w,line,len); break; case BIOSYN_FORMAT_WIG: hi_wig(w,line,len); break; default: break; }
}
uint64_t biosyn_highlight_line(biosyn_format_t fmt, const char *line, uint64_t len, uint64_t zero_based_line_no, biosyn_span_t *out, uint64_t out_cap) { writer_t w; uint32_t phase=(fmt==BIOSYN_FORMAT_FASTQ)?(uint32_t)(zero_based_line_no&3u):0u; if(len>(uint64_t)SIZE_MAX||out_cap>(uint64_t)SIZE_MAX)return 0; if(!line&&len>0)return 0; w.out=out; w.cap=(size_t)out_cap; w.count=0; w.has_last=0; dispatch(&w,fmt,line?line:"",(size_t)len,phase,NULL); return (uint64_t)w.count; }
uint64_t biosyn_highlight_next_line(biosyn_state_t *st, const char *line, uint64_t len, biosyn_span_t *out, uint64_t out_cap) {
    writer_t w;
    uint32_t next = 0, phase = 0;
    biosyn_format_t fmt;
    if (len > (uint64_t)SIZE_MAX || out_cap > (uint64_t)SIZE_MAX) return 0;
    if (!st || (!line && len > 0)) return 0;
    if (st->abi_version != BIOSYN_ABI_VERSION) biosyn_state_init(st, st->format);
    fmt = st->format;
    w.out = NULL;
    w.cap = 0;
    w.count = 0;
    w.has_last = 0;
    if (fmt == BIOSYN_FORMAT_FASTQ) {
        phase = st->fastq_phase;
        dispatch(&w, fmt, line ? line : "", (size_t)len, phase, &next);
    } else if (fmt == BIOSYN_FORMAT_WIG) {
        hi_wig_mode(&w, line ? line : "", (size_t)len, st->wig_mode, &next);
    } else {
        dispatch(&w, fmt, line ? line : "", (size_t)len, 0, NULL);
    }
    if (w.count > out_cap) return w.count;
    w.out = out;
    w.cap = (size_t)out_cap;
    w.count = 0;
    w.has_last = 0;
    if (fmt == BIOSYN_FORMAT_FASTQ) {
        dispatch(&w, fmt, line ? line : "", (size_t)len, phase, &next);
        st->fastq_phase = next & 3u;
    } else if (fmt == BIOSYN_FORMAT_WIG) {
        hi_wig_mode(&w, line ? line : "", (size_t)len, st->wig_mode, &next);
        st->wig_mode = next;
    } else {
        dispatch(&w, fmt, line ? line : "", (size_t)len, 0, NULL);
    }
    st->line_no++;
    return (uint64_t)w.count;
}
static void app(char *out, size_t cap, size_t *pos, const char *s, size_t n) { if(out&&cap>0){size_t avail=*pos<cap?cap-*pos:0;if(avail>1){size_t copy=n<avail-1?n:avail-1;memcpy(out+*pos,s,copy);}} *pos+=n; }
static void app_cstr(char *out, size_t cap, size_t *pos, const char *s) { app(out,cap,pos,s,strlen(s)); }
static const char *style_sgr(biosyn_class_t cls, const biosyn_ansi_style_t *styles, size_t style_count) {
    size_t i;
    for (i = 0; styles && i < style_count; i++) if (styles[i].class_id == cls && styles[i].ansi_sgr) return styles[i].ansi_sgr;
    return biosyn_class_ansi_sgr(cls);
}
uint64_t biosyn_render_ansi_line_with_styles(const char *line, uint64_t len, const biosyn_span_t *spans, uint64_t span_count, const biosyn_ansi_style_t *styles, uint64_t style_count, char *out, uint64_t out_cap) { size_t pos=0,cursor=0,i,n,sc,stc,cap; if(len>(uint64_t)SIZE_MAX||span_count>(uint64_t)SIZE_MAX||style_count>(uint64_t)SIZE_MAX||out_cap>(uint64_t)SIZE_MAX)return 0; n=(size_t)len; sc=(size_t)span_count; stc=(size_t)style_count; cap=(size_t)out_cap; if(!line){if(out&&cap)out[0]=0;return 0;} for(i=0;i<sc;i++){uint64_t ss,ee; size_t s,e; if(!spans)break; ss=spans[i].start; ee=spans[i].length>UINT64_MAX-ss?UINT64_MAX:ss+spans[i].length; if(ss>len)ss=len; if(ee>len)ee=len; s=(size_t)ss; e=(size_t)ee; if(e<=s||s<cursor)continue; if(s>cursor)app(out,cap,&pos,line+cursor,s-cursor); app_cstr(out,cap,&pos,"\033["); app_cstr(out,cap,&pos,style_sgr(spans[i].class_id,styles,stc)); app_cstr(out,cap,&pos,"m"); app(out,cap,&pos,line+s,e-s); app_cstr(out,cap,&pos,"\033[m"); cursor=e;} if(cursor<n)app(out,cap,&pos,line+cursor,n-cursor); if(out&&cap)out[pos<cap?pos:cap-1]=0; return (uint64_t)pos; }
uint64_t biosyn_render_ansi_line(const char *line, uint64_t len, const biosyn_span_t *spans, uint64_t span_count, char *out, uint64_t out_cap) { return biosyn_render_ansi_line_with_styles(line,len,spans,span_count,NULL,0,out,out_cap); }
