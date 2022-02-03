# 如何用C++实现一个简易数据库（一）
## 1. REPL是什么？
“读取-求值-输出”循环 ***(英語：Read-Eval-Print Loop，简称REPL)***，也被称做交互式顶层构件 ***(英語：interactive toplevel)***，是一个简单的，交互式的编程环境。
## 2. 怎么实现一个简单REPL？
首先我们直接启动一个无限循环，就像一个shell一样。之后我们`print_prompt()`意味着打印提示符。接着虽然std::string被大家诟病许久，但是作为我们简易数据库的使用也暂且足矣。我们每次重复创建`input_line`这个string对象，同时通过`std::getline`这个函数从`std::cin`标准输入中获取到所需信息（以换行符做分割，即回车标志着新语句）

`.exit`意味着退出当前REPL交互，即成功退出，其他情况即输出未知命令。
```c++
void print_prompt()
{
    std::cout << "db > ";
}

int main(int argc, char const *argv[])
{
    while (true)
    {
        print_prompt();
        std::string input_line;
        std::getline(std::cin, input_line);

        if (input_line == ".exit")
        {
            exit(EXIT_SUCCESS);
        }
        else
        {
            std::cout << "Unrecognized command " 
            << input_line << "." << std::endl;
        }
    }
    return 0;
}
```