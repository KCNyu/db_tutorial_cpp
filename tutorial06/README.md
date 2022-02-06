# 如何用C++实现一个简易数据库（六）

## 1. 我们的光标需要做什么？
* 指向`table`开头
* 指向`table`结尾
* 指向`table`中的一个`row`
* 将`cursor`往后推进

## 2. 如何实现光标？
显然我们现在要指向`table`开头/结尾，所以我们需要实现一个`cursor`，它可以指向`table`开头，也可以指向`table`结尾。注意我们即然使用了`cursor`，也即`指向`这个词，我们在此处使用的就是`指针`，使用的其实一直就是`DB::table`唯一对象。
```c++
class Cursor
{
public:
    Table *table;
    uint32_t row_num;
    bool end_of_table;

    Cursor(Table *&table, bool option);
    void *cursor_value();
    void cursor_advance();
};
Cursor::Cursor(Table *&table, bool option)
{
    this->table = table;
    if (option)
    {
        // start at the beginning of the table
        row_num = 0;
        end_of_table = (table->num_rows == 0);
    }
    else
    {
        // end of the table
        row_num = table->num_rows;
        end_of_table = true;
    }
}
```
同时我们将不再使用`row_slot`这个函数，而转为使用`cursor_value`函数。
```c++
void *Cursor::cursor_value()
{
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void *page = table->pager.get_page(page_num);
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return (char *)page + byte_offset;
}
```
注意到，其实基本没有任何区别。

现在让我们来看看`cursor_advance`函数，它的作用是将`cursor`往后推进一个`row`。
```c++
void Cursor::cursor_advance()
{
    row_num += 1;
    if (row_num >= table->num_rows)
    {
        end_of_table = true;
    }
}
```

## 3. 如何使用我们的光标？
现在让我们来看如何使用光标进行`insert`操作。我们先创建一个指向`table`末尾的`cursor`，然后调用`cursor_value`函数便可直接获得对应分页信息。关键点其实，我们本质上调用的都是同一个`table`的`pager`的`get_page`函数，而`pager`的`get_page`函数的作用是从`pager`中获取分页信息。
```c++
ExecuteResult DB::execute_insert(Statement &statement)
{
    if (table->num_rows >= TABLE_MAX_ROWS)
    {
        std::cout << "Error: Table full." << std::endl;
        return EXECUTE_TABLE_FULL;
    }

    // end of the table
    Cursor *cursor = new Cursor(table, false);

    serialize_row(statement.row_to_insert, cursor->cursor_value());
    table->num_rows++;

    delete cursor;

    return EXECUTE_SUCCESS;
}
```
再看一下`select`操作，我们先创建一个指向`table`开头的`cursor`，然后同样调用`cursor_value`函数便可直接获得对应分页信息。再使用`cursor_advance`函数，将`cursor`往后推进一个`row`。
```c++
ExecuteResult DB::execute_select(Statement &statement)
{
    // start of the table
    Cursor *cursor = new Cursor(table, true);

    Row row;
    while (!cursor->end_of_table)
    {
        deserialize_row(cursor->cursor_value(), row);
        std::cout << "(" << row.id << ", " << row.username << ", " << row.email << ")" << std::endl;
        cursor->cursor_advance();
    }

    delete cursor;

    return EXECUTE_SUCCESS;
}
```
测试一下，看看是否有影响我们原有的功能。
```
.......

Finished in 0.0375 seconds (files took 0.07779 seconds to load)
7 examples, 0 failures
```
看样子还是非常不错的。此外，即然我们有使用大量的内存操作情况，我们可以使用`valgrind`来检查内存是否有问题。让我们来看一下`valgrind`的使用方法。

输入命令`valgrind --leak-check=yes ./db test.db`
```
==353480== Memcheck, a memory error detector
==353480== Copyright (C) 2002-2017, and GNU GPL'd, by Julian Seward et al.
==353480== Using Valgrind-3.15.0 and LibVEX; rerun with -h for copyright info
==353480== Command: ./db test.db
==353480== 
db > insert 1 person1 person1@example.com
Executed.
db > insert 2 person2 person2@example.com
Executed.
db > select
(1, user1, person1@example.com)
(1, person1, person1@example.com)
(2, person2, person2@example.com)
Executed.
db > .exit
Bye!
==353480== 
==353480== HEAP SUMMARY:
==353480==     in use at exit: 0 bytes in 0 blocks
==353480==   total heap usage: 12 allocs, 12 frees, 79,896 bytes allocated
==353480== 
==353480== All heap blocks were freed -- no leaks are possible
==353480== 
==353480== For lists of detected and suppressed errors, rerun with: -s
==353480== ERROR SUMMARY: 0 errors from 0 contexts (suppressed: 0 from 0)
```
内存检查结果看起来很不错，没有发现任何内存泄露。

## 4. 总结

注意一点，我们实际还并没有完成我们上一章所说的减少对硬盘的重复刷新，因为事实上我们到目前为止还使用的是`数组`型结构储存，实现它的功能并没有什么意义。我们将在后续章节中详细讨论`B-Tree`型结构。这将极大提高我们的效率。