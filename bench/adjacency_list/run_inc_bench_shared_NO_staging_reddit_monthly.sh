#!/usr/bin/env bash

export OMP_SCHEDULE="static"
export OMP_NUM_THREADS=96

INPUT_BASE_PATH=$1
INPUT_FILES_LIST=$2
DATASTORE_DIR=$3

rm -rf $DATASTORE_DIR

first_file=$(head -n 1 ${INPUT_FILES_LIST})
echo "First file ${first_file}"
srun --drop-caches=pagecache true
./run_adj_list_bench_metall -o ${DATASTORE_DIR} -V ${INPUT_BASE_PATH}/${first_file}


# echo ${INPUT_BASE_PATH}/${first_file}

# while IFS= read -r line
for line in 2017-02 2017-03 2017-04 2017-05 2017-06 2017-07 2017-08 2017-09 2017-10 2017-11 2017-12 2018-01 2018-02 2018-03 2018-04 2018-05 2018-06 2018-07 2018-08 2018-09 2018-10 2018-11 2018-12 2019-01 2019-02 2019-03 2019-04 2019-05 2019-06 2019-07 2019-08 2019-09 2019-10 2019-11 2019-12 2020-01 2020-02 2020-03 2020-04;
do
  # echo "File from loop ${line}"
#  if [ "$line" != "${first_file}" ]; then
     echo "File ${line}"
     srun --drop-caches=pagecache true
    ./run_adj_list_bench_metall -o ${DATASTORE_DIR} -V -A ${INPUT_BASE_PATH}/$line
#  fi
# done < "$INPUT_FILES_LIST"
done

# Useful information
echo "Datastore"
ls -Rlsth ${DATASTORE_DIR}"/"
df ${DATASTORE_DIR}"/"
du -h ${DATASTORE_DIR}"/"
