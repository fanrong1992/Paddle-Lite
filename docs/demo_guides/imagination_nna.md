# PaddleLite使用颖脉NNA预测部署

Paddle Lite已支持Imagination NNA的预测部署。
其接入原理是与之前华为Kirin NPU类似，即加载并分析Paddle模型，将Paddle算子转成Imagination DNN APIs进行网络构建，在线生成并执行模型。

## 支持现状

### 已支持的芯片

- 紫光展锐虎贲T7510

### 已支持的设备

- 海信F50，Roc1开发板（基于T7510的微型电脑主板）
- 酷派X10（暂未提供demo）

### 已支持的Paddle模型

#### 模型
- [mobilenet_v1_int8_224_per_layer](https://paddlelite-demo.bj.bcebos.com/models/mobilenet_v1_int8_224_per_layer.tar.gz)

#### 性能
- 测试环境
  - 编译环境
    - Ubuntu 18.04，GCC 5.4 for ARMLinux aarch64

  - 硬件环境
    - 紫光展锐虎贲T7510
      - Roc1开发板
      - CPU：4 x Cortex-A75 2.0 GHz + 4 x Cortex-A55 1.8 GHz
      - NNA：4 TOPs @1.0GHz

- 测试方法
  - warmup=1，repeats=5，统计平均时间，单位是ms
  - 线程数为1，```paddle::lite_api::PowerMode CPU_POWER_MODE```设置为``` paddle::lite_api::PowerMode::LITE_POWER_HIGH ```
  - 分类模型的输入图像维度是{1，3，224，224}

- 测试结果

  |模型 |紫光展锐虎贲T7510||
  |---|---|---|
  |  |CPU(ms) | NPU(ms) |
  |mobilenet_v1_int8_224_per_layer|  61.093601|  3.217800|

### 已支持（或部分支持）的Paddle算子

可以通过访问[https://github.com/PaddlePaddle/Paddle-Lite/blob/develop/lite/kernels/nnadapter/bridges/paddle_use_bridges.h](https://github.com/PaddlePaddle/Paddle-Lite/blob/develop/lite/kernels/nnadapter/bridges/paddle_use_bridges.h)获得最新的算子支持列表。

**不经过NNAdapter标准算子转换，而是直接将Paddle算子转换成Imagination NNA IR的方案可点击[链接](https://paddle-lite.readthedocs.io/zh/release-v2.9/demo_guides/imagination_nna.html)**。

## 参考示例演示

### 测试设备(Roc1开发板)

![roc1_front](https://paddlelite-demo.bj.bcebos.com/devices/imagination/Roc1_front.jpg)

![roc1_back](https://paddlelite-demo.bj.bcebos.com/devices/imagination/Roc1_back.jpg)

### 准备设备环境

- 需要依赖特定版本的firmware，请联系Imagination相关研发同学 jason.wang@imgtec.com；
- 确定能够通过SSH方式远程登录Roc 1开发板；
- 由于Roc 1的ARM CPU能力较弱，示例程序和PaddleLite库的编译均采用交叉编译方式。

### 准备交叉编译环境

- 按照以下两种方式配置交叉编译环境：
  - Docker交叉编译环境：由于Roc1运行环境为Ubuntu 18.04，且Imagination NNA DDK依赖高版本的glibc，因此不能直接使用[编译环境准备](../source_compile/compile_env)中的docker image，而需要按照如下方式在Host机器上手动构建Ubuntu 18.04的docker image；

    ```
    $ wget https://paddlelite-demo.bj.bcebos.com/devices/imagination/Dockerfile
    $ docker build --network=host -t paddlepaddle/paddle-lite-ubuntu18_04:1.0 .
    $ docker run --name paddle-lite-ubuntu18_04 --net=host -it --privileged -v $PWD:/Work -w /Work paddlepaddle/paddle-lite-ubuntu18_04:1.0 /bin/bash
    ```

  - Ubuntu交叉编译环境：要求Host为Ubuntu 18.04系统，参考[编译环境准备](../source_compile/compile_env)中的"交叉编译ARM Linux"步骤安装交叉编译工具链。
- 由于需要通过scp和ssh命令将交叉编译生成的PaddleLite库和示例程序传输到设备上执行，因此，在进入Docker容器后还需要安装如下软件：

  ```
  # apt-get install openssh-client sshpass
  ```

### 运行图像分类示例程序

- 下载PaddleLite通用示例程序[PaddleLite-generic-demo.tar.gz](https://paddlelite-demo.bj.bcebos.com/devices/generic/PaddleLite-generic-demo.tar.gz)，解压后目录主体结构如下：

  ```shell
    - PaddleLite-generic-demo
      - image_classification_demo
        - assets
          - images
            - tabby_cat.jpg # 测试图片
            - tabby_cat.raw # 经过convert_to_raw_image.py处理后的RGB Raw图像
          - labels
            - synset_words.txt # 1000分类label文件
          - models
            - mobilenet_v1_int8_224_per_layer # Paddle non-combined格式的mobilenet_v1 int8全量化模型
              - __model__ # Paddle fluid模型组网文件，可使用netron查看网络结构
              — conv1_weights # Paddle fluid模型参数文件
              - batch_norm_0.tmp_2.quant_dequant.scale # Paddle fluid模型量化参数文件
              — subgraph_partition_config_file.txt # 自定义子图分割配置文件
              ...
        - shell
          - CMakeLists.txt # 示例程序CMake脚本
          - build.linux.arm64 # arm64编译工作目录
            - image_classification_demo # 已编译好的，适用于arm64的示例程序
            ...
          ...
          - image_classification_demo.cc # 示例程序源码
          - build.sh # 示例程序编译脚本
          - run_with_ssh.sh # 示例程序adb运行脚本
          - run_with_adb.sh # 示例程序ssh运行脚本
          - run.sh # 示例程序运行脚本
      - libs
        - PaddleLite
          - linux
            - arm64
              - include
              - lib
                - imagination_nna
                  - libnnadapter_driver_imagination_nna.so # imagination nna driver hal库
                  - libnnadapter.so # NNAdapter API运行时库
                  - libcrypto.so
                  ...
                - libpaddle_full_api_shared.so # 预编译PaddleLite full api库
                - libpaddle_light_api_shared.so # 预编译PaddleLite light api库
                ...
            ...
          ...
        - OpenCV # OpenCV预编译库
  ```

- 按照以下命令分别运行转换后的ARM CPU模型和Imagination NNA模型，比较它们的性能和结果；

  ```shell
  注意：
  1）run_with_adb.sh不能在docker环境执行，否则可能无法找到设备，也不能在设备上运行。
  2）run_with_ssh.sh不能在设备上运行，且执行前需要配置目标设备的IP地址、SSH账号和密码。
  3）build.sh根据入参生成针对不同操作系统、体系结构的二进制程序，需查阅注释信息配置正确的参数值。
  4）run_with_adb.sh入参包括模型名称、操作系统、体系结构、目标设备、设备序列号等，需查阅注释信息配置正确的参数值。
  5）run_with_ssh.sh入参包括模型名称、操作系统、体系结构、目标设备、ip地址、用户名、用户密码等，需查阅注释信息配置正确的参数值。

  在ARM CPU上运行mobilenetv1全量化模型
  $ cd PaddleLite-generic-demo/image_classification_demo/shell
  $ ./run_with_ssh.sh mobilenet_v1_int8_224_per_layer linux arm64 cpu 192.168.100.10 22 img imgroc1
  ...
  iter 0 cost: 61.130001 ms
  iter 1 cost: 61.073002 ms
  iter 2 cost: 61.081001 ms
  iter 3 cost: 61.088001 ms
  iter 4 cost: 61.096001 ms
  warmup: 1 repeat: 5, average: 61.093601 ms, max: 61.130001 ms, min: 61.073002 ms
  results: 3
  Top0  tabby, tabby cat - 0.490191
  Top1  Egyptian cat - 0.441032
  Top2  tiger cat - 0.060051
  Preprocess time: 0.798000 ms
  Prediction time: 61.093601 ms
  Postprocess time: 0.167000 ms


  在Imagination NNA上运行mobilenetv1全量化模型
  $ cd PaddleLite-generic-demo/image_classification_demo/shell
  $ ./run_with_ssh.sh mobilenet_v1_int8_224_per_layer linux arm64 imagination_nna 192.168.100.10 22 img imgroc1
  ...
  iter 0 cost: 3.288000 ms
  iter 1 cost: 3.220000 ms
  iter 2 cost: 3.167000 ms
  iter 3 cost: 3.268000 ms
  iter 4 cost: 3.146000 ms
  warmup: 1 repeat: 5, average: 3.217800 ms, max: 3.288000 ms, min: 3.146000 ms
  results: 3
  Top0  tabby, tabby cat - 0.514779
  Top1  Egyptian cat - 0.421183
  Top2  tiger cat - 0.052648
  Preprocess time: 0.818000 ms
  Prediction time: 3.217800 ms
  Postprocess time: 0.157000 ms
  ```

  - 如果需要更改测试图片，可将图片拷贝到PaddleLite-generic-demo/image_classification_demo/assets/images目录下，然后调用convert_to_raw_image.py生成相应的RGB Raw图像，最后修改run_with_adb.sh的IMAGE_NAME变量即可；
  ```shell
  注意：
  1）请根据buid.sh配置正确的参数值。
  2）需在docker环境中编译。

  ./build.sh linux arm64
  ```


### 更新模型

- 通过Paddle Fluid训练，或X2Paddle转换得到MobileNetv1 foat32模型[mobilenet_v1_fp32_224_fluid](https://paddlelite-demo.bj.bcebos.com/models/mobilenet_v1_fp32_224_fluid.tar.gz)；
- 参考[模型量化-静态离线量化](../user_guides/quant_post_static)使用PaddleSlim对float32模型进行量化（注意：由于Imagination NNA只支持tensor-wise的全量化模型，在启动量化脚本时请注意相关参数的设置），最终得到全量化MobileNetV1模型[mobilenet_v1_int8_224_fluid](https://paddlelite-demo.bj.bcebos.com/devices/imagination/mobilenet_v1_int8_224_fluid.tar.gz)；
- 参考[模型转化方法](../user_guides/model_optimize_tool)，利用opt工具转换生成Imagination NNA模型，仅需要将valid_targets设置为imagination_nna,arm即可。

  ```shell
  $ ./opt --model_dir=mobilenet_v1_int8_224_for_imagination_nna_fluid \
      --optimize_out_type=naive_buffer \
      --optimize_out=opt_model \
      --valid_targets=imagination_nna,arm
  
  替换自带的Imagination NNA模型
  $ cp opt_model.nb mobilenet_v1_int8_224_for_imagination_nna/model.nb
  ```

- 注意：opt生成的模型只是标记了Imagination NNA支持的Paddle算子，并没有真正生成Imagination NNA模型，只有在执行时才会将标记的Paddle算子转成Imagination DNN APIs，最终生成并执行模型。

### 更新支持Imagination NNA的Paddle Lite库

- 下载PaddleLite源码和Imagination NNA DDK

  ```shell
  $ git clone https://github.com/PaddlePaddle/Paddle-Lite.git
  $ cd Paddle-Lite
  $ git checkout <release-version-tag>
  $ curl -L https://paddlelite-demo.bj.bcebos.com/devices/imagination/imagination_nna_sdk.tar.gz -o - | tar -zx
  ```

- 编译并生成PaddleLite+ImaginationNNA for armv8的部署库

  - For Roc1
    - tiny_publish编译方式
      ```shell
      $ ./lite/tools/build_linux.sh --with_extra=ON --with_log=ON --with_nnadapter=ON --nnadapter_with_imagination_nna=ON --nnadapter_imagination_nna_sdk_root=$(pwd)/imagination_nna_sdk
      ```
      
    - full_publish编译方式
      ```shell
      $ ./lite/tools/build_linux.sh --with_extra=ON --with_log=ON --with_nnadapter=ON --nnadapter_with_imagination_nna=ON --nnadapter_imagination_nna_sdk_root=$(pwd)/imagination_nna_sdk full_publish
      ```
    - 替换头文件和库
      ```shell
      # 替换 include 目录：
      $ cp -rf build.lite.linux.armv8.gcc/inference_lite_lib.armlinux.armv8.nnadapter/cxx/include/ PaddleLite-generic-demo/libs/PaddleLite/linux/arm64/include/
      # 替换 NNAdapter相关so：
      $ cp -rf build.lite.linux.armv8.gcc/inference_lite_lib.armlinux.armv8.nnadapter/cxx/lib/libnnadapter* PaddleLite-generic-demo/libs/PaddleLite/linux/arm64/lib/imagination_nna/
      # 替换 libpaddle_full_api_shared.so或libpaddle_light_api_shared.so
      $ cp -rf build.lite.linux.armv8.gcc/inference_lite_lib.armlinux.armv8.nnadapter/cxx/lib/libpaddle_full_api_shared.so PaddleLite-generic-demo/libs/PaddleLite/linux/arm64/lib/
      $ cp -rf build.lite.linux.armv8.gcc/inference_lite_lib.armlinux.armv8.nnadapter/cxx/lib/libpaddle_light_api_shared.so PaddleLite-generic-demo/libs/PaddleLite/linux/arm64/lib/
      ```

- 替换头文件后需要重新编译示例程序

## 其它说明

- Imagination研发同学正在持续增加用于适配Paddle算子bridge/converter，以便适配更多Paddle模型。
