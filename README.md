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
bin/mrsequential lib/libwc.so  MapReduce/pg*.txt 
```

##  其他 
```shell 
# 查看 so中的函数名
objdump -T lib/libmtiming.so | grep mapTask
```