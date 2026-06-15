require 'bundler/gem_tasks'
require 'open-uri'
require 'rake/clean'
require 'rake/extensiontask'
require 'rake/testtask'

gemspec = Bundler.load_gemspec('biosyntax.gemspec')

Rake::ExtensionTask.new('biosyntax_ext', gemspec) do |ext|
  ext.ext_dir = 'ext/biosyntax'
  ext.lib_dir = 'lib/biosyntax'
end

Rake::TestTask.new do |test|
  test.libs << 'lib'
  test.pattern = 'test/test_*.rb'
  test.warning = true
end

Rake::Task[:test].enhance [:compile]

namespace :update do
  desc 'Update vendored libbiosyntax'
  task :libbiosyntax do
    base_url = 'https://raw.githubusercontent.com/kojix2/libbiosyntax/main'

    {
      'include/biosyntax.h' => 'ext/biosyntax/biosyntax.h',
      'src/biosyntax.c' => 'ext/biosyntax/biosyntax.c'
    }.each do |source, target|
      url = "#{base_url}/#{source}"
      File.write(target, URI.open(url, &:read))
      puts "updated #{target}"
    end
  end
end

task default: :test
