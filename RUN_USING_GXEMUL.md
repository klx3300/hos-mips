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

4. HOS 同样需要使用我魔改过的版本。不过目前为止原版大概也能用？

5. 在 HOS 根目录下执行以下命令编译内核：

   ```shell
   $ env CROSS_COMPILE=mips-mti-elf- make
   # ... make 输出 ...
   ```

6. 在 HOS 根目录下执行 `./gxemul_run.sh` 开始模拟。

## 已知问题

目前为止，对 GXEMUL 的双向适配只进行到成功执行用户进程为止。输入、总线、Flash 控制器等功能仍未做适配；仅适用于内核初始化阶段的调试。