# 如何用C++实现一个简易数据库（十一）

## 1. `Cursor`存在什么问题？
我们在上一章提到了，我们现在使用的`Cursor`在扫描多层级树的时候存在问题，但究竟是什么问题呢？让我们先来增加一个测试用例。
```Ruby
  it "prints all rows in a multi-level tree" do
    script = []
    (1..15).each do |i|
      script << "insert #{i} user#{i} person#{i}@example.com"
    end
    script << "select"
    script << ".exit"
    result = run_script(script)
    expect(result[15...result.length]).to match_array([
      "db > (1, user1, person1@example.com)",
      "(2, user2, person2@example.com)",
      "(3, user3, person3@example.com)",
      "(4, user4, person4@example.com)",
      "(5, user5, person5@example.com)",
      "(6, user6, person6@example.com)",
      "(7, user7, person7@example.com)",
      "(8, user8, person8@example.com)",
      "(9, user9, person9@example.com)",
      "(10, user10, person10@example.com)",
      "(11, user11, person11@example.com)",
      "(12, user12, person12@example.com)",
      "(13, user13, person13@example.com)",
      "(14, user14, person14@example.com)",
      "(15, user15, person15@example.com)",
      "Executed.", "db > Bye!",
    ])
  end
  ```
  直接运行来看一看结果
  ```Ruby
  ...........F

Failures:

  1) database prints all rows in a multi-level tree
     Failure/Error:
       expect(result[15...result.length]).to match_array([
         "db > (1, user1, person1@example.com)",
         "(2, user2, person2@example.com)",
         "(3, user3, person3@example.com)",
         "(4, user4, person4@example.com)",
         "(5, user5, person5@example.com)",
         "(6, user6, person6@example.com)",
         "(7, user7, person7@example.com)",
         "(8, user8, person8@example.com)",
         "(9, user9, person9@example.com)",
     
       expected collection contained:  ["(10, user10, person10@example.com)", "(11, user11, person11@example.com)", "(12, user12, person12@e..."(9, user9, person9@example.com)", "Executed.", "db > (1, user1, person1@example.com)", "db > Bye!"]
       actual collection contained:    ["Executed.", "db > (2, user1, person1@example.com)", "db > Bye!"]
       the missing elements were:      ["(10, user10, person10@example.com)", "(11, user11, person11@example.com)", "(12, user12, person12@e...8, person8@example.com)", "(9, user9, person9@example.com)", "db > (1, user1, person1@example.com)"]
       the extra elements were:        ["db > (2, user1, person1@example.com)"]
     # ./db_test.rb:251:in `block (2 levels) in <top (required)>'

Finished in 0.05596 seconds (files took 0.07827 seconds to load)
12 examples, 1 failure

Failed examples:

rspec ./db_test.rb:243 # database prints all rows in a multi-level tree
```
我们只成功打印出来了` ["db > (2, user1, person1@example.com)"]`这个奇怪的数据。

按道理说，我们使用`.btree`时，`Cursor`的行为是正确的，但是当我们使用`select`时，`Cursor`的行为就不正确了。

## 2. 如何修复我们的`Cursor`？
首先定位问题，我们调用`select`函数时，返回的是一个`Cursor`对象，并且指向的是`start of the table`。但是我们现在的根结点实际上是一个内部结点，并不包含任何真实的`Row`。所以此时，我们需要将`Cursor`指向真正的`start of the table`，也即最左边叶子结点的初始`cell`。
```c++
Cursor::Cursor(Table *table)
{
    Cursor* cursor = table->table_find(0);
    this->table = table;
    this->page_num = cursor->page_num;
    this->cell_num = cursor->cell_num;

    LeafNode root_node = table->pager.get_page(cursor->page_num);
    uint32_t num_cells = *root_node.leaf_node_num_cells();

    this->end_of_table = (num_cells == 0);
}
```
我们通过`table->find`来找到真正的初始`Cursor`，然后将得到的`Cursor`中的数据一一对应的复制给我们`this->page_num`和`this->cell_num`。

现在让我们来运行测试一下，看看结果是否正确。
```Ruby
...........F

Failures:

  1) database prints all rows in a multi-level tree
     Failure/Error:
       expect(result[15...result.length]).to match_array([
         "db > (1, user1, person1@example.com)",
         "(2, user2, person2@example.com)",
         "(3, user3, person3@example.com)",
         "(4, user4, person4@example.com)",
         "(5, user5, person5@example.com)",
         "(6, user6, person6@example.com)",
         "(7, user7, person7@example.com)",
         "(8, user8, person8@example.com)",
         "(9, user9, person9@example.com)",
     
       expected collection contained:  ["(10, user10, person10@example.com)", "(11, user11, person11@example.com)", "(12, user12, person12@e..."(9, user9, person9@example.com)", "Executed.", "db > (1, user1, person1@example.com)", "db > Bye!"]
       actual collection contained:    ["(2, user2, person2@example.com)", "(3, user3, person3@example.com)", "(4, user4, person4@example.co..."(7, user7, person7@example.com)", "Executed.", "db > (1, user1, person1@example.com)", "db > Bye!"]
       the missing elements were:      ["(10, user10, person10@example.com)", "(11, user11, person11@example.com)", "(12, user12, person12@e...ser15, person15@example.com)", "(8, user8, person8@example.com)", "(9, user9, person9@example.com)"]
     # ./db_test.rb:251:in `block (2 levels) in <top (required)>'

Finished in 0.05607 seconds (files took 0.08103 seconds to load)
12 examples, 1 failure

Failed examples:

rspec ./db_test.rb:243 # database prints all rows in a multi-level tree
```
问题发生了变化，我们只能打印出单个叶子结点的数据。第二个叶子结点我们无法连续的打印出来。

## 3. 如何连续打印出所有的叶子结点？
为了保证我们能够在打印到第一个叶子结点的末端时，自动跳转到第二个叶子结点,我们在其标头设置相应的`next_leaf`字段来指向下一个叶子结点。
```c++
/*
 * Leaf Node Header Layout
 */
const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_NEXT_LEAF_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NEXT_LEAF_OFFSET =
    LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
                                       LEAF_NODE_NUM_CELLS_SIZE +
                                       LEAF_NODE_NEXT_LEAF_SIZE;
 ```        
我们需要给`LeafNode`增加一个`leaf_node_next_leaf`方法。
```c++
    u_int32_t* leaf_node_next_leaf()
    {
        return (u_int32_t *)((char *)node + LEAF_NODE_NEXT_LEAF_OFFSET);
    }
```
同时在相应的`initialize`函数中也要对`next_leaf`相关做出修改。
```c++
    void initialize_leaf_node()
    {
        set_node_type(NODE_LEAF);
        set_node_root(false);
        *leaf_node_num_cells() = 0;
        *leaf_node_next_leaf() = 0;  // 0 represents no sibling
    }
```
注意，我们拆分叶子结点时，还需要补充更新新老结点的`next_leaf`字段的继承关系。
```c++
void Cursor::leaf_node_split_and_insert(uint32_t key, Row &value)
{
    /*
    Create a new node and move half the cells over.
    Insert the new value in one of the two nodes.
    Update parent or create a new parent.
    */

    LeafNode old_node = table->pager.get_page(page_num);
    uint32_t new_page_num = table->pager.get_unused_page_num();
    LeafNode new_node = table->pager.get_page(new_page_num);
    new_node.initialize_leaf_node();
    *new_node.leaf_node_next_leaf() = *old_node.leaf_node_next_leaf();
    *old_node.leaf_node_next_leaf() = new_page_num;

    /*
    All existing keys plus new key should be divided
    evenly between old (left) and new (right) nodes.
    Starting from the right, move each key to correct position.
    */
    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--)
    {
        LeafNode destination_node;
        if (i >= LEAF_NODE_LEFT_SPLIT_COUNT)
        {
            destination_node = new_node;
        }
        else
        {
            destination_node = old_node;
        }
        uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
        LeafNode destination = destination_node.leaf_node_cell(index_within_node);

        if (i == cell_num)
        {
            serialize_row(value,
                          destination_node.leaf_node_value(index_within_node));
            *destination_node.leaf_node_key(index_within_node) = key;
        }
        else if (i > cell_num)
        {
            memcpy(destination.get_node(), old_node.leaf_node_cell(i - 1), LEAF_NODE_CELL_SIZE);
        }
        else
        {
            memcpy(destination.get_node(), old_node.leaf_node_cell(i), LEAF_NODE_CELL_SIZE);
        }
    }
    /* Update cell count on both leaf nodes */
    *old_node.leaf_node_num_cells() = LEAF_NODE_LEFT_SPLIT_COUNT;
    *new_node.leaf_node_num_cells() = LEAF_NODE_RIGHT_SPLIT_COUNT;

    if (old_node.is_node_root())
    {
        return table->create_new_root(new_page_num);
    }
    else
    {
        std::cout << "Need to implement updating parent after split" << std::endl;
        exit(EXIT_FAILURE);
    }
}
```
所以，我们现在可以愉快的修改`cursor_advance`函数，以便在找到叶子结点后，自动跳转到下一个叶子结点。
```c++
void Cursor::cursor_advance()
{
    LeafNode leaf_node = table->pager.get_page(page_num);
    cell_num += 1;
    if (cell_num >= *leaf_node.leaf_node_num_cells())
    {
        /* Advance to next leaf node */
        uint32_t next_page_num = *leaf_node.leaf_node_next_leaf();
        if (next_page_num == 0)
        {
            /* This was rightmost leaf */
            end_of_table = true;
        }
        else
        {
            page_num = next_page_num;
            cell_num = 0;
        }
    }
}
```
最后，我们由于修改了部分常量，所以在测试用例中也要做出一些相应的修改。
```Ruby
  it "prints constants" do
    script = [
      ".constants",
      ".exit",
    ]
    result = run_script(script)

    expect(result).to match_array([
                        "db > Constants:",
                        "ROW_SIZE: 293",
                        "COMMON_NODE_HEADER_SIZE: \u0006",
                        "LEAF_NODE_HEADER_SIZE: 14",
                        "LEAF_NODE_CELL_SIZE: 297",
                        "LEAF_NODE_SPACE_FOR_CELLS: 4082",
                        "LEAF_NODE_MAX_CELLS: 13",
                        "db > Bye!",
                      ])
  end
```
现在让我们来测试一下，我们能不能打印出所有的数据了。
```Ruby
............

Finished in 0.06438 seconds (files took 0.09934 seconds to load)
12 examples, 0 failures
```
恭喜通过了所有的测试。

## 4. 总结
我们通过`TDD`能够很好的发现问题，并且能够解决问题。同时我们需要不断的定义和修改`Node`的字段信息来达到更好的解决方案，同时需要对相应的函数做出相应的修改。下一章我们将对父结点在拆分后进行更新，以保证能够完成更大数据量乱序的插入测试。