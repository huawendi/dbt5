#!/bin/bash
# filepath: /home/hwd/dbt5/run.sh

# 第 0 步:
    cd ~/dbt5
    source ~/postgresql-14.17/var
    source ~/dbt5/var
    dropdb -e dbt5

# 第 1 步:
    # (第一次 build 执行：)
    # dbt5-build-egen --include-dir=/home/hwd/dbt5/src/include --patch-dir=/home/hwd/dbt5/patches --source-dir=/home/hwd/dbt5/src /home/hwd/dbt5/egen/ || { 
    dbt5-build-egen --build-only --include-dir=/home/hwd/dbt5/src/include --source-dir=/home/hwd/dbt5/src /home/hwd/dbt5/egen/ || { 
        echo "dbt5-build-egen failed"; 
        exit 1; 
    }

# 第 2 步:
    dbt5-pgsql-build-db -c 1000 -i /home/hwd/dbt5/egen/ -s 100 -t 1000 -w 25 || {
        echo "dbt5-pgsql-build-db failed"; 
        exit 1; 
    }

    # dbt5-build pgsql 就是 dbt5-pgsql-build-db: dbt5-build --tpcetools=/mnt/disk/egen/ -t 1000 pgsql dbt5

    # (dbt5-pgsql-build-db 中依次执行 dbt5-pgsql-create-db, dbt5-pgsql-create-tables, dbt5-pgsql-create-indexes, dbt5-pgsql-load-stored-procs)
    # dbt5-pgsql-create-db -d dbt5
    # dbt5-pgsql-create-tables -d dbt5
    # dbt5-pgsql-load-stored-procs -d dbt5 -i /home/hwd/dbt5/storedproc/pgsql/pgsql
    # (修改 SHAREDIR="/home/hwd/dbt5/storedproc/pgsql/pgsql")

# 第 3 布 （可选）：备份数据库状态
    echo "backup dbt5 database ..."
    pg_dump -d dbt5 -f /data/hwd/pg14.17/backup/dbt5_$(date +%Y%m%d_%H%M%S).backup

# 第 4 步（可选）: 删除捕获到的日志文件
    rm -r /data/hwd/pg14.17/dbt5_results
    rm -r /data/hwd/pg14.17/tpce-log/capture.*

# 第 5 步:
    dbt5-run --client-side -d 47 -f 100 --tpcetools=/home/hwd/dbt5/egen -t 1000 -u 2 -w 25 pgsql /data/hwd/pg14.17/dbt5_results
    # dbt5-run --client-side -d 76 -f 100 --tpcetools=/mnt/disk/egen -t 1000 -u 1 -w 25 pgsql /mnt/disk/pg15.7/dbt5_results
