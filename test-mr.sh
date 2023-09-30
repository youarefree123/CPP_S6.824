bash build.sh 

# 执行
if [ -f "wc-correct" ]; 
then echo "wc-correct 已存在 "
else
    # start=$(date +%s)
    
    bin/mrsequential lib/libwc.so  MapReduce/pg*.txt || exit 1
    sort mr-wc > wc-correct
    rm -rf mr-wc

    # end=$(date +%s)
    # dif=$[$end - $start ]
    # echo " mrsequential :  $dif s" 
fi

failed_any=0

echo '*****' Starting wc test

# 开启master

# start=$(date +%s)

timeout -k 2s 180s bin/master  MapReduce/pg*.txt &
sleep 1

# 开启 多个worker
timeout -k 2s 180s bin/worker lib/libwc.so &
timeout -k 2s 180s bin/worker lib/libwc.so &
timeout -k 2s 180s bin/worker lib/libwc.so &

wait

# end=$(date +%s)
# dif=$[ $end - $start ]
# echo " MapReduce :  $dif s" 


sort mr-out-* | grep . > mr-wc-all


echo " "

# 判断是否一致
if cmp wc-correct mr-wc-all 
then 
    echo '--- wc test : PASS'
else 
    echo '--- wc test : Fail'
    failed_any=1
fi


echo '***' Starting map parallelism test.

rm -f mr-* 
echo "临时文件已删除"

timeout -k 2s 180s bin/master MapReduce/pg*txt &
sleep 1

timeout -k 2s 180s bin/worker lib/libmtiming.so &
timeout -k 2s 180s bin/worker lib/libmtiming.so

NT=`cat mr-out* | grep '^times-' | wc -l | sed 's/ //g'`
if [ "$NT" != "2" ]
then
  echo '---' saw "$NT" workers rather than 2
  echo '---' map parallelism test: FAIL
  failed_any=1
fi

if cat mr-out* | grep '^parallel.* 2' > /dev/null
then
  echo '---' map parallelism test: PASS
else
  echo '---' map workers did not run in parallel
  echo '---' map parallelism test: FAIL
  failed_any=1
fi

wait ; wait


if [ $failed_any -eq 0 ]; then
    echo '***' PASSED ALL TESTS
else
    echo '***' FAILED SOME TESTS
    exit 1
fi
