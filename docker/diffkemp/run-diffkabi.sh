OLD_VER=linux-$1
if [ -d kernel/$OLD_VER ]; then
    VOL1="-v $PWD/kernel/$OLD_VER:/diffkemp/kernel/$OLD_VER:Z"
fi

NEW_VER=linux-$2
if [ -d kernel/$NEW_VER ]; then
    VOL2="-v $PWD/kernel/$NEW_VER:/diffkemp/kernel/$NEW_VER:Z"
fi

docker run -t $VOL1 $VOL2 -w /diffkemp -m 8g --cpus 3 viktormalik/diffkemp bin/diffkabi $@

