require 'mkmf'
require 'rbconfig'

abort 'missing stdint.h' unless have_header('stdint.h')

$defs << '-DBIOSYN_STATIC'

cc = RbConfig::CONFIG['CC'].to_s
$CFLAGS << ' -std=c99' unless cc =~ /\bcl(\.exe)?\b/i

$objs = ["biosyntax_ext.#{$OBJEXT}", "biosyntax.#{$OBJEXT}"]

create_makefile('biosyntax/biosyntax_ext')
