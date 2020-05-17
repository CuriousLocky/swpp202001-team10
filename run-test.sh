#!/bin/bash

if [ "$#" -ne 2 ]; then
  echo "run-test.sh all <Interpreter path>"
  echo "run-test.sh <Test-path> <Interpreter path>"
  echo "eg)  ./run-test.sh ./test/binary_tree/ ~/swpp202001-interpreter/sf-interpreter"
  exit 1
fi

INTERPRETER="$2"

if [[ "$1" == "all" ]]
then
  TEST="./test/"
else
  TEST=$1
fi

echo "--- Start Test.. ---"
set -e

res="./test-score.log"

touch -c $res

if [[ "$1" == "all" ]]; then
  for dir in `find $TEST -maxdepth 1 -type d | cut -c 8-`  ; do
    echo "== Run Test on ${TEST}${dir} =="
    echo "#### $dir ####" >> $res
    echo " " >> $res
    ll=`find ${TEST}${dir} -name "*.ll"`
    s=`find ${TEST}${dir} -name "*.s"`
    ./sf-compiler $ll -o tmp.s
    END=`find ${TEST}${dir}/test/ -name "input*.txt" | wc -l`
      for i in $(seq 1 $END) ; do
        input="./${TEST}${dir}/test/input${i}.txt"
        output="./${TEST}${dir}/test/output${i}.txt"
        echo "-- input${i} --"
        echo "== input${i} ==" >> $res
        echo "-- before --" >> $res
        cat $input | $INTERPRETER $s > /dev/null
        cat sf-interpreter.log >> $res
        cat $input | $INTERPRETER tmp.s | diff $output -
        echo "-- after --" >> $res
        cat sf-interpreter.log >> $res
        echo " " >> $res
      done
  done
else
  dir="$1"
  echo "== Run Test on ${dir} =="
  echo "#### $dir ####" >> $res
  echo " " >> $res
  ll=`find $dir -name "*.ll"`
  s=`find $dir -name "*.s"`
  ./sf-compiler $ll -o tmp.s
  END=`find ${dir}test/ -name "input*.txt" | wc -l`
  for i in $(seq 1 $END) ; do
    input="./${dir}test/input${i}.txt"
    output="./${dir}test/output${i}.txt"
    echo "-- input${i} --"
    echo "== input${i} ==" >> $res
    echo "-- before --" >> $res
    cat $input | $INTERPRETER $s &> /dev/null
    cat sf-interpreter.log >> $res
    cat $input | $INTERPRETER tmp.s | diff $output -
    echo "-- after --" >> $res
    cat sf-interpreter.log >> $res
    echo " " >> $res
  done
fi

rm -f tmp.s sf-interpreter.log
