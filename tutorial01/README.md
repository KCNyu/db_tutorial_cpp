# 如何用C++实现一个简易数据库（一）

## 1. 如何安装我们的单元测试环境？
* 检查我们是否有安装`ruby`。

    输入命令`ruby -v`，我们将得到
```
ruby 2.7.0p0 (2019-12-25 revision 647ee6f091) [x86_64-linux-gnu]
```
如果没有安装使用`sudo apt install ruby`或对应操作系统的包管理工具安装即可。
* 检查我们是否有安装`rspec`
   
    输入命令`rspec -v`，我们将得到
```
RSpec 3.10
  - rspec-core 3.10.2
  - rspec-expectations 3.10.2
  - rspec-mocks 3.10.3
  - rspec-support 3.10.3
```
同理如果没有安装则使用`sudo gem install rspec`安装即可。若提示没有`gem`则使用`sudo apt install gem`便可完成安装。

`ruby`和`rspec`版本基本未对本单元测试造成影响，如果存在问题，可提出。

至此我们的测试环境便已经安装完成了。

## 2. 如何编写和使用我们的第一个单元测试？
让我们在目录下创建`db_test.rb`文件。
```ruby
describe 'database' do
  def run_script(commands)
    raw_output = nil
    IO.popen("./db", "r+") do |pipe|
      commands.each do |command|
        pipe.puts command
      end

      pipe.close_write

      # Read entire output
      raw_output = pipe.gets(nil)
    end
    raw_output.split("\n")
  end

  it 'test exit and unrecognized command' do
    result = run_script([
      "hello world",
      "HELLO WORLD",
      ".exit",
    ])
    expect(result).to match_array([
      "db > Unrecognized command: hello world",
      "db > Unrecognized command: HELLO WORLD",
      "db > Bye!",
    ])
  end
end
```
让我们来执行它看看，输入`rspec spec db_test.rb`
```
LoadError:
  cannot load such file ...
```
显然是会失败的，因为我们甚至还没有在目录下编译创建我们的`db`文件。
现在让我们从最简单交互窗口`REPL`开始。
## 3. REPL是什么？
“读取-求值-输出”循环 ***(英語：Read-Eval-Print Loop，简称REPL)***，也被称做交互式顶层构件 ***(英語：interactive toplevel)***，是一个简单的，交互式的编程环境。
## 4. 怎么实现一个简单REPL？
首先我们直接启动一个无限循环，就像一个shell一样。之后我们`print_prompt()`意味着打印提示符。接着虽然std::string被大家诟病许久，但是作为我们简易数据库的使用也暂且足矣。我们每次重复创建`input_line`这个string对象，同时通过`std::getline`这个函数从`std::cin`标准输入中获取到所需信息（以换行符做分割，即回车标志着新语句）

`.exit`意味着退出当前REPL交互，即成功退出，其他情况即输出未知命令。
```c++
void DB::print_prompt()
{
    std::cout << "db > ";
}

bool DB::parse_meta_command(std::string command)
{
    if (command == ".exit")
    {
        std::cout << "Bye!" << std::endl;
        exit(EXIT_SUCCESS);
    }
    else
    {
        std::cout << "Unrecognized command: " << command << std::endl;
        return true;
    }
    return false;
}
void DB::start()
{
    while (true)
    {
        print_prompt();
        
        std::string input_line;
        std::getline(std::cin, input_line);

        if (parse_meta_command(input_line))
        {
            continue;
        }
    }
}
```
让我们来测试一下，首先编译我们的文件`g++ db.cpp -o db`，再输入`rspec spec db_test.rb`
```
.

Finished in 0.00504 seconds (files took 0.07621 seconds to load)
1 example, 0 failures
```
结果显然，恭喜大家通过了所创建的单元测试。
## 3. 总结
作为教程第一章，实现REPL是非常基础的。由于我们使用了`std::string`，故省去了很多`c_str`内存管理以及长度等一系列繁琐的事情。同时基于我们的测试进行开发，作为教程来说可以让大家更加清楚该朝着哪个方向前进。下一章我们将对输入的命令进行更进一步的分析。