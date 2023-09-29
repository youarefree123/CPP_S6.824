bash build.sh 

# 执行
if [ -f "mr-wc-correct" ]; 
then echo "mr-wc-correct 已存在 "
else
    bin/mrsequential lib/libwc.so  MapReduce/pg*.txt || exit 1
    sort mr-wc > mr-wc-correct
    rm -rf mr-wc
fi

echo '*****' Starting wc test

# 开启master
timeout -k 2s 180s bin/master  MapReduce/pg*.txt &
sleep 1

# 开启 多个worker
timeout -k 2s 180s bin/worker lib/libwc.so &
timeout -k 2s 180s bin/worker lib/libwc.so &
timeout -k 2s 180s bin/worker lib/libwc.so &

wait

sort mr-out-* | grep . > mr-wc-all


echo " "
echo " "
echo " "

# 判断是否一致
if cmp mr-wc-correct mr-wc-all 
then 
    echo '--- wc test : PASS'
else 
    echo '--- wc Fail'
fi