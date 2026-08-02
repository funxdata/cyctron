# localserver
small HTTP server running locally


### 开发

* 编译并运行测试
```
ninja test

```

* 创建发布包

```

mkdir build
cd build
cmake -G Ninja ..

```

* 同时编译、测试和打包

ninja package test

#### windows 环境下的一些改变

* 相关依赖

https://www.msys2.org/

pacman -S mingw-w64-x86_64-toolchain

cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

mingw32-make -j4



