# 如何用C++实现一个简易数据库（八）

## 1.如何按序排列和防重？
在上一章我们提到了，我们实际上插入`B-Tree`当中的数据时，需要按照`key`的顺序排列，这样才能保证`B-Tree`的顺序性。同时，既然存在`key`的顺序性，那么我们就需要确保`key`的唯一性。让我们来调整我们的测试。
```ruby
it "allows printing out the structure of a one-node btree" do
    script = [3, 1, 2].map do |i|
      "insert #{i} user#{i} person#{i}@example.com"
    end
    script << ".btree"
    script << ".exit"
    result = run_script(script)

    expect(result).to match_array([
                        "db > Executed.",
                        "db > Executed.",
                        "db > Executed.",
                        "db > Tree:",
                        "leaf (size 3)",
                        "  - 0 : 1",
                        "  - 1 : 2",
                        "  - 2 : 3",
                        "db > Bye!",
                      ])
  end
it "prints an error message if there is a duplicate id" do
    script = [
      "insert 1 user1 person1@example.com",
      "insert 1 user1 person1@example.com",
      "select",
      ".exit",
    ]
    result = run_script(script)
    expect(result).to match_array([
                        "db > Executed.",
                        "db > Error: Duplicate key.",
                        "db > (1, user1, person1@example.com)",
                        "Executed.",
                        "db > Bye!",
                      ])
  end
```

## 2. 如何按正确的顺序插入数据？
在之前，我们插入数据时总是考虑插入到`end_of_table`的位置，而不是根据我们所设置的`key`的顺序插入。现在让我们对此做出调整，先看最直接的`execute_insert`这个函数接口。
```c++
ExecuteResult DB::execute_insert(Statement &statement)
{
    LeafNode leaf_node = table->pager.get_page(table->root_page_num);
    uint32_t num_cells = *leaf_node.leaf_node_num_cells();
    if (num_cells >= LEAF_NODE_MAX_CELLS)
    {
        std::cout << "Leaf node full." << std::endl;
        return EXECUTE_TABLE_FULL;
    }

    Cursor *cursor = table->table_find(statement.row_to_insert.id);

    if (cursor->cell_num < num_cells)
    {
        uint32_t key_at_index = *leaf_node.leaf_node_key(cursor->cell_num);
        if (key_at_index == statement.row_to_insert.id)
        {
            return EXECUTE_DUPLICATE_KEY;
        }
    }
    cursor->leaf_node_insert(statement.row_to_insert.id, statement.row_to_insert);

    delete cursor;

    return EXECUTE_SUCCESS;
}
```
核心便是`table->table_find(statement.row_to_insert.id);`这返回一个`Cursor`对象，其指向了`key`应该插入的位置。

但让我们先不看实现，来看看最简单的防重 ***（DUPLICATE_KEY）*** 部分。
```c++
enum ExecuteResult
{
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL,
    EXECUTE_DUPLICATE_KEY
};
void DB::execute_statement(Statement &statement)
{
    ExecuteResult result;
    switch (statement.type)
    {
    case STATEMENT_INSERT:
        result = execute_insert(statement);
        break;
    case STATEMENT_SELECT:
        result = execute_select(statement);
        break;
    }

    switch (result)
    {
    case EXECUTE_SUCCESS:
        std::cout << "Executed." << std::endl;
        break;
    case (EXECUTE_DUPLICATE_KEY):
        std::cout << "Error: Duplicate key." << std::endl;
        break;
    case EXECUTE_TABLE_FULL:
        std::cout << "Error: Table full." << std::endl;
        break;
    }
}
```
我们对其增加了一个相应的`EXECUTE_DUPLICATE_KEY`枚举值，这样我们就可以在`execute_insert`函数中出现相应情况时，做出对应的错误提示了。

现在让我们回到`execute_insert`函数中的核心`table->table_find(statement.row_to_insert.id);`，看看它的实现。
```c++
Cursor *Table::table_find(uint32_t key)
{
    LeafNode root_node = pager.get_page(root_page_num);

    if (root_node.get_node_type() == NODE_LEAF)
    {
        return new Cursor(this, root_page_num, key);
    }
    else
    {
        std::cout << "Need to implement searching internal nodes." << std::endl;
        exit(EXIT_FAILURE);
    }
}
```
我们首先需要知道`root_page_num`，这是`B-Tree`的根节点的页号。然后，我们需要知道`root_node`，这是`B-Tree`的根节点。如果`root_node`是`LeafNode`，那么我们就可以进行查找了。所以我们需要为我们`LeafNode`类新增两个确定类型的函数。
```c++
    NodeType get_node_type()
    {
        uint8_t value = *((uint8_t *)((char *)node + NODE_TYPE_OFFSET));
        return (NodeType)value;
    }
    void set_node_type(NodeType type)
    {
        *((uint8_t *)((char *)node + NODE_TYPE_OFFSET)) = (uint8_t)type;
    }
```
让我们现在来看看如果是叶结点，我们需要返回一个怎样的`Cursor`对象呢？
```c++
Cursor::Cursor(Table *table, uint32_t page_num, uint32_t key)
{
    this->table = table;
    this->page_num = page_num;
    this->end_of_table = false;

    LeafNode root_node = table->pager.get_page(page_num);
    uint32_t num_cells = *root_node.leaf_node_num_cells();

    // Binary search
    uint32_t min_index = 0;
    uint32_t one_past_max_index = num_cells;
    while (one_past_max_index != min_index)
    {
        uint32_t index = (min_index + one_past_max_index) / 2;
        uint32_t key_at_index = *root_node.leaf_node_key(index);
        if (key == key_at_index)
        {
            this->cell_num = index;
            return;
        }
        if (key < key_at_index)
        {
            one_past_max_index = index;
        }
        else
        {
            min_index = index + 1;
        }
    }

    this->cell_num = min_index;
}
```
关键点就是一个二分搜索的过程，我们需要知道`min_index`和`one_past_max_index`，这两个变量分别代表`key`应该插入的位置的左右边界。当我们搜索到`key`应该插入的位置时，我们就可以知道`cell_num`了。也即可以完成对应的插入了。

让我们来看看测试情况。  
```
..........

Finished in 0.05035 seconds (files took 0.08038 seconds to load)
10 examples, 0 failures
```
非常棒，再一次测试通过了！

## 3. 总结
相较于前一章，本章内容实现起来比较容易，但已经初步完成了`B-Tree`的一个顺序排列结构。但注意到的点是，我们目前所有的操作，都仅仅基于一个结点，而不是一个整个`B-Tree`。我们甚至还没有开始创建不同的结点。在下一章，我们将介绍如何拆分叶结点，同时如何创造内部结点等方法等实现。
