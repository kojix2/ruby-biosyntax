require 'biosyntax'

path = ARGV.fetch(0) do
  warn 'usage: ruby examples/inspect_spans.rb FILE [FORMAT]'
  exit 2
end

format = ARGV[1] || BioSyntax.guess_format(path)
unless format
  warn "could not guess format for #{path.inspect}; pass FORMAT explicitly"
  exit 2
end

highlighter = BioSyntax[format]

File.foreach(path, chomp: false).with_index(1) do |line, line_no|
  highlighter.highlight(line).each do |span|
    puts [line_no, span.start, span.end, span.kind.name, span.scope].join("\t")
  end
end
