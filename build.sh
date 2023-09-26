# 如果没有build文件夹，就创建
if [ ! -d `pwd`/build ]; then
    mkdir `pwd`/build
fi


# 编译
cd  build/ &&
    cmake .. && 
    make -j8
cd ../