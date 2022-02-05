# 如何用C++实现一个简易数据库（三）

## 1. 怎么初步完成我们的数据库？

现在我们先从最简单的开始，我们即将完成的数据库功能非常简单。
* 仅支持`insert`和`select`两项
* 将一切数据储存到内存中，即退出程序后一切数据丢失。
* 仅支持单个硬编码表

## 2. 如何修改我们的测试用例？
为了支持上述情况，我们需要修改我们的insert和select测试用例如下。
```ruby
it 'inserts and retrieves a row' do
    result = run_script([
      "insert 1 user1 person1@example.com",
      "insert 2 user2",
      "select",
      ".exit",
    ])
    expect(result).to match_array([
      "db > Executed.",
      "db > Syntax error. Could not parse statement.",
      "db > (1, user1, person1@example.com)",
      "Executed.",
      "db > Bye!",
    ])
  end
```
## 3. 怎么设计我们的储存结构？

我们目前规定所储存的类型结构如下
| 列       | 类型                    |
| -------- | ----------------------- |
| id       | 整型 ***(integer)***           |
| username | 可变字符串 ***(varchar 32)***  |
| email    | 可变字符串 ***(varchar 255)*** |

注意到我们这个时候没有使用 `std::string` 而使用的是 `char[]` 以限制长度。
```c++
#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
class Row
{
public:

    uint32_t id;
    char username[COLUMN_USERNAME_SIZE];
    char email[COLUMN_EMAIL_SIZE];

}; 
```
现在我们已经有了一行行的数据了，现在让我们将它合理的储存起来。 `sqlite` 使用 `B-Tree` 进行储存，这种优越的数据结构可以快速的进行查找，插入，删除。但是显然我们应该从最简单的开始，我们选择将它分组到 `页面` ***(Page)*** 当中去，然后将这些页面以数组的形式排列。此外，我们需要在一页中，尽可能的将其紧密排列，意味着数据应该一个挨着一个。
|列	|大小 ***(bytes)***|偏移量 ***(offset)***|
| -------- | ---- |---|
|id        |  4   |	0|
|username  |  32  |	4|
|email	   |  255 |36|
|总计	   |  291|  |
 
我们通过实现序列化 ***(serialize)*** 以及反序列化 ***(serialize)*** 来达成我们的目的。同时注意到我们这里写了一个`(char *)`的强制转化类型，是为了让编译器明白，偏移量 ***(offset)*** 是以单个字节 ***(bytes)*** 为单位的。
```c++
#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute)

const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;

void serialize_row(Row &source, void *destination)
{
    memcpy((char *)destination + ID_OFFSET, &(source.id), ID_SIZE);
    memcpy((char *)destination + USERNAME_OFFSET, &(source.username), USERNAME_SIZE);
    memcpy((char *)destination + EMAIL_OFFSET, &(source.email), EMAIL_SIZE);
}

void deserialize_row(void *source, Row &destination)
{
    memcpy(&(destination.id), (char *)source + ID_OFFSET, ID_SIZE);
    memcpy(&(destination.username), (char *)source + USERNAME_OFFSET, USERNAME_SIZE);
    memcpy(&(destination.email), (char *)source + EMAIL_OFFSET, EMAIL_SIZE);
}
```
接下来，让我们创建我们的`Table`来储存这些分页。同时我们与大多数计算机系统一样，将其设置为4k大小。
```c++
#define TABLE_MAX_PAGES 100
const uint32_t PAGE_SIZE = 4096;
const uint32_t ROWS_PER_PAGE = PAGE_SIZE / ROW_SIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;
class Table
{
public:
    uint32_t num_rows;
    void *pages[TABLE_MAX_PAGES];
    Table()
    {
        num_rows = 0;
        for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
        {
            pages[i] = NULL;
        }
    }
    ~Table()
    {
        for (int i = 0; pages[i]; i++)
        {
            free(pages[i]);
        }
    }
};
```
注意到，由于`C++`的构造和析构特性，使得我们在一定程度上减少了很多手动初始化和释放的代码工作量。

此外我们还应该知道，我们的表页该从何处开始读写。
```c++
void *row_slot(Table &table, uint32_t row_num)
{
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void *page = table.pages[page_num];
    if (page == NULL)
    {
        // Allocate memory only when we try to access page
        page = table.pages[page_num] = malloc(PAGE_SIZE);
    }
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return (char *)page + byte_offset;
}
```

## 4. 怎么实现执行储存操作？
现在我们已经设计好了储存结构，让我们来看一下怎么执行储存操作。首先，我们为`statement`添加我们的`row`属性。
```c++
class Statement
{
public:
    StatementType type;
    Row row_to_insert;
};
```
之后我们需要解析`insert`这个操作。为此，我们对添加`PrepareResult`新增一个解析句法错误状态`PREPARE_SYNTAX_ERROR`。我们使用`sscanf`来进行格式化读取输入，并将它储存到我们的`statement`当中去。
```c++
PrepareResult DB::prepare_statement(std::string &input_line, Statement &statement)
{
    if (!input_line.compare(0, 6, "insert"))
    {
        statement.type = STATEMENT_INSERT;
        int args_assigned = std::sscanf(
            input_line.c_str(), "insert %d %s %s", &(statement.row_to_insert.id),
            statement.row_to_insert.username, statement.row_to_insert.email);
        if (args_assigned < 3)
        {
            return PREPARE_SYNTAX_ERROR;
        }
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
现在让我们来真正设计执行`insert`与`select`操作。

对于`insert`操作，我们首先判断它是否超出储存限制，针对操作执行结果，我们同样添加了与我们之前类似的枚举类状态码
`enum ExecuteResult
{
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
};`
之后，若满足对应条件，我们寻找到合适的内存插入位置，将我们输入的行以`serialize_row`的方式填充到内存`page`当中。
```c++
ExecuteResult DB::execute_insert(Statement &statement, Table &table)
{
    if (table.num_rows >= TABLE_MAX_ROWS)
    {
        std::cout << "Error: Table full." << std::endl;
        return EXECUTE_TABLE_FULL;
    }

    void *page = row_slot(table, table.num_rows);
    serialize_row(statement.row_to_insert, page);
    table.num_rows++;

    return EXECUTE_SUCCESS;
}
```
类似的对于`select`操作，我们仅需从`page`中对应位置通过`deserialize_row`的方式获取到即可。
```c++
ExecuteResult DB::execute_select(Statement &statement, Table &table)
{
    for (uint32_t i = 0; i < table.num_rows; i++)
    {
        Row row;
        void *page = row_slot(table, i);
        deserialize_row(page, row);
        std::cout << "(" << row.id << ", " << row.username << ", " << row.email << ")" << std::endl;
    }

    return EXECUTE_SUCCESS;
}
```
最后将我们所设计好的操作交给`虚拟机`来执行即可。
```c++
void DB::execute_statement(Statement &statement, Table &table)
{
    ExecuteResult result;
    switch (statement.type)
    {
    case STATEMENT_INSERT:
        result = execute_insert(statement, table);
        break;
    case STATEMENT_SELECT:
        result = execute_select(statement, table);
        break;
    }

    switch (result)
    {
    case EXECUTE_SUCCESS:
        std::cout << "Executed." << std::endl;
        break;
    case EXECUTE_TABLE_FULL:
        std::cout << "Error: Table full." << std::endl;
        break;
    }
}

void DB::start()
{
    Table table;

    while (true)
    {
        print_prompt();

        std::string input_line;
        std::getline(std::cin, input_line);

        if (parse_meta_command(input_line))
        {
            continue;
        }

        Statement statement;

        if (parse_statement(input_line, statement))
        {
            continue;
        }

        execute_statement(statement, table);
    }
}
```
让我们来看看能否通过测试吧。
```
..

Finished in 0.00768 seconds (files took 0.07764 seconds to load)
2 examples, 0 failures
```
结果很不错，看上去我们的程序运行得很好，但是实际上我们还是需要更多的测试。
## 5.总结
我们已经初步实现了一个具有储存，查询固定表功能的一个数据库了。作为数据库，我们不可避免的涉及到了大量对内存指针的操作，这其中需要大家认真去理解每一个常量的设计原理以及考量。在下一章节，我们将对我们所写的数据库进行更加极端的边界测试，并且来一起修正其中的Bug。同时如果对本教程有所疑问或笔者存在错误，都欢迎在`评论区`或者`issue`中提出。