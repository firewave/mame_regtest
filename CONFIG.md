##### Input options
* _executable_ - the name or full path of the executable you want to use
* _listxml_file_ - the name or full path of the alernative -listxml output you want to use

##### Environment options
* _rompath_ - the rom path(s) passed to the -rp parameter of the given executable
* _hashpath_ - the hash path(s) used by the given executable (MESS only)
* _osdprocessors_ - sets the OSDPROCESSOR environment variable

##### Processing options
* _use_devices_ - controls the usage of device files (must be named **mrt_\<system\>.xml** and placed in the working directory / MESS only)
* _use_nonrunnable_ - controls the processing of entries in -listxml marked as runnable=“no”
* _xpath_expr_ - the XPath expression applied on the -listxml output to select the driver, that should be processed (has to start with DRIVER_ROOT macro to be application-independent / only XPath 1.0 is supported)
* _device_file_ - use a fixed list of devices instead of the automatic detection based on driver name and device type
* _use_isbios_ - controls the testing of MAME bios sets
* _skip_mandatory_ - controls the testing of drivers with mandatory devices, when devices are specified (MESS only)
* _test_softrest_ - controls the execution of a softreset on each driver (only works with “use_debug”)

##### Command-line options
* _str_ - the value passed to the -str parameter of the executable
* _use_bios_ - controls the processing of all available bioses for a driver
* _use_sound_ - controls the sound output
* _use_ramsize_ - controls the processing of all available ramsizes of a driver (MESS only)
* _use_autosave_ - controls the usage of -autosave of all drivers, that support it (runs each driver twice - once for initial saving and once or loading)
* _use_throttle_ - controls the usage of throttling
* _use_debug_ - controls the usage of the internal debugger (will automatically pass a script with the “go” command, so it will continue)
* _use_dipswitches_ - controls the processing of all available dip switches for a driver (only working with 0.136 and up)
* _use_configurations_ - controls the processing of all available configurations for a driver (only working with 0.136 and up)
* _use_softwarelist_ - controls the usage of software lists (MESS only / only working with 0.138 and up)
* _write_mng_ - controls the writing of MNGs for each driver
* _write_avi_ - controls the writing of AVIs for each driver
* _write_wav_ - controls the writing of WAVs for each driver
* _additional_options_ - specifies additional options for the command-line

##### Output options
* _output_folder_ - specify the folder the mame_regtest output gets written to
* _store_output_ - controls if the output files of a call are kept
* _clear_output_folder_ - controls the clearing the output folder on start-up
* _print_xpath_results_ - controls the creatíon of a XML containg the results of the XPath expression

##### Valgrind options
* _use_valgrind_ - will run each call in valgrind (UNIX only)
* _valgrind_binary_ - the location of the valgrind binary (UNIX only)
* _valgrind_parameters_ - the parameters, that will be passed to the valgrind calls (–log-file is added by mame_regtest / UNIX only)

##### Additional options
* _test_createconfig_ - controls the configuration creation in the output folder (the file is not used as configuration for the tests)
* _verbose_ - controls the amount of command-line output

##### Hacks
* _hack_debug_ - indicates, that given executable is a debug version for versions, that don't have the debug attribute in the -listxml output (necessary until 0.114)
* _hack_ftr_ - forces usage of old -ftr parameter and the old snapshot locations (necessary until 0.113)
* _hack_biospath_ - forces the usage of the old -bp parameter (MESS only / necessary until 0.111)
* _hack_mngwrite_ - forces the usage of the old MNG locations (necessary until 0.???)
* _hack_pinmame_ - works around issues in the -listxml output of PinMAME (necessary until 0.???)