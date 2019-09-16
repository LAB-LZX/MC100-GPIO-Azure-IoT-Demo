# MC100-GPIO-Azure-IoT-Demo

        docker build -t imx6_mbyz:latest . --network=host
        id=$(docker create imx6_mbyz)
        docker cp $id:/home/builder/src/cmake/mbyz ./mbyz-openwrt-imx6
        docker rm -v $id
