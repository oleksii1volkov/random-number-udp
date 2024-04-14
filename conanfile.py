from conan import ConanFile

class UDPConan(ConanFile):
    settings = "os", "compiler", "build_type", "arch"
    generators = "CMakeToolchain", "CMakeDeps"

    def requirements(self):
        self.requires("boost/1.84.0")
        self.requires("protobuf/3.21.12")

    def configure(self):
        # enabled_modules = ["container", "context", "coroutine", "exception", "system"]

        # for option, value in self.options["boost"].items():
            # if option.startswith("without_"):
            #     module_name = option[8:]
            #     self.options["boost"][option] = not (module_name in enabled_modules)

        self.options["boost"].without_container = False
        self.options["boost"].without_context = False
        self.options["boost"].without_coroutine = False
        self.options["boost"].without_exception = False
        self.options["boost"].without_program_options = False
        self.options["boost"].without_system = False

        self.options["boost"].without_log = True
        self.options["boost"].without_url = True
        self.options["boost"].without_json = True
        self.options["boost"].without_math = True
        self.options["boost"].without_test = True
        self.options["boost"].without_wave = True
        self.options["boost"].without_fiber = True
        self.options["boost"].without_graph = True
        self.options["boost"].without_regex = True
        self.options["boost"].without_timer = True
        self.options["boost"].without_atomic = True
        self.options["boost"].without_chrono = True
        self.options["boost"].without_locale = True
        self.options["boost"].without_nowide = True
        self.options["boost"].without_random = True
        self.options["boost"].without_thread = True
        self.options["boost"].without_headers = True
        self.options["boost"].without_contract = True
        self.options["boost"].without_math_c99 = True
        self.options["boost"].without_math_tr1 = True
        self.options["boost"].without_date_time = True
        self.options["boost"].without_iostreams = True
        self.options["boost"].without_log_setup = True
        self.options["boost"].without_math_c99f = True
        self.options["boost"].without_math_c99l = True
        self.options["boost"].without_math_tr1f = True
        self.options["boost"].without_math_tr1l = True
        self.options["boost"].without_fiber_numa = True
        self.options["boost"].without_filesystem = True
        self.options["boost"].without_stacktrace = True
        self.options["boost"].without__boost_cmake = True
        self.options["boost"].without_type_erasure = True
        self.options["boost"].without_serialization = True
        self.options["boost"].without_wserialization = True
        self.options["boost"].without_dynamic_linking = True
        self.options["boost"].without_stacktrace_noop = True
        self.options["boost"].without_prg_exec_monitor = True
        self.options["boost"].without_stacktrace_basic = True
        self.options["boost"].without_test_exec_monitor = True
        self.options["boost"].without_disable_autolinking = True
        self.options["boost"].without_unit_test_framework = True
        self.options["boost"].without_stacktrace_addr2line = True
        self.options["boost"].without_stacktrace_backtrace = True
        self.options["boost"].without_diagnostic_definitions = True
