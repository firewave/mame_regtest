#### mame_regtest.xml

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
* _test_softreset_ - controls the execution of a softreset on each driver (only works with “use_debug”)
* _no_execution_ - 

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
    * _1_ - run all entries of the software list (also uses all parts)
    * _2_ - only run the first entry of the software list
    * _3_ - only run the first entry for the available slots (behaves like _2_ when no slots exist)
    * _4_ - only run the first entry for the available interfaces
* _use_slots_ - 
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
* _verbose_ - controls the amount of command-line output
* _use_gdb_ - 
* _test_frontend_ -

##### Hacks
* _hack_debug_ - indicates, that given executable is a debug version for versions, that don't have the debug attribute in the -listxml output (necessary until 0.114)
* _hack_ftr_ - forces usage of old -ftr parameter and the old snapshot locations (necessary until 0.113)
* _hack_biospath_ - forces the usage of the old -bp parameter (MESS only / necessary until 0.111)
* _hack_mngwrite_ - forces the usage of the old MNG locations (necessary until 0.???)
* _hack_pinmame_ - works around issues in the -listxml output of PinMAME (necessary until 0.???)
* _hack_softwarelist_ - 
* _hack_nosound_ - 
* _hack_driver_root_ - forces usage of old MAME/MESS-specific `-listxml` node names (necessary until 0.???)
* _hack_str_snapname_ - omits `-snapname` parameter since `-str` hard-coded `final.png` name (necessary until 0.236)

#### create_report.xml

* _report_type_ - The type of report to generate
    * 0 - result report (default)
    * 1 - comparison report
    * 2 - speed comparison report
    * 3 - no longer used
    * 4 - speed report 
* _xml_folder_ -
* _recursive_ -
* _output_file_ -
* _use_markdown_ - Generate the report in Markdown format (only for report_type 0)
* _ignore_exitcode_4_ -
* _show_memleaks_ -
* _show_clipped_ -
* _unexpected_stderr_ - Treat any output in stderr as error (only for report_type 0)
* _group_data_ -
* _print_stderr_ -
* _compare_folder_ -
* _print_stdout_ -
* _speed_threshold_ -