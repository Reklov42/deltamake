> [!CAUTION]
> Node.JS implementation (v2.1.0) is deprecated and no longer supported

# DeltaMake

Project and solution builder with JSON-based configuration files for my C/C++ projects.

## Usage

`deltamake [flags] [build1, build2, ...]`

If build names are not specified, the "default" build name will be used.

### Flags

`-h --help`<br>
Show help text

`-f --force`<br>
Force rebuild all solutions (ignore all pre-builds)

`-n --no-build`
Don't build anything (useful with scan flag)

`-s --scan <code> <headers>`<br>
Scan `scanPath` for files with specified extensions and save to files and headers lists

Example usage:<br>
`deltamake -n -s .cpp .h`

## Example project tree

Single solution:

```text
TestSolution
├─ build
│  └─ tmp
├─ source
│  ├─ main.cpp
│  └─ main.h
└─ solution.json
```

Project with multiple solutions:

```text
TestProjext
├─ TestSolution1
│  ├─ SomeFolder
│  └─ solution.json
├─ TestSolution2
│  ├─ SomeFolder
│  └─ solution.json
└─ project.json
```

## Project configuration file

```json
{
    "version": "2.1.0",
    "solutions": {
        "ts1": "TestSolution1",
        "ts2": "TestSolution2"
    },
    "builds": {
        "default": {
            "solutions": [ "ts1" ]
        },
        "buildAll": {
            "solutions": [ "ts1", "ts2" ]
        }
    }
}
```

`solutions`<br>
list of solutions pairs `"codename": "path"`

`builds`<br>
list of build configurations with structure:
> `solutions`<br>
> list of solution codenames

## Solution configuration file

```json
{
    "version": "2.1.0",
    "scanPath": "source/",
    "buildPath": "build/",
    "tmpPath": "build/tmp/",
    "files": [
        "source/main.cpp",
    ],
    "headers": [
        "source/main.h"
    ],
    "builds": {
        "default": {
            "type": "exec",
            "includePaths": [
                "source/",
            ],
            "libPaths": [],
            "defines": [],
            "compiler": "g++",
            "compilerFlags": "-O2 -std=c++17 -lm",
            "linker": "g++",
            "linkerFlags": "-O2 -std=c++17 -lm",
            "staticLibs": [],
            "outname": "project"
        },
        "lib": {
            "type": "lib",
            "includePaths": [
                "source/"
            ],
            "libPaths": [],
            "defines": [
                "__SOME_LIB__"
            ],
            "compiler": "gcc",
            "compilerFlags": "-O2 -std=gnu99",
            "archiver": "ar",
            "staticLibs": [],
            "outname": "lib.a"
        }
    }
}
```

`scanPath`<br>
path for `-s`/`--scan` option

`buildPath`<br>
path for build output

`tmpPath`<br>
path for tmp objects

`files`<br>
list of found code files

`headers`<br>
list of found header files

`builds`<br>
list of build configurations with one of structure:
> `type`<br>
> type of build configuration. Can be:
>
> * `exec`<br>
> build as executable
>
> * `lib`<br>
> build as library
>
> `includePaths`<br>
> list of include paths
> 
> `libPaths`<br>
> list of paths to libraries
> 
> `defines`<br>
> list of global defines names
> 
> `compiler`<br>
> used compiler
> 
> `compilerFlags`<br>
> string of flags for compiler
> 
> `staticLibs`<br>
> list of static library names
> 
> `outname`<br>
> name of output build
>
> if `type` is `exec`:<br>
> > `linker`<br>
> > used linker<br>
> >
> > `linkerFlags`<br>
> > string of flags for linker
>
> if `type` is `lib`:
> > `archiver`
> > used archiver
