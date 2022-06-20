import platform

from conan import ConanFile
from conan.tools.cmake import CMakeToolchain, CMakeDeps, CMake, cmake_layout
from os import path


class isis(ConanFile):
    name = "isis"
    version = "0.8.0"

    # Binary configuration
    settings = "os", "compiler", "build_type", "arch"
    options = {
        "shared": [True, False],
        "debug_log": [True, False],
        "with_cli": [True, False],
        "with_qt5": [True, False],
        "with_python": [True, False],
        "io_zisraw": [True, False],
        "io_sftp": [True, False],
        "io_png": [True, False],
        "testing": [True, False]
    }
    default_options = {
        "shared": True,
        "with_qt5": False,
        "with_cli": True,
        "with_python": True,
        "debug_log": False,
        "io_zisraw": True,
        "io_sftp": True,
        "io_png": True,
        "testing": False,
        "fftw:precision": "single",
        "boost:lzma": True,
        "boost:zstd": False,
        "boost:i18n_backend": None
    }
    boost_without = [
        "atomic", "chrono","container","context","contract","coroutine","date_time","fiber","filesystem", "graph",
        "graph_parallel","json","locale","log","math","mpi","nowide","program_options","python", "serialization",
        "stacktrace","thread","timer","type_erasure","wave"
    ]
    default_options.update({"boost:without_{}".format(_name): True for _name in boost_without})

    url = "https://github.com/DerOrfa/isis"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = ["CMakeLists.txt", "COPYING.txt", "isis/*", "io_plugins/*", "cmake/*", "tools/*"]
    generators = "CMakeDeps", "CMakeToolchain"


    requires = [
        "boost/[>1.75]",
        ("jsoncpp/[~=1]", "private"),
        ("fftw/[~=3]", "private"),
        ("openjpeg/[~=2]", "private"),
        ("gsl/[~=2.7]","private"),
        ("eigen/[~=3]", "private")
    ]

    def config_options(self):
        if(self.options.testing):
            self.exports_sources.append("tests/*")
        if self.options.io_sftp:
            self.options['libssh2'].with_zlib = False #prevent imported zlib from conflicting with png

    def requirements(self):
        if self.options.with_cli:    self.requires("muparser/[~=2]", "private")
        if self.options.with_qt5:    self.requires("qt/[~=5]")
        if self.options.with_python: self.requires("pybind11/[>2.9.0]", "private")
        if self.options.io_zisraw:   self.requires("jxrlib/cci.20170615", "private")
        if self.options.io_sftp:     self.requires("libssh2/[~=1]", "private")
        if self.options.io_png:      self.requires("libpng/[>1.2]", "private")

        if platform.system() == "Linux":
            self.requires("ncurses/[~=6]", "private")

    def generate(self):
        self.python_path = path.join("lib", "python3", "dist-packages")
        tc = CMakeToolchain(self)

        for var in ["ISIS_RUNTIME_LOG", "ISIS_USE_FFTW"]:
            tc.variables[var] = True

        #io plugins
        tc.variables["ISIS_IOPLUGIN_ZISRAW"] = bool(self.options.io_zisraw)
        tc.variables["ISIS_IOPLUGIN_SFTP"] = bool(self.options.io_sftp)
        tc.variables["ISIS_IOPLUGIN_PNG"] = bool(self.options.io_png)

        #python adapter
        tc.variables["ISIS_PYTHON"] = bool(self.options.with_python)
        if self.options.with_python:
            tc.variables["USE_SYSTEM_PYBIND"] = True
            tc.variables["PYTHON_MODULE_PATH"] = self.python_path
            tc.variables["PYTHON_MODULE_PATH_ARCH"] = self.python_path

        #other
        tc.variables["BUILD_TESTING"] =    bool(self.options.testing)
        tc.variables["ISIS_QT5"] =         bool(self.options.with_qt5)
        tc.variables["ISIS_BUILD_TOOLS"] = bool(self.options.with_cli)
        tc.variables["ISIS_CALC"] =        bool(self.options.with_cli)
        tc.variables["ISIS_DEBUG_LOG"] =   bool(self.options.debug_log)

        tc.generate()

        deps = CMakeDeps(self)
        # This writes all the config files (xxx-config.cmake)
        deps.generate()

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
        if (platform.system() == "Darwin"):
            self.env_info.DYLD_LIBRARY_PATH.append(path.join(self.package_folder, "lib"))
        elif (platform.system() == "Linux"):
            self.env_info.LD_LIBRARY_PATH.append(path.join(self.package_folder, "lib"))

        if (self.options.with_python):
            self.env_info.PYTHONPATH.append(path.join(self.package_folder, "lib", "python3", "dist-packages"))
        if (self.options.with_cli):
            self.env_info.path.append(path.join(self.package_folder, "bin"))
