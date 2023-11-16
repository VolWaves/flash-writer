# pico template with scheduler
rpi pico template with scheduler

## prepare

### Get pico SDK

```shell
git clone https://github.com/raspberrypi/pico-sdk --depth=1 --recurse-submodules --shallow-submodules -j8
cd pico-sdk

## Add pico-sdk Path to your environment
echo export PICO_SDK_PATH=$PWD >> ~/.profile
```

### Install dependencies

```shell
sudo apt update && sudo apt install -y cmake make ninja-build gcc g++ openssl libssl-dev cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib
```

## Compile

```shell
make
```

## Usage

```shell
# build
make
# clang-format
make format
# clear build
make clean
# rebuild
make rebuild
```
