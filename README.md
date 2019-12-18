![App Icon](icon.png)

# Introduction

Bak-Tsiu is a handcrafted image viewer focusing on comparing images and examining pixel differences. To make render passes as few as possible, most features are combined into few fragment shaders.



# Build

Bak-Tsiu is trying to be self-contained, most third party libraries are contained in the same respository. However, there are still two external submodules need to be downloaded:

* GLFW
* OpenEXR [optional]

## Windows

1. git clone --recursive https://foo
2. cd baktsiu
3. mkdir build
4. cd build
5. cmake -G "Visual Studio 15 2017 Win64"  ..