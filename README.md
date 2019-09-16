# MC100-GPIO-Azure-IoT-Demo

        docker build -t imx6_mbyz:latest . --network=host
        id=$(docker create imx6_mbyz)
        docker cp $id:/home/builder/src/cmake/mbyz ./mbyz-openwrt-imx6
        docker rm -v $id


**More info about the MC-Technologies MC100-GPIO**

you can find [here](https://www.mc-technologies.net/en/product-groups/data-techniques/mobile-terminals-gateways/MC100-GPIO-4G-LTE-Gateway)
