# mame_regtest

**mame_regtest** is a tool designed to mass-test drivers of MAME-based applications like MAME, MESS and PinMAME.

The latest stable version is **0.72** ([download](http://www.breaken.de/mame_regtest/mame_regtest_072.zip)|[libxml2.dll](http://www.breaken.de/mame_regtest/libxml2_dll-2.6.27.zip)|[zlib1.dll](http://www.breaken.de/mame_regtest/zlib1_dll-1.2.3.zip)) which includes the new XML-based configuration instead of command-line parameters to make the usage easier and XPath support to select drivers for testing as well as the optional XML output with all the information about a driver run.

**Note on XPath:** You have to complete the ”/mame/game” or ”/mess/machine” expression with one, that returns a valid list of “game” or “machine” nodes. You have to use the placeholder **DRIVER_ROOT** for the application-specific beginning. See some examples for the removed options in the sample mame_regtest.xml.

**Note on multiple configurations:** You have to add a new element to the configuration with the options you want to override and specify the name as command-line parameter. I added a sample “mess” configuration to the default mame_regtest.xml.

**Note on 0.113 cycle:**

* 0.113u2: Won't work at all, because -str doesn't skips the disclaimer screens in this version.

**Building mame_regtest:** As of version 0.66 mame_regtest comes with a makefile. Make sure you change the *_INC and LIBS, so they match your system.

You should have at least **libxml2-2.6.27** and **zlib-1.2.3**.

**How to use devices:** You have to create a file with the following syntax:

`mrt_<drivername>.xml`

It has to contain a list of filenames specified with the full path and you have to put it in the same directory as the mame_regtest binary.
```xml
<images>
    <image cart="/path/to/superchargercart.rom" cassette="/path/to/superchargergame.cas"/>
    <image cart="/path/to/superchargercart1.rom" cassette="/path/to/superchargergame1.cas"/>
    <image cart="/path/to/cart2.rom"/>
</images>
```

## Example

Here is a sample mess section for the mame_regtest.xml which tests the a2600 set:

```xml
<mess>
    <option name='executable' value='/path/to/messd'/>
    <option name='rompath' value='/path/to/biosroms'/>
    <option name='xpath_expr' value='DRIVER_ROOT[@sourcefile="a2600.c"]'/>
    <option name='str' value='3'/>
    <option name='use_debug' value='0'/>
</mess>
```