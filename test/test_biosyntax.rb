require_relative 'test_helper'

class TestBioSyntax < Minitest::Test
  ANSI_RE = /\e\[[0-9;]*m/

  def strip_ansi(str)
    str.gsub(ANSI_RE, '')
  end

  def test_versions_are_available
    assert_match(/\A\d+\.\d+\.\d+\z/, BioSyntax::VERSION)
    assert_match(/\A\d+\.\d+\.\d+\z/, BioSyntax::LIBBIOSYNTAX_VERSION)
    assert_operator BioSyntax::LIBBIOSYNTAX_ABI_VERSION, :>=, 1
  end

  def test_metadata_is_generated_from_libbiosyntax
    assert_includes BioSyntax::FORMAT_NAMES, :vcf
    assert_includes BioSyntax::FORMAT_NAMES, :fastq
    assert_instance_of BioSyntax::Format, BioSyntax::FORMATS.fetch(:vcf)
    assert_equal :vcf, BioSyntax::FORMATS.fetch(:vcf).name
    assert_same BioSyntax::FORMATS.fetch(:vcf), BioSyntax::Format::VCF

    assert_includes BioSyntax::KIND_NAMES, :chrom
    assert_instance_of BioSyntax::Kind, BioSyntax::KINDS.fetch(:chrom)
    assert_equal 'biosyntax.chrom', BioSyntax::KINDS.fetch(:chrom).scope
    assert_same BioSyntax::KINDS.fetch(:chrom), BioSyntax::Kind::CHROM
    assert_includes BioSyntax::SCOPES.fetch('biosyntax.chrom'), BioSyntax::KINDS.fetch(:chrom)
  end

  def test_dynamic_format_factory
    assert_respond_to BioSyntax, :vcf
    assert_respond_to BioSyntax, :fasta_nt

    highlighter = BioSyntax.vcf
    assert_instance_of BioSyntax::Highlighter, highlighter
    assert_equal :vcf, highlighter.format_name
    assert_same BioSyntax::FORMATS.fetch(:vcf), highlighter.format
  end

  def test_aliases_and_format_helpers
    assert BioSyntax.format_supported?(:vcf)
    assert BioSyntax.format_supported?(:bam)
    refute BioSyntax.format_supported?(:not_a_format)

    assert_equal :sam, BioSyntax.format_name(:bam)
    assert_equal :"fasta-nt", BioSyntax.format_name(:fasta_nt)
    assert_equal :"fasta-nt", BioSyntax[:fasta_nt].format_name
    assert_equal :vcf, BioSyntax.guess_format('sample.vcf.gz')
    assert_nil BioSyntax.guess_format('sample.txt')
    assert_nil BioSyntax.guess('sample.txt')
    assert_instance_of BioSyntax::Highlighter, BioSyntax.guess('sample.fastq')
  end

  def test_highlight_vcf_line
    line = "chr1\t42\trs1\tA\tT\t99\tPASS\tDP=10;AF=0.5\n"
    spans = BioSyntax.vcf.highlight(line)

    refute_empty spans
    assert(spans.all? { |span| span.is_a?(BioSyntax::Span) })

    first = spans.first
    assert_equal 0, first.start
    assert_operator first.end, :>, first.start
    assert_equal :chrom, first.kind_name
    assert_equal BioSyntax::KINDS.fetch(:chrom), first.kind
    assert_equal 'biosyntax.chrom', first.scope
    assert_equal [first.start, first.end, first.kind_name], first.to_a
    assert_equal [first.start, first.end, first.kind_name], first.deconstruct
    assert_equal first.start...first.end, first.range

    assert_equal(
      {
        start: 0,
        end: first.end,
        length: first.length,
        kind: :chrom,
        kind_id: first.kind_id,
        scope: 'biosyntax.chrom'
      },
      first.to_h
    )
    assert_equal first, BioSyntax::Span.new(first.start, first.length, first.kind_id)
    refute_equal first, BioSyntax::Span.new(first.start + 1, first.length, first.kind_id)
  end

  def test_colorize_preserves_text_after_removing_ansi
    line = "chr1\t42\trs1\tA\tT\t99\tPASS\tDP=10;AF=0.5\n"
    colored = BioSyntax.vcf.colorize(line)

    assert_includes colored, "\e["
    assert_equal line, strip_ansi(colored)
  end

  def test_colorize_preserves_encoding_for_empty_span_line
    line = "\n".b
    colored = BioSyntax.vcf.colorize(line)

    assert_empty BioSyntax.vcf.highlight(line)
    assert_equal line, colored
    assert_equal Encoding::BINARY, colored.encoding
  end

  def test_highlight_handles_more_than_stack_buffer_spans
    info = Array.new(80) { |i| "DP#{i}=#{i}" }.join(';')
    line = "chr1\t42\trs1\tA\tT\t99\tPASS\t#{info}\n"
    spans = BioSyntax.vcf.highlight(line)

    assert_operator spans.size, :>, 64
    assert_equal line, strip_ansi(BioSyntax.vcf.colorize(line))
  end

  def test_highlighter_is_stateful_and_resettable
    highlighter = BioSyntax.fastq

    assert highlighter.stateful?
    assert_equal 0, highlighter.line_no

    header_spans = highlighter.highlight("@read1\n")
    sequence_spans = highlighter.highlight("ACGTN\n")

    assert_equal 2, highlighter.line_no
    assert_includes header_spans.map(&:kind_name), :header
    assert_includes sequence_spans.map(&:kind_name), :nt_a

    highlighter.reset
    assert_equal 0, highlighter.line_no
  end

  def test_highlighter_reports_non_stateful_formats
    highlighter = BioSyntax.fasta

    refute highlighter.stateful?
    assert_equal 0, highlighter.line_no

    highlighter.highlight(">seq1\n")
    assert_equal 1, highlighter.line_no

    highlighter.reset
    assert_equal 0, highlighter.line_no
  end

  def test_unsupported_format_raises
    assert_raises(BioSyntax::UnsupportedFormatError) do
      BioSyntax::Highlighter.new(:not_a_format)
    end
  end

  def test_unknown_kind_raises
    assert_nil BioSyntax.kind_name(:not_a_kind)
    refute BioSyntax.kind_known?(:not_a_kind)

    assert_raises(BioSyntax::UnknownKindError) do
      BioSyntax.kind(:not_a_kind)
    end
  end
end
