require_relative 'lib/biosyntax/version'

Gem::Specification.new do |spec|
  spec.name          = 'biosyntax'
  spec.version       = BioSyntax::VERSION
  spec.summary       = 'Ruby native binding for libbiosyntax'
  spec.homepage      = 'https://github.com/kojix2/ruby-biosyntax'
  spec.license       = 'MIT'

  spec.author        = 'kojix2'
  spec.email         = '2xijok@gmail.com'

  spec.required_ruby_version = '>= 3.1'

  spec.files         = Dir['*.{md,txt}', 'exe/*', 'ext/**/*.{c,h,rb}', 'lib/**/*.rb']
  spec.bindir        = 'exe'
  spec.executables   = ['biocat']
  spec.extensions    = ['ext/biosyntax/extconf.rb']
  spec.require_path  = 'lib'
end
