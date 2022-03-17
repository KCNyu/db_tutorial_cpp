# 如何用C++实现一个简易数据库
*基于cstack/db_tutorial C语言版本*
* KCNyu
* 2022/2/2

作为笔者写的第一个系列型教程，还是选择基于前人的教程经验以及添加一些自己个人的探索。也许有很多纰漏之处，希望大家指正。

## 1. 数据库是什么？
数据库是“按照数据结构来组织、存储和管理数据的仓库”。是一个长期存储在计算机内的、有组织的、可共享的、统一管理的大量数据的集合。使用者可以对其中的资料执行新增、选取、更新、刪除等操作。

## 2. SQL是什么？
SQL ***(Structured Query Language:结构化查询语言)*** 是一种特定目的程式语言，用于管理关系数据库管理系统 ***(RDBMS)***，或在关系流数据管理系统 ***(RDSMS)*** 中进行流处理。

## 3. 我们最终会实现什么？
前端 ***(front-end)***

* 分词器 ***(tokenizer)***

* 解析器 ***(parser)***

* 代码生成器 ***(code generator)***

后端 ***(back-end)***

* 虚拟机 ***(virtual machine)***

* B树 ***(B-tree)***

* 分页 ***(pager)***

* 操作系统层接口 ***(os interface)***


## 4. 我们的开发流程是什么？
### 测试驱动开发 ***(test driven development, TDD)***
* 添加测试用例
* 运行并查看失败的用例
* 改动代码以通过测试
* 通过全部测试
  
## 5. 我们的单元目录结构是什么？

### **tutorial**
* 本单元完整代码实现 ***(db.cpp)***
* 本单元对应测试用例 ***(db_test.rb)***
* 本单元对应教程详解 ***(README.md)***

## 6. 教学大纲
* [tutorial01-安装测试环境和实现REPL](./tutorial01/README.md)
* [tutorial02-实现解析前端和虚拟机](./tutorial02/README.md)
* [tutorial03-实现insert和select](./tutorial03/README.md)
* [tutorial04-边界测试与修复BUG](./tutorial04/README.md)
* [tutorial05-实现磁盘存储](./tutorial05/README.md)
* [tutorial06-实现光标](./tutorial06/README.md)
* [tutorial07-初步实现B-Tree](./tutorial07/README.md)
* [tutorial08-实现二分搜索和防重](./tutorial08/README.md)
* [tutorial09-实现拆分叶结点](./tutorial09/README.md)
* [tutorial10-实现递归搜索](./tutorial10/README.md)