<p align="center"><img src="https://github.com/shihchinw/baktsiu/raw/master/resources/app.png"></p>

# Bak-Tsiu — Examining Every Pixel Differences

<iframe src="https://player.vimeo.com/video/381616409" width="640" height="360" frameborder="0" allow="autoplay; fullscreen" allowfullscreen></iframe>

## Introduction

Bak-Tsiu is an image viewer designed for comparing and examining differences among multiple images. 

The motivation behind the implementation of Bak-Tsiu is that I found there are no convenient tools to examine pixel differences among multiple rendering results of different anti-aliasing algorithms and their corresponding configurations. Bak-Tsiu is a tool derived from my personal workflow, it provides split and side-by-side view along with close-up RGB values inspection. For more details, please see the [quickstart guide](docs/quickstart_guide.md).

![Column View](docs/images/side_by_side_navigation.jpg)

## Compiling

Bak-Tsiu is trying to be self-contained as much as possible. Most third party libraries are contained in the same repository. However, there are still two external submodules need to be downloaded:

* GLFW
* OpenEXR [optional]

All relevant resources like fonts and shaders are all embedded into the execution file. Thus we only need to take single execution file for distribution.

### Windows

To fetch the entire project with submodules, please add `--recursive` flag in clone command:

```bash
$ git clone --recursive https://github.com/shihchinw/baktsiu.git
$ cd baktsiu
$ mkdir build
$ cd build
$ cmake -G "Visual Studio 15 2017 Win64" -DUSE_OPENEXR:BOOL=ON ..
```

You can open generated baktsiu.sln to build the solution as usual, or execute `msbuild baktsiu.sln -target:Build` from console.

### Linux and Mac OS

```bash
$ git clone --recursive https://github.com/shihchinw/baktsiu.git
$ cd baktsiu
$ mkdir build
$ cd build
$ cmake -G "Unix Makefiles" -DUSE_OPENEXR:BOOL=ON ..
$ make
$ sudo make install
```