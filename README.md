# simple-luatb

一个简单的VPI库，可以加载使用Lua写的测试脚本，
用来对Verilog设计进行验证。
目前不支持在lua中操作VHDL对象。

目前在QuestaSim 2019.3中进行测试，
其他仿真器未测试，理论上支持Verilog
VPI标准的应该都行。

使用的lua版本是5.4。

构建工具为xmake，编译工具为TDM-GCC。

构建时需要在项目目录下建立lib文件夹，
并在里面放上liblua.a以及libmtipli.a。

liblua.a可以下载lua源码自己编译，
TDM-GCC就能完成。

libmtipli.a是QuestaSim中已有的，
在安装路径下win64目录里。

