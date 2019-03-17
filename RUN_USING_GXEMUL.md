# Tips for Running HOS with GXEMUL

0. 如果你使用的不是 Linux，那么目前为止并没有提供支持 `:)`

---

1. 使用魔改版 GXEMUL。你可以在这里下载: [My GitHub](https://github.com/klx3300/gxemul-hos)

2. 使用如下命令编译并安装。注意改变安装目录:

   ```shell
   $ env PREFIX=/path/to/where/you/want/install ./configure
   # ... autoconf 输出 ...
   $ make && make install
   # ... make 输出 ...
   $ export PATH=$PATH:/path/to/where/you/want/install/bin
   # 这用于设置环境变量。如果你安装到系统路径（如 /usr），那么你大概不需要如此做。
   # 强烈不推荐安装到系统路径，除非你很清楚你在干什么！！
   ```

3. 下载MIPS交叉编译工具链. 用 `MIPS MTI Baremetal`版本. 解压并正确设置 `PATH` 和/或 `LD_LIBRARY_PATH` 环境变量，使得编译器能够工作.

   1. 如果你不确定的话，执行 `mips-mti-elf-gcc --version` 检查你是否正确设置。
   2. 同样，执行 `gxemul -H` 可以检查你的 GXEMUL 是否已正确安装。

4. HOS 同样需要使用魔改过的版本。不过也可能已经合并进入 mainline，具体参阅 HOS 的提交记录。

   1. 注意，如果你只打算观察原版 HOS 的运行，请切换到分支 `gxemul-adapt-basic`。其它分支可能正在被我魔改，可能甚至无法通过编译，请注意。
   2. 检查 `kern-ucore/Makefile.build`,  `TARGET_CFLAGS += -DBUILD_GXEMUL` 这行需要取消注释。
   3. 如果没有这一行，请在其他 `TARGET_CLFAGS += ...` 行的后面，实际构建代码的前面添加。

5. 在 HOS 根目录下执行以下命令编译内核：

   ```shell
   $ env CROSS_COMPILE=mips-mti-elf- make
   # ... make 输出 ...
   ```

6. 在 HOS 根目录下执行 `./gxemul_run.sh` 开始模拟。

## 已知问题

目前不存在已知问题。

## 更新记录

### Mar 15, 2019

修复了一些其它问题，现在已经可以正常进入 user shell，工作正常。

#### 建议升级到此版本，GXEMUL 和 HOS 本体代码均需要重新获取。

#### 目前该版本仍未被 merge 进入 HOS mainline

## 我修改了什么？

- 修改 GXEMUL，对 R6000 型处理器启用了 MIPSR2 指令集的支持，否则无法执行在该标准下编译的 HOS。
- 修改 GXEMUL，将 R6000 型处理器的模拟 TLB 数量降为 16 个，以匹配 HOS 硬编码的数量。
- 修改 GXEMUL，增加了 `CP0[EBase]` 处理器，完善了向量中断，使得依赖于此的 HOS-MIPS 中断能够正常运行。
- 修改 HOS-MIPS，增加了 GXEMUL 特有的控制台输入输出驱动，以及中断控制的控制台输入处理流程，使其能够正确与宿主进行输入输出。
- 修改 HOS-MIPS 用户库的系统调用过程，以避开一个编译器 BUG。
  - 具体描述如下：当源代码函数体中出现内联汇编时，函数返回时 `jr $ra` 的分支延迟槽中丢失 `nop` 指令，导致执行其他未知指令。
- 增加了一些debug宏和 kernel monitor 命令，使得调试更加方便。