from conans import ConanFile, CMake, tools

class TaraxaConan(ConanFile):
    name = "taraxa-node"
    version = "0.1"
    description = "Taraxa is a Practical Byzantine Fault Tolerance blockchain."
    topics = ("blockchain", "taraxa", "crypto")
    url = "https://github.com/Taraxa-project/taraxa-node"
    homepage = "https://www.taraxa.io"
    license = "MIT"

    settings = "os", "compiler", "build_type", "arch"
    generators = "cmake"

    def requirements(self):
        self.requires("mpfr/4.1.0")
        self.requires("boost/1.79.0")
        self.requires("cppcheck/2.7.5")
        self.requires("openssl/1.1.1s")
        self.requires("cryptopp/8.7.0")
        self.requires("gtest/1.12.1")
        self.requires("lz4/1.9.4")
        self.requires("rocksdb/6.29.5")
        self.requires("gmp/6.2.1")
        self.requires("libjson-rpc-cpp/1.3.0@bincrafters/stable")

    def _configure_boost_libs(self):
        self.options["boost"].without_atomic = False
        self.options["boost"].without_chrono = False
        self.options["boost"].without_container = False
        self.options["boost"].without_context = True
        self.options["boost"].without_contract = True
        self.options["boost"].without_coroutine = True
        self.options["boost"].without_date_time = False
        self.options["boost"].without_exception = False
        self.options["boost"].without_fiber = True
        self.options["boost"].without_filesystem = False
        self.options["boost"].without_graph = True
        self.options["boost"].without_graph_parallel = True
        self.options["boost"].without_iostreams = True
        self.options["boost"].without_locale = False
        self.options["boost"].without_log = False
        self.options["boost"].without_math = False
        self.options["boost"].without_mpi = True
        self.options["boost"].without_program_options = False
        self.options["boost"].without_python = True
        self.options["boost"].without_random = False
        self.options["boost"].without_regex = False
        self.options["boost"].without_serialization = False
        self.options["boost"].without_stacktrace = True
        self.options["boost"].without_system = False
        self.options["boost"].without_test = True
        self.options["boost"].without_thread = False
        self.options["boost"].without_timer = True
        self.options["boost"].without_type_erasure = True
        self.options["boost"].without_wave = True

    def configure(self):
        # Configure boost
        self._configure_boost_libs()
        # Configure gtest
        self.options["gtest"].build_gmock = False
        # this links cppcheck to prce library
        self.options["cppcheck"].have_rules = False
        self.options["rocksdb"].use_rtti = True
        self.options["rocksdb"].with_lz4 = True
        if (self.settings.arch == "x86_64"):
            self.options["rocksdb"].enable_sse = "avx2"
        self.options["libjson-rpc-cpp"].shared = False
        # mpir is required by cppcheck and it causing gmp confict
        self.options["mpir"].enable_gmpcompat = False

        # mpir is z3 dependency and it couldn't be built for arm
        if (self.settings.arch == "armv8"):
            self.options["cppcheck"].with_z3 = False

    def _configure_cmake(self):
        cmake = CMake(self)
        # set find path to clang utils dowloaded by that script
        cmake.configure()
        return cmake

    def build(self):
        cmake = self._configure_cmake()
        cmake.build()

    def package(self):
        cmake = self._configure_cmake()
        cmake.install()

    def package_info(self):
        self.cpp_info.libs = ["taraxa-node"]
