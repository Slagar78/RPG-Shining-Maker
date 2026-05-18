# -*- encoding: utf-8 -*-
# stub: ocran 1.4.3 x64-mingw-ucrt lib

Gem::Specification.new do |s|
  s.name = "ocran".freeze
  s.version = "1.4.3".freeze
  s.platform = "x64-mingw-ucrt".freeze

  s.required_rubygems_version = Gem::Requirement.new(">= 0".freeze) if s.respond_to? :required_rubygems_version=
  s.metadata = { "changelog_uri" => "https://github.com/largo/ocran/CHANGELOG.txt", "homepage_uri" => "https://github.com/largo/ocran", "source_code_uri" => "https://github.com/largo/ocran" } if s.respond_to? :metadata=
  s.require_paths = ["lib".freeze]
  s.authors = ["Andi Idogawa".freeze, "shinokaro".freeze, "Lars Christensen".freeze]
  s.bindir = "exe".freeze
  s.date = "1980-01-02"
  s.description = "OCRAN (One-Click Ruby Application Next) packages Ruby applications for\ndistribution. It bundles your script, the Ruby interpreter, gems, and native\nlibraries into a self-contained artifact that runs without requiring Ruby to\nbe installed on the target machine.\n\nThree output formats are supported on all platforms:\n- Self-extracting executable (.exe on Windows, native binary on Linux/macOS)\n- Directory with a launch script (--output-dir)\n- Zip archive with a launch script (--output-zip)\n\nThis is a fork of OCRA maintained for Ruby 3.2+ compatibility.\nMigration guide: replace OCRA_EXECUTABLE with OCRAN_EXECUTABLE in your code.\n\nUsage:\n  ocran helloworld.rb          # builds helloworld.exe / helloworld\n  ocran --output-dir out/ app.rb\n  ocran --output-zip app.zip app.rb\n\nSee readme at https://github.com/largo/ocran\nReport problems at https://github.com/largo/ocran/issues\n".freeze
  s.email = ["andi@idogawa.com".freeze]
  s.executables = ["ocran".freeze]
  s.files = ["exe/ocran".freeze]
  s.homepage = "https://github.com/largo/ocran".freeze
  s.licenses = ["MIT".freeze]
  s.required_ruby_version = Gem::Requirement.new(">= 3.2.0".freeze)
  s.rubygems_version = "4.0.6".freeze
  s.summary = "OCRAN (One-Click Ruby Application Next) packages Ruby applications for distribution on Windows, Linux, and macOS.".freeze

  s.installed_by_version = "4.0.6".freeze

  s.specification_version = 4

  s.add_runtime_dependency(%q<fiddle>.freeze, ["~> 1.0".freeze])
end
