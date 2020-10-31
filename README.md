# mame_regtest

(c) Copyright 2005-2020 by Oliver Stöneberg

**mame_regtest** is a tool designed to mass-test drivers of MAME-based applications like MAME, MESS and PinMAME.

The latest stable version is **0.74** which includes the new XML-based configuration instead of command-line parameters to make the usage easier and XPath support to select drivers for testing as well as the optional XML output with all the information about a driver run.

**Note on XPath:** You have to complete the `/mame/game` or `/mess/machine` expression with one, that returns a valid list of `game` or `machine` nodes. You have to use the placeholder **DRIVER_ROOT** for the application-specific beginning. See some examples for the removed options in the sample `mame_regtest.xml`. A tutorial on XPath can be found at http://www.zvon.org/xxl/XPathTutorial/General/examples.html

**Note on multiple configurations:** You have to add a new element to the configuration with the options you want to override and specify the name as command-line parameter. I added a sample `mess` configuration to the default `mame_regtest.xml`.

**Note on 0.113 cycle:**

* 0.113u2: Won't work at all, because -str doesn't skips the disclaimer screens in this version.

**Note on big-endian systems:** >ou have to comment the `LITTLE_ENDIAN` and uncomment the `BIG_ENDIAN` define

**Building mame_regtest:** As of version 0.75 mame_regtest is utilizing CMake and conan. **libxml2** and **zlib** are required to compile.

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

## MAME/MESS returncodes
`1` - terminated / unknown error
`2` - missing roms
`3` - assertion / fatalerror
`4` - no image specified (MESS-only)
`5` - no such game
`6` - invalid config
`128` - ???  
`100` - exception
`-1073741819` - exception (SDLMAME/SDLMESS)

## mame_regtest returncodes
`0` - OK
`1` - error

## pngcmp:
- is built as part of the MAME/MESS "tools" target - please copy it from an existing installation to the mame_regtest folder
- retruncodes:
    `0` - images are identical - no diff written
    `1` - images are identical - diff written
    `-1` - an error occured