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