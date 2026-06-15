require 'biosyntax'

highlighter = BioSyntax.vcf
line = "chr1\t42\trs1\tA\tG\t99\tPASS\tDP=10;AF=0.5\n"

highlighter.highlight(line).each do |span|
  text = line.byteslice(span.start, span.length)
  puts [span.start, span.end, span.kind.name, text.inspect].join("\t")
end
