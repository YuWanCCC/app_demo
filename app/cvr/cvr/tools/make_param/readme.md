# make_param

一个用于创建和管理参数分区镜像的工具。该工具支持在开发板上读写参数分区，以及在PC上创建参数镜像文件。

## 功能特点

- 支持多个配置文件的打包和解包
- 为每个配置文件预留2KB扩展空间
- 支持文件CRC校验
- 支持参数分区的读取和回写
- 支持自定义分区路径
- 支持参数读写自动化测试
- 支持调试信息输出控制

## 镜像格式

参数镜像（param.img）的布局如下：
+---------------------------+ 0x0000
|        ParamHeader        |
|--------------------------|
| - magic (4 bytes)        |  "PARA" (0x4D524150)
| - version (4 bytes)      |  0x0100
| - file_count (4 bytes)   |  实际文件数量
| - total_size (4 bytes)   |  总大小
| - header_crc (4 bytes)   |  头部校验和
|--------------------------|
|      FileInfo[0]         |
| - filename (64 bytes)    |  文件名
| - size (4 bytes)         |  实际大小
| - offset (4 bytes)       |  文件偏移
| - crc32 (4 bytes)        |  校验和
|--------------------------|
|      FileInfo[1]         |
|          ...            |
|      FileInfo[7]         |  最多8个文件信息
+---------------------------+ 0x0200 (大约512字节)
|                          |
|     File 1 Content       |  第一个文件内容
|          ...            |  (实际大小)
|                          |
|--------------------------|
|     Reserved Space 1     |  预留2KB空间
+---------------------------+
|                          |
|     File 2 Content       |  第二个文件内容
|          ...            |  (实际大小)
|                          |
|--------------------------|
|     Reserved Space 2     |  预留2KB空间
+---------------------------+
|          ...            |
+---------------------------+
|                          |
|     File N Content       |  最后一个文件内容
|          ...            |  (实际大小)
|                          |
|--------------------------|
|     Reserved Space N     |  预留2KB空间
+---------------------------+

注意：
1. 每个文件后面预留2KB空间用于将来扩展

## 使用方法

### 在开发板上

1. 读取参数分区（默认读取/dev/mmcblk0p5）：
./make_param
2. 读取指定分区：
./make_param --partition /dev/mmcblk0p6
3. 回写参数：
./make_param --writeback
4. 回写到指定分区：
./make_param --writeback --partition /dev/mmcblk0p6
5. 显示帮助：
./make_param --help
### 在PC上

1. 制作param.img：

  ./build_param.sh --ini_dir ini --output ./

  make_param_host "--ini_dir" "${INI_DIR}" "--output" "${OUTPUT_DIR}"

2. 显示帮助：
  ./make_param --help
## 工作流程

1. **创建镜像**：
   - 在PC上使用make_param_host工具
   - 指定ini文件目录
   - 生成param.img文件

2. **读取参数**：
   - 在开发板上使用make_param工具
   - 从分区读取param.img内容
   - 解析并导出各个配置文件

3. **修改参数**：
   - 编辑导出的配置文件
   - 使用make_param工具的writeback功能回写到分区

## 注意事项

- 每个配置文件后预留2KB空间用于将来扩展
- 最多支持8个配置文件
- 确保有足够的权限访问分区设备
- 回写操作前建议备份原有数据
