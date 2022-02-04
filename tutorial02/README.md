# 如何用C++实现一个简易数据库（二）

## 1. SQL的解析前端是什么？
它将传统输入的`string`字符串，解析成可被机器识别的字节码内部表现形式，并传递给虚拟机进一步执行。

## 2. 怎么实现一个SQL的解析前端？
先从我们上一章所解析的`command`来看起。我们将以`.`开头的非sql语句称作元命令 ***(meta command)*** 所以我们在一开始就检查是否以其开头，并单独封装一个`do_meta_command`函数来处理它。
```c++
bool DB::parse_meta_command(std::string command)
{
    if (command[0] == '.')
    {
        switch (do_meta_command(command))
        {
        case META_COMMAND_SUCCESS:
            return true;
        case META_COMMAND_UNRECOGNIZED_COMMAND:
            std::cout << "Unrecognized command: " << command << std::endl;
            return true;
        }
    }
    return false;
}
MetaCommandResult DB::do_meta_command(std::string command)
{
    if (command == ".exit")
    {
        std::cout << "Bye!" << std::endl;
        exit(EXIT_SUCCESS);
    }
    else
    {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}
```

同时我们观察到，我们现在引入了一些枚举类的解析结果，例如我们现在所使用用于解析元命令的
`
enum MetaCommandResult
    {
        META_COMMAND_SUCCESS,
        META_COMMAND_UNRECOGNIZED_COMMAND
    };
`

因此接下来，我将一次性定义另外两个解析结果用于解析`sql`语句。
`    
enum PrePareResult
    {
        PREPARE_SUCCESS,
        PREPARE_UNRECOGNIZED_STATEMENT
    };
`
这个枚举结果用于返回我们所传递的`sql`语句是否是符合标准的状态。这个时候就能将传统的以`select`或`insert`开头的`sql`语句转化成对应的`statement`状态字节码。
```c++
PrePareResult DB::prepare_statement(std::string &input_line, Statement &statement)
{
    if (!input_line.compare(0, 6, "insert"))
    {
        statement.type = STATEMENT_INSERT;
        return PREPARE_SUCCESS;
    }
    else if (!input_line.compare(0, 6, "select"))
    {
        statement.type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    else
    {
        return PREPARE_UNRECOGNIZED_STATEMENT;
    }
}
```
我们为`sql`语句目前仅定义了如下简单的两种状态字节码
`
    enum StatementType
    {
        STATEMENT_INSERT,
        STATEMENT_SELECT
    };
`
同时再将上一步成功转化后得到的`statement`交给虚拟机进行解析。
```c++
bool DB::parse_statement(std::string &input_line, Statement &statement)
{
    switch (prepare_statement(input_line, statement))
    {
    case PREPARE_SUCCESS:
        return false;
    case PREPARE_UNRECOGNIZED_STATEMENT:
        std::cout << "Unrecognized keyword at start of '" << input_line << "'." << std::endl;
        return true;
    }
    return false;
}
```
至此我们初步完成了解析前端的工作，根据`command`或者`sql`得到了我们所需要的`statement`。

## 3. 怎么实现一个虚拟机？
我们先根据得到的`statement`让虚拟机伪执行一下对应`sql`语句的操作效果。
```c++
void DB::excute_statement(Statement &statement)
{
    switch (statement.type)
    {
    case STATEMENT_INSERT:
        std::cout << "Executing insert statement" << std::endl;
        break;
    case STATEMENT_SELECT:
        std::cout << "Executing select statement" << std::endl;
        break;
    }
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

        Statement m_currentStatementType;

        if (parse_statement(input_line, m_currentStatementType))
        {
            continue;
        }

        excute_statement(m_currentStatementType);
    }
}
```
## 4. 总结
作为教程的第二章，我们引入了`解析前端`和`虚拟机`的概念，同时将各种状态映射到对应的枚举类型。至此，我们数据库的基本框架已经搭建完成，下一章就要一起来实现`数据存储`功能了。