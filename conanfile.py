import platform

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake
from os import path

#conan build . -o io_sftp=False --build=missing --output-folder=build

class isis(ConanFile):
	name = "isis"
	version = "0.9.0-dev"
	license = "GPL-3.0-or-later"
	description = "The ISIS project aims to provide a framework to access a large variety of image processing libraries written in different programming languages and environments."

	# Binary configuration
	settings = "os", "compiler", "build_type", "arch"
	options = {
		"shared": [True, False],
		"debug_log": [True, False],
		"with_cli": [True, False],
		"with_qt5": [True, False],
		"with_python": [None, "system", "3.7", "3.8", "3.9", "3.10"],
		"io_zisraw": [True, False],
		"io_sftp": [True, False],
		"io_png": [True, False],
		"testing": [True, False]
	}
	default_options = {
		"shared": True,
		"with_qt5": False,
		"with_cli": True,
		"with_python": None,
		"debug_log": False,
		"io_zisraw": True,
		"io_sftp": True,
		"io_png": True,
		"testing": False,
		"cmake*:with_openssl": False,  # in case people got cmake from conan, we need to avoid openssl
		"fftw*:precision": "single",
		"fftw*:threads": True,
		"boost*:lzma": True,
		"boost*:zstd": False
	}
	boost_without = [
		"atomic", "chrono", "container", "context", "contract", "coroutine", "date_time", "fiber", "filesystem",
		"graph", "graph_parallel", "json", "locale", "log", "math", "mpi", "nowide", "program_options", "python",
		"serialization", "stacktrace", "thread", "timer", "type_erasure", "wave"
	]
	default_options.update({f"boost*:without_{n}": True for n in boost_without})

	url = "https://github.com/DerOrfa/isis"

	# Sources are located in the same place as this recipe, copy them to the recipe
	exports_sources = ["CMakeLists.txt", "COPYING.txt", "isis/*", "io_plugins/*", "cmake/*", "tools/*"]
	generators = "CMakeDeps"

	def build_requirements(self):
		if self.options.with_python:
			if self.options.with_python != "system": #assume the given value is the conan-package version for python
				self.tool_requires(f'cpython/[~{self.options.with_python}]')

	def config_options(self):
		if self.options.testing:
			self.exports_sources.append("tests/*")

	def requirements(self):
		# make everything statically linked into the exe (or the shared lib) => not needed by the consumer
		for dep in ["jsoncpp/[~1]", "fftw/[~3]", "openjpeg/[~2]", "eigen/[~3]"]:
			self.requires(dep)

		if self.options.with_cli:
			self.requires("muparser/[~2]")

		if self.options.with_qt5:
			self.requires("qt/[~5]")
			self.options['qt'].openssl = False
			self.options['qt'].with_mysql = False
			self.options['qt'].with_sqlite3 = False
			self.options['qt'].with_odbc = False
			self.options['qt'].with_pq = False
			self.options['qt'].qtwayland = bool(self.settings.os == "Linux")

		if self.options.with_python:
			self.requires("pybind11/[>2.9.0]", libs=False)  # pybind is header only

		if self.options.io_zisraw:
			self.requires("jxrlib/cci.20170615")

		if self.options.io_sftp:
			self.requires("libssh2/[~1]")
			# self.options['libssh2'].with_zlib = False  # prevent imported zlib from conflicting with png

		if self.options.io_png:
			self.requires("libpng/[>1.2]")

		self.requires("boost/[>1.75]", libs=True)
		self.tool_requires("cmake/[>3.15]")

	def generate(self):
		self.python_path = path.join("lib", "python3", "dist-packages")
		tc = CMakeToolchain(self)

		for var in ["ISIS_RUNTIME_LOG", "ISIS_USE_FFTW"]:
			tc.variables[var] = True

		# io plugins
		tc.variables["ISIS_IOPLUGIN_ZISRAW"] = bool(self.options.io_zisraw)
		tc.variables["ISIS_IOPLUGIN_SFTP"] = bool(self.options.io_sftp)
		tc.variables["ISIS_IOPLUGIN_PNG"] = bool(self.options.io_png)
		tc.variables["ISIS_IOPLUGIN_NULL"] = bool(self.options.testing)

		# python adapter
		tc.variables["ISIS_PYTHON"] = bool(self.options.with_python)
		if self.options.with_python:
			tc.variables["USE_SYSTEM_PYBIND"] = True
			tc.variables["PYTHON_MODULE_PATH"] = self.python_path
			tc.variables["PYTHON_MODULE_PATH_ARCH"] = self.python_path

		# other
		tc.variables["BUILD_TESTING"] = bool(self.options.testing)
		# tc.variables["BOOST_IS_SHARED"] = bool(self.options['boost'].shared)
		tc.variables["ISIS_QT5"] = bool(self.options.with_qt5)
		tc.variables["ISIS_BUILD_TOOLS"] = bool(self.options.with_cli)
		tc.variables["ISIS_CALC"] = bool(self.options.with_cli)
		tc.variables["ISIS_DEBUG_LOG"] = bool(self.options.debug_log)
		tc.generate()

	def build(self):
		cmake = CMake(self)
		cmake.configure()
		# cmake.verbose = True
		cmake.build()

	def package(self):
		cmake = CMake(self)
		cmake.install()

	def package_info(self):
		self.env_info.ISIS_PLUGIN_PATH = path.join(self.package_folder, "lib", "isis", "plugins")
		if platform.system() == "Darwin":
			self.env_info.DYLD_LIBRARY_PATH.append(path.join(self.package_folder, "lib"))
		elif platform.system() == "Linux":
			self.env_info.LD_LIBRARY_PATH.append(path.join(self.package_folder, "lib"))

		if self.options.with_python:
			self.env_info.PYTHONPATH.append(path.join(self.package_folder, "lib", "python3", "dist-packages"))
		if self.options.with_cli:
			self.env_info.path.append(path.join(self.package_folder, "bin"))
