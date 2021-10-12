import platform

from conans import ConanFile
from conan.tools.cmake import CMakeToolchain, CMake
from conan.tools.layout import cmake_layout
from os import path


class Pkg(ConanFile):
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
        "io_zisraw": [True, False]
    }
    default_options = {
        "shared": True,
        "with_qt5": False,
        "with_cli": True,
        "with_python": True,
        "debug_log": False,
        "io_zisraw": True
    }

    url = "https://github.com/DerOrfa/isis"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = "CMakeLists.txt", "COPYING.txt", "isis/*", "tests/*", "io_plugins/*", "cmake/*", "tools/*"
    generators = "cmake_paths", "cmake_find_package"


    requires = [
        "boost/[>1.30]",
        "jsoncpp/[~=1]",
        ("fftw/[~=3]", "private"),
        ("lzma_sdk/[~=9]", "private"),
        ("openjpeg/[~=2]", "private"),
        ("libpng/[>1.2]", "private")
    ]

    # def configure(self):
    #     self.options['fftw'].precision = "single"

    def requirements(self):
        if self.options.with_qt5:
            self.requires("qt/[~=5]")
        if self.options.with_python:
            self.requires("pybind11/[>2.6.0]", "private")
        if self.options.io_zisraw:
            self.requires("jxrlib/cci.20170615", "private")

    def config_options(self):
        None

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self)
        self.python_path = path.join("lib", "python3", "dist-packages")

        for var in ["USE_CONAN", "ISIS_RUNTIME_LOG", "ISIS_IOPLUGIN_COMP_LZMA", "ISIS_IOPLUGIN_PNG"]:
            tc.variables[var] = "ON"

        tc.variables["ISIS_QT5"] = "ON" if self.options.with_qt5 else "OFF"
        tc.variables["ISIS_BUILD_TOOLS"] = "ON" if self.options.with_cli else "OFF"
        tc.variables["ISIS_PYTHON"] = "ON" if self.options.with_python else "OFF"
        tc.variables["ISIS_DEBUG_LOG"] = "ON" if self.options.debug_log else "OFF"
        tc.variables["ISIS_IOPLUGIN_ZISRAW"] = "ON" if self.options.io_zisraw else "OFF"

        tc.variables["PYTHON_MODULE_PATH"] = self.python_path
        tc.variables["PYTHON_MODULE_PATH_ARCH"] = self.python_path
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
        if (platform.system() == "Darwin"):
            self.env_info.DYLD_LIBRARY_PATH.append(path.join(self.package_folder, "lib"))
        elif (platform.system() == "Linux"):
            self.env_info.LD_LIBRARY_PATH.append(path.join(self.package_folder, "lib"))

        if (self.options.with_python):
            self.env_info.PYTHONPATH.append(path.join(self.package_folder, "lib", "python3", "dist-packages"))
        if (self.options.with_cli):
            self.env_info.path.append(path.join(self.package_folder, "bin"))

        # self.cpp_info.includedirs=
        # self.cpp_info.libs = ["hello"]
