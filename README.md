# rnbo.adapter.max
RNBO Max Adapter Code

## Building for Development

We use [CMake](https://cmake.org/) and [Conan](https://conan.io/) to build our externals.
You'll also need some exported c++ source from a rnbo patcher.

There are a few varaibles you might like to set with CMake:

* `RNBO_CPP_DIR`
* `RNBO_CLASS_FILE`
* `RNBO_DESCRIPTION_FILE`
* `RNBO_WRAPPER_HAS_AUDIO`
* `RNBO_WRAPPER_HAS_MSG_OUT`
* `RNBO_WRAPPER_INC_TRANSPORT_ATTR`
* `EXTERN_OUTPUT_NAME`
* `CMAKE_BUILD_TYPE`

Here is an example of how I build for debugging with Xcode:

```shell
cd external
mkdir build
cd build
cmake .. -G Xcode -DEXTERN_OUTPUT_NAME="foo~" -DRNBO_WRAPPER_HAS_AUDIO=True -DRNBO_WRAPPER_HAS_MSG_OUT=True \
    -DRNBO_CPP_DIR=/Users/xnor/dev/rnbo.core/src/cpp/ \
    -DRNBO_CLASS_FILE=/Users/xnor/Documents/export/bufferplayer/rnbo_source.cpp \
    -DRNBO_DESCRIPTION_FILE=/Users/xnor/Documents/export/bufferplayer/description.json
cmake --build .
```

You can also use `ccmake` or `cmake-gui` to set these values interatively.

### Notes

To easily load an external after I've built it, I created a symlink to my build folder from the Max Packages directory:

`/Users/xnor/Documents/Max 8/Packages/rnbo-extern-debug -> /Users/xnor/dev/rnbo.core/src/cpp/adapters/max/external/build`

This lets Max find the built external and allows you to hit breakpoints in the external code from your debugger.
