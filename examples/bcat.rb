#!/usr/bin/env ruby
# frozen_string_literal: true

require 'optparse'
require 'biosyntax'

options = { format: nil }

parser = OptionParser.new do |opts|
  opts.banner = 'usage: bcat [options] [FILE ...]'

  opts.on('-f', '--format FORMAT', 'Highlight as FORMAT') do |format|
    options[:format] = format
  end

  opts.on('-h', '--help', 'Print this help') do
    puts opts
    exit
  end
end

begin
  if ARGV == ['-l'] || ARGV == ['--language']
    puts BioSyntax::FORMAT_NAMES.join("\n")
    exit
  end

  parser.parse!

  paths = ARGV.empty? ? ['-'] : ARGV

  paths.each do |path|
    format = options[:format]
    format ||= BioSyntax.guess_format(path) unless path == '-'

    highlighter = BioSyntax[format] if format
    input = path == '-' ? $stdin : File.open(path, 'rb')

    begin
      input.each_line do |line|
        output = highlighter ? highlighter.colorize(line) : line
        print output
      end
    ensure
      input.close unless path == '-'
    end
  end
rescue OptionParser::ParseError, BioSyntax::Error => e
  warn "bcat: #{e.message}"
  warn parser
  exit 2
rescue SystemCallError => e
  warn "bcat: #{e.message}"
  exit 1
end
