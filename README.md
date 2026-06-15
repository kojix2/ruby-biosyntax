# ruby-biosyntax

[![CI](https://github.com/kojix2/ruby-biosyntax/actions/workflows/ci.yml/badge.svg)](https://github.com/kojix2/ruby-biosyntax/actions/workflows/ci.yml)
[![Lines of Code](https://img.shields.io/endpoint?url=https%3A%2F%2Ftokei.kojix2.net%2Fbadge%2Fgithub%2Fkojix2%2Fruby-biosyntax%2Flines)](https://tokei.kojix2.net/github/kojix2/ruby-biosyntax)


:dna: [bioSyntax](https://github.com/bioSyntax/bioSyntax) - Syntax highlighting for biological data formats - for Ruby.

Powered by [libbiosyntax](https://github.com/kojix2/libbiosyntax).

## Installation

```sh
gem install biosyntax
```

## ANSI coloring

```ruby
require "biosyntax"

hl = BioSyntax.fastq

File.foreach("reads.fastq", chomp: false) do |line|
  print hl.colorize(line)
end
```

`colorize` returns a string with ANSI SGR escape sequences using the built-in
`libbiosyntax` colors.

`Highlighter` is stateful. Reuse one highlighter for one input stream,
especially for FASTQ and WIG. Use `reset` before starting another stream with
the same object.

```ruby
hl = BioSyntax.fastq
# process one file...
hl.reset
# process another file...
```

## Highlight spans

```ruby
require "biosyntax"

hl = BioSyntax.vcf
line = "chr1\t42\trs1\tA\tT\t99\tPASS\tDP=10;AF=0.5\n"

spans = hl.highlight(line)

spans.each do |span|
  puts [span.start, span.end, span.kind.name, span.scope].join("\t")
end
```

A span uses byte offsets into the input line:

```ruby
span.start   # byte offset at the start of the highlighted range
span.end     # byte offset just after the highlighted range
span.length  # byte length
span.kind    # BioSyntax::Kind
span.scope   # e.g. "biosyntax.chrom"
```

## Formats and metadata

Create highlighters with `BioSyntax.<format>` or `BioSyntax[format]`.
Hyphenated format names use underscores for factory methods.

```ruby
BioSyntax.vcf
BioSyntax.fastq
BioSyntax.fasta_nt
BioSyntax[:"fasta-nt"]
BioSyntax["bam"]       # canonical format is :sam
```

Useful metadata:

```ruby
BioSyntax::FORMAT_NAMES # array of canonical format names
BioSyntax::FORMATS      # { name => BioSyntax::Format }
BioSyntax::KIND_NAMES   # array of kind names
BioSyntax::KINDS        # { name => BioSyntax::Kind }
BioSyntax::SCOPES       # { scope => [BioSyntax::Kind, ...] }

BioSyntax::Format::VCF
BioSyntax::Kind::CHROM

BioSyntax.format_supported?(:vcf)  # true
BioSyntax.format_name(:bam)        # :sam
BioSyntax.guess_format("a.vcf.gz") # :vcf
```

The metadata is generated from `libbiosyntax` at load time. The Ruby side does
not maintain a separate hand-written table of formats or kinds.

## Examples

This gem does not install a CLI. See `examples/` for small scripts:

```sh
ruby examples/bcat.rb sample.vcf
ruby examples/bcat.rb -l fastq reads.fastq
ruby examples/bcat.rb -l
ruby examples/inspect_spans.rb sample.vcf
```

`bcat.rb` guesses the format from the file name when possible. Use `-l` /
`--language` to pass a format explicitly. Calling `-l` without an argument
prints the supported format names.

## Development tasks

```sh
bundle exec rake -T
bundle exec rake test
bundle exec rake build
bundle exec yard doc
```

The native extension is built with `rake-compiler`. Temporary build products
are written under `tmp/`, and the compiled extension is copied to
`lib/biosyntax/`.

## Updating vendored libbiosyntax

This gem vendors the C source of `libbiosyntax` and builds it into the Ruby extension.
It does not require a system `libbiosyntax` shared library.
The vendored C source lives under:

```text
ext/biosyntax/biosyntax.c
ext/biosyntax/biosyntax.h
```

When `libbiosyntax` is updated, refresh the vendored files and run the test suite:

```sh
bundle exec rake update:libbiosyntax
bundle exec rake
```

## License

`biosyntax` vendors `libbiosyntax`, which is licensed under the GNU General
Public License version 3 only. This gem is therefore distributed under
`GPL-3.0-only`. See `LICENSE.md`.

This project is inspired by the original bioSyntax project:
<https://github.com/bioSyntax/bioSyntax>
