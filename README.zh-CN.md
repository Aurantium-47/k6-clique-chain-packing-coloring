# K6 团链的 packing coloring：反例与最佳统一上界

本包给出 Furmańczyk、Gözüpek、Özkan 的 *Packing coloring of graphs with long paths*（[arXiv:2511.12761v1](https://arxiv.org/abs/2511.12761)）Conjecture 2 的计算机辅助反例。

H_t 由 t 个顶点互不相交的 K6 组成，每团选不同的 L、R 顶点，相邻团仅通过 R_j L_(j+1) 桥边连接。同色 i 的不同顶点必须满足图距离严格大于 i。证明结果为：所有 t≥1 都能用23色；所有 t≥75 恰好需要23色。因此450顶点、1199边的 H75 否定原22色统一上界，23是整个图族的最佳统一常数。

这里不声称75是最小反例，不声称求出全部较短链的精确色数，也不作全球首证保证。

英文论文：[PDF](manuscript/main.pdf) · [LaTeX 源码](manuscript/main.tex)。

[完整证明](PROOF.zh-CN.md) 包含距离公式、必要调度松弛、容量界、递降秩归纳及周期构造，完整证明材料并非只有答案。负证书包含16646562个状态及全部合法后继的覆盖信息；两个完整检查程序的已执行结果见 [验证记录](verification/negative_full_results.json)。两种实现共享数学模型，不等于两份毫无共同假设的证明。正证书给出480团周期及H75完整边表、实际顶点颜色。

133172496字节的原始负证书不进入源仓库，单独以 Release 资产 **H75_rank_certificate.bin.gz** 提供。压缩及解压后的SHA256见 [资产清单](CERTIFICATE_ASSET.json)，解压、编译、运行和预期计数见 [复现说明](REPRODUCIBILITY.md)。无需重做搜索或安装优化求解器；需要Python 3.10+标准库和C++17编译器。完整验证的数据及查找表约占400 MB，实际环境另有开销。

已有结果记录不意味着这次公开打包重新运行了负证书。计算机辅助证明仍依赖检查器、编译器和运行环境；本包没有提供证明助手形式化，也不声称获得外部专家审稿。研究在AI辅助下完成。本包同时提供完整中文证明与英文论文 PDF、LaTeX 源码。

[English README](README.md) · [证书格式](docs/CERTIFICATE_FORMAT.md) · [归属与许可](RIGHTS_AND_LICENSE.md)
