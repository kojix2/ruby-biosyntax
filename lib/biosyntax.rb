require_relative 'biosyntax/version'
require_relative 'biosyntax/biosyntax_ext'

# Ruby bindings for the vendored `libbiosyntax` tokenizer/highlighter.
#
# BioSyntax highlights biological text formats one line at a time. The parser
# and ANSI renderer are implemented by a native C extension; this module exposes
# Ruby value objects and convenience factories around that extension.
#
# @example Highlight a VCF line and inspect spans
#   highlighter = BioSyntax.vcf
#   line = "chr1\t42\trs1\tA\tT\t99\tPASS\tDP=10\n"
#   highlighter.highlight(line).each do |span|
#     puts [span.start, span.end, span.kind_name, span.scope].join("\t")
#   end
#
# @example Render ANSI-colored output
#   highlighter = BioSyntax.fastq
#   File.foreach("reads.fastq", chomp: false) do |line|
#     print highlighter.colorize(line)
#   end
module BioSyntax
  # Base error class for BioSyntax exceptions.
  class Error < StandardError; end

  # Raised when a format name or id is not supported by libbiosyntax.
  class UnsupportedFormatError < Error; end

  # Raised when a token kind name or id is not known.
  class UnknownKindError < Error; end

  class << self
    private

    def normalize_name(value)
      value.to_s.downcase.to_sym
    end

    def constant_name(value)
      value.to_s.gsub(/[^0-9A-Za-z]+/, '_').upcase.sub(/\A_+/, '').sub(/_+\z/, '')
    end
  end

  # Metadata for a supported input format.
  #
  # Format objects are generated from the native extension at load time. They are
  # immutable and comparable by native format id.
  #
  # @see BioSyntax::FORMATS
  # @see BioSyntax.format
  class Format
    # @return [Integer] native format id
    # @return [Symbol] canonical format name, such as `:vcf` or `:"fasta-nt"`
    # @return [String] human-readable format description
    attr_reader :id, :name, :description

    # @api private
    def initialize(id:, name:, description:, stateful:)
      @id = Integer(id)
      @name = BioSyntax.__send__(:normalize_name, name)
      @description = String(description).freeze
      @stateful = !!stateful
      freeze
    end

    # @return [Boolean] true when highlighting depends on previous lines
    def stateful?
      @stateful
    end

    # Ruby factory method name for this format.
    #
    # Hyphenated format names are exposed with underscores, for example
    # `:"fasta-nt"` becomes `:fasta_nt`.
    #
    # @return [Symbol]
    def method_name
      name.to_s.tr('-', '_').to_sym
    end

    # @return [String] canonical format name
    def to_s
      name.to_s
    end

    # @return [Hash] serializable metadata for this format
    def to_h
      {
        id: id,
        name: name,
        description: description,
        stateful: stateful?
      }
    end

    # @param other [Object]
    # @return [Boolean]
    def ==(other)
      other.is_a?(Format) && other.id == id
    end
    alias eql? ==

    # @return [Integer]
    def hash
      [self.class, id].hash
    end

    # @return [String]
    def inspect
      "#<#{self.class} name=#{name.inspect} id=#{id} stateful=#{stateful?}>"
    end
  end

  # Metadata for a semantic token kind.
  #
  # Kinds describe spans returned by {Highlighter#highlight}. They include a
  # TextMate-style scope and the ANSI SGR sequence used by {Highlighter#colorize}.
  #
  # @see BioSyntax::KINDS
  # @see BioSyntax.kind
  class Kind
    # @return [Integer] native kind id
    # @return [Symbol] canonical kind name, such as `:chrom`
    # @return [String] semantic scope, such as `"biosyntax.chrom"`
    # @return [String] foreground color as a hex string, or an empty string
    # @return [String] background color as a hex string, or an empty string
    # @return [String] font style, or an empty string
    # @return [String] ANSI SGR sequence without the surrounding escape bytes
    attr_reader :id, :name, :scope, :foreground, :background, :font_style, :ansi_sgr

    # @api private
    def initialize(id:, name:, scope:, foreground:, background:, font_style:, ansi_sgr:)
      @id = Integer(id)
      @name = BioSyntax.__send__(:normalize_name, name)
      @scope = String(scope).freeze
      @foreground = String(foreground).freeze
      @background = String(background).freeze
      @font_style = String(font_style).freeze
      @ansi_sgr = String(ansi_sgr).freeze
      freeze
    end

    # @return [String] canonical kind name
    def to_s
      name.to_s
    end

    # @return [Hash] serializable metadata for this kind
    def to_h
      {
        id: id,
        name: name,
        scope: scope,
        foreground: foreground,
        background: background,
        font_style: font_style,
        ansi_sgr: ansi_sgr
      }
    end

    # @param other [Object]
    # @return [Boolean]
    def ==(other)
      other.is_a?(Kind) && other.id == id
    end
    alias eql? ==

    # @return [Integer]
    def hash
      [self.class, id].hash
    end

    # @return [String]
    def inspect
      "#<#{self.class} name=#{name.inspect} id=#{id} scope=#{scope.inspect}>"
    end
  end

  # A highlighted byte range within one input line.
  #
  # Offsets are byte offsets into the original line, not character indexes. This
  # matches the native C API and keeps slicing correct for arbitrary encodings.
  #
  # @example Extract the text covered by a span
  #   text = line.byteslice(span.start, span.length)
  class Span
    # @return [Integer] byte offset at the start of the span
    # @return [Integer] byte length of the span
    # @return [Integer] native kind id for this span
    attr_reader :start, :length, :kind_id

    # @api private
    def initialize(start, length, kind_id)
      @start = Integer(start)
      @length = Integer(length)
      @kind_id = Integer(kind_id)
      freeze
    end

    # @return [Integer] byte offset just after the span
    def end
      @start + @length
    end

    # @return [Kind] token kind metadata for this span
    def kind
      BioSyntax.kind(@kind_id)
    end

    # @return [Symbol] token kind name
    def kind_name
      kind.name
    end

    # @return [String] semantic scope for this span
    def scope
      kind.scope
    end

    # @return [Range<Integer>] byte range covered by this span
    def range
      @start...self.end
    end

    # @return [Array(Integer, Integer, Symbol)] start offset, end offset, and kind name
    def to_a
      [@start, self.end, kind.name]
    end

    # Pattern matching support.
    #
    # @return [Array(Integer, Integer, Symbol)]
    def deconstruct
      to_a
    end

    # @return [Hash] serializable metadata for this span
    def to_h
      {
        start: @start,
        end: self.end,
        length: @length,
        kind: kind.name,
        kind_id: @kind_id,
        scope: kind.scope
      }
    end

    # @param other [Object]
    # @return [Boolean]
    def ==(other)
      other.is_a?(Span) &&
        other.start == @start &&
        other.length == @length &&
        other.kind_id == @kind_id
    end
    alias eql? ==

    # @return [Integer]
    def hash
      [self.class, @start, @length, @kind_id].hash
    end

    # @return [String]
    def inspect
      "#<#{self.class} start=#{@start} end=#{self.end} kind=#{kind.name.inspect}>"
    end
  end

  # Stateful highlighter for one input format.
  #
  # Reuse one highlighter for one logical input stream. Some formats, such as
  # FASTQ and WIG, need line-to-line state. Call {#reset} before reusing the
  # object for another stream.
  #
  # @example
  #   highlighter = BioSyntax[:vcf]
  #   File.foreach("sample.vcf", chomp: false) do |line|
  #     print highlighter.colorize(line)
  #   end
  class Highlighter
    # @return [Format] format metadata for this highlighter
    attr_reader :format

    # @param format [Format, Symbol, String, Integer] format object, name, alias, or native id
    # @raise [UnsupportedFormatError] if the format is not supported
    def initialize(format)
      @format = BioSyntax.format(format)
      @state = Native::State.new(@format.id)
    end

    # @return [Symbol] canonical format name
    def format_name
      @format.name
    end

    # Highlight one input line and return semantic spans.
    #
    # @param line [String] one input line
    # @return [Array<Span>] highlighted spans for the line
    def highlight(line)
      @state.highlight(line)
    end
    alias highlight_line highlight

    # Highlight one input line and return ANSI-colored text.
    #
    # @param line [String] one input line
    # @return [String] line with ANSI SGR escape sequences
    def colorize(line)
      @state.colorize(line)
    end
    alias colorize_line colorize
    alias render_ansi colorize
    alias render_ansi_line colorize

    # Reset line-oriented parser state.
    #
    # @return [Highlighter] self
    def reset
      @state.reset(@format.id)
      self
    end

    # Number of lines processed since initialization or the last reset.
    #
    # @return [Integer]
    def line_no
      @state.line_no
    end

    # @return [Boolean] true when this format depends on previous lines
    def stateful?
      @format.stateful?
    end

    # @return [String]
    def inspect
      "#<#{self.class} format=#{format_name.inspect} line_no=#{line_no}>"
    end
  end

  # Version string reported by the vendored native `libbiosyntax` core.
  # @return [String]
  LIBBIOSYNTAX_VERSION = Native.libbiosyntax_version.freeze

  # ABI version reported by the vendored native `libbiosyntax` core.
  # @return [Integer]
  LIBBIOSYNTAX_ABI_VERSION = Native.abi_version

  RAW_FORMATS = Native.formats_raw.freeze
  RAW_KINDS = Native.kinds_raw.freeze
  private_constant :RAW_FORMATS, :RAW_KINDS

  # Supported formats keyed by canonical format name.
  #
  # @return [Hash{Symbol => Format}]
  FORMATS = RAW_FORMATS.each_with_object({}) do |row, hash|
    next if row.fetch(:id).zero?

    format = Format.new(
      id: row.fetch(:id),
      name: row.fetch(:name),
      description: row.fetch(:description),
      stateful: row.fetch(:stateful)
    )
    hash[format.name] = format
  end.freeze

  # Known token kinds keyed by canonical kind name.
  #
  # @return [Hash{Symbol => Kind}]
  KINDS = RAW_KINDS.each_with_object({}) do |row, hash|
    kind = Kind.new(
      id: row.fetch(:id),
      name: row.fetch(:name),
      scope: row.fetch(:scope),
      foreground: row.fetch(:foreground),
      background: row.fetch(:background),
      font_style: row.fetch(:font_style),
      ansi_sgr: row.fetch(:ansi_sgr)
    )
    hash[kind.name] = kind
  end.freeze

  # @return [Array<Symbol>] supported canonical format names
  FORMAT_NAMES = FORMATS.keys.freeze

  # @return [Array<Symbol>] known canonical kind names
  KIND_NAMES = KINDS.keys.freeze
  FORMATS_BY_ID = FORMATS.values.to_h { |format| [format.id, format] }.freeze
  KINDS_BY_ID = KINDS.values.to_h { |kind| [kind.id, kind] }.freeze
  private_constant :FORMATS_BY_ID, :KINDS_BY_ID

  # Token kinds grouped by semantic scope.
  #
  # @return [Hash{String => Array<Kind>}]
  SCOPES = KINDS.values.each_with_object(Hash.new { |hash, key| hash[key] = [] }) do |kind, hash|
    hash[kind.scope] << kind
  end.each_with_object({}) do |(scope, kinds), hash|
    hash[scope.freeze] = kinds.freeze
  end.freeze

  class << self
    # Create a highlighter for a format.
    #
    # @param format [Format, Symbol, String, Integer] format object, name, alias, or native id
    # @return [Highlighter]
    # @raise [UnsupportedFormatError] if the format is not supported
    def [](format)
      Highlighter.new(format)
    end
    alias highlighter []

    # @return [Array<Symbol>] supported canonical format names
    def formats
      FORMAT_NAMES
    end

    # @return [Array<Symbol>] known canonical kind names
    def kinds
      KIND_NAMES
    end

    # Resolve a format object from a name, alias, id, or existing object.
    #
    # @param value [Format, Symbol, String, Integer]
    # @return [Format]
    # @raise [UnsupportedFormatError] if the format is not supported
    def format(value)
      return value if value.is_a?(Format)

      found = case value
              when Integer
                FORMATS_BY_ID[value]
              else
                name = value.to_s.downcase
                FORMATS[name.to_sym] ||
                  FORMATS[name.tr('_', '-').to_sym] ||
                  FORMATS_BY_ID[Native.format_id_from_name(name)]
              end

      return found if found

      raise UnsupportedFormatError, "unsupported format: #{value.inspect}"
    end

    # Resolve the canonical name for a format.
    #
    # @param value [Format, Symbol, String, Integer]
    # @return [Symbol, nil]
    def format_name(value)
      format(value).name
    rescue UnsupportedFormatError
      nil
    end

    # @param value [Format, Symbol, String, Integer]
    # @return [Boolean]
    def format_supported?(value)
      !format_name(value).nil?
    end

    # Resolve token kind metadata from a name, id, or existing object.
    #
    # @param value [Kind, Symbol, String, Integer]
    # @return [Kind]
    # @raise [UnknownKindError] if the kind is not known
    def kind(value)
      return value if value.is_a?(Kind)

      found = case value
              when Integer
                KINDS_BY_ID[value]
              else
                KINDS[normalize_name(value)] || KINDS[value.to_s.downcase.tr('-', '_').to_sym]
              end

      return found if found

      raise UnknownKindError, "unknown kind: #{value.inspect}"
    end

    # Resolve the canonical name for a token kind.
    #
    # @param value [Kind, Symbol, String, Integer]
    # @return [Symbol, nil]
    def kind_name(value)
      kind(value).name
    rescue UnknownKindError
      nil
    end

    # @param value [Kind, Symbol, String, Integer]
    # @return [Boolean]
    def kind_known?(value)
      !kind_name(value).nil?
    end

    # Guess a format from a path or extension.
    #
    # @param path_or_extension [String, #to_s]
    # @return [Symbol, nil] canonical format name if recognized
    def guess_format(path_or_extension)
      id = Native.guess_format_id(path_or_extension.to_s)
      format = FORMATS_BY_ID[id]
      format&.name
    end

    # Guess a format from a path or extension and create a highlighter.
    #
    # @param path_or_extension [String, #to_s]
    # @return [Highlighter, nil]
    def guess(path_or_extension)
      name = guess_format(path_or_extension)
      name && Highlighter.new(name)
    end
  end

  FORMATS.each_value do |format|
    const_name = constant_name(format.name)
    Format.const_set(const_name, format) unless Format.const_defined?(const_name, false)

    method_name = format.method_name
    next unless method_name.to_s.match?(/\A[a-z_]\w*\z/)
    next if singleton_class.method_defined?(method_name) ||
            singleton_class.private_method_defined?(method_name)

    define_singleton_method(method_name) do
      Highlighter.new(format)
    end
  end

  KINDS.each_value do |kind|
    const_name = constant_name(kind.name)
    Kind.const_set(const_name, kind) unless Kind.const_defined?(const_name, false)
  end
end
