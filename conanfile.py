import platform

from conans import ConanFile
from conans.model.options import PackageOption
from conan.tools.cmake import CMakeToolchain, CMake
from conan.tools.layout import cmake_layout
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
        "testing": [True, False]
    }
    default_options = {
        "shared": True,
        "with_qt5": False,
        "with_cli": True,
        "with_python": True,
        "debug_log": False,
        "io_zisraw": True,
        "testing": False
    }

    url = "https://github.com/DerOrfa/isis"

    # Sources are located in the same place as this recipe, copy them to the recipe
    exports_sources = ["CMakeLists.txt", "COPYING.txt", "isis/*", "io_plugins/*", "cmake/*", "tools/*"]
    generators = "cmake_paths", "cmake_find_package", "CMakeToolchain"


    requires = [
        "boost/[>1.30]",
        "jsoncpp/[~=1]",
        ("fftw/[~=3]", "private"),
        ("lzma_sdk/[~=9]", "private"),
        ("openjpeg/[~=2]", "private"),
        ("libpng/[>1.2]", "private")
    ]

    def configure(self):
        if(self.options.testing):
            self.exports_sources.append("tests/*")
        self.options['fftw'].precision = "single"

    def requirements(self):
        if self.options.with_qt5:
            self.requires("qt/[~=5]")
        if self.options.with_python:
            self.requires("pybind11/[>2.6.0]", "private")
        if self.options.io_zisraw:
            self.requires("jxrlib/cci.20170615", "private")

    def layout(self):
        cmake_layout(self)

    def generate(self):
        tc = CMakeToolchain(self) # https://docs.conan.io/en/latest/reference/conanfile/tools/cmake/cmaketoolchain.html?highlight=cmaketoolchain
        self.python_path = path.join("lib", "python3", "dist-packages")

        for var in ["USE_CONAN", "ISIS_RUNTIME_LOG", "ISIS_IOPLUGIN_COMP_LZMA", "ISIS_IOPLUGIN_PNG"]:
            tc.variables[var] = True

        tc.variables["ISIS_QT5"] = bool(self.options.with_qt5)
        tc.variables["ISIS_BUILD_TOOLS"] = bool(self.options.with_cli)
        tc.variables["ISIS_PYTHON"] = bool(self.options.with_python)
        tc.variables["ISIS_DEBUG_LOG"] = bool(self.options.debug_log)
        tc.variables["ISIS_IOPLUGIN_ZISRAW"] = bool(self.options.io_zisraw)
        tc.variables["BUILD_TESTING"] = bool(self.options.testing)

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
