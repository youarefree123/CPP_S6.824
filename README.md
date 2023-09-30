# CPP_S6.081

## 使用前需要安装 雅兰亭库

## 克隆项目
git clone https://github.com/youarefree123/CPP_S6.824.git

## 编译
```shell
bash build.sh
```

## MapReduce
```shell
# 注意： 第一次测试时，因为没有正确的结果文件，会调用单机版代码跑一次，可能会需要等待一段时间
bash test-mr.sh

bin/mrsequential lib/libwc.so  MapReduce/pg*.txt 
```

##  其他 
```shell 
# 查看 so中的函数名
objdump -T lib/libmtiming.so | grep mapTask
```