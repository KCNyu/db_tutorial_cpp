# 如何用C++实现一个简易数据库（九）

## 1. 如何分裂叶子结点？

按照惯例，我们先增加一个测试用例，对应的已经是 `3-leaf-node btree` 。

```ruby
it "allows printing out the structure of a 3-leaf-node btree" do
    script = (1..14).map do |i|
      "insert #{i} user#{i} person#{i}@example.com"
    end
    script << ".btree"
    script << "insert 15 user15 person15@example.com"
    script << ".exit"
    result = run_script(script)

    expect(result[14...(result.length)]).to match_array([
      "db > Tree:",
      "- internal (size 1)",
      "  - leaf (size 7)",
      "    - 1",
      "    - 2",
      "    - 3",
      "    - 4",
      "    - 5",
      "    - 6",
      "    - 7",
      "  - key 7",
      "  - leaf (size 7)",
      "    - 8",
      "    - 9",
      "    - 10",
      "    - 11",
      "    - 12",
      "    - 13",
      "    - 14",
      "db > Need to implement searching an internal node.",
    ])
  end
```

此外需要对之前的测试用例进行修改，因为目前限制的不再是 `Table full` 而是我们还没有实现对 `Internal node` 进行分裂。

```ruby
it "prints error message when table is full" do
    script = (1..900).map do |i|
      "insert #{i} user#{i} person#{i}@example.com"
    end
    script << ".exit"
    result = run_script(script)
    expect(result.last(2)).to match_array([
      "db > Executed.",
      "db > Need to implement searching an internal node.",
    ])
  end
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
                        "- leaf (size 3)",
                        "  - 1",
                        "  - 2",
                        "  - 3",
                        "db > Bye!",
                      ])
  end
```

现在让我们来从 `Cursor::leaf_node_insert` 这个接口入，在其中新增 `leaf_node_split_and_insert` 这个方法。
```c++
void Cursor::leaf_node_insert(uint32_t key, Row &value)
{

    LeafNode leaf_node = table->pager.get_page(page_num);
    uint32_t num_cells = *leaf_node.leaf_node_num_cells();

    if (num_cells >= LEAF_NODE_MAX_CELLS)
    {
        // Node full
        leaf_node_split_and_insert(key, value);
        return;
    }

    if (cell_num < num_cells)
    {
        // make room for new cell
        for (uint32_t i = num_cells; i > cell_num; i--)
        {
            memcpy(leaf_node.leaf_node_cell(i), leaf_node.leaf_node_cell(i - 1),
                   LEAF_NODE_CELL_SIZE);
        }
    }

    // insert new cell
    *leaf_node.leaf_node_num_cells() += 1;
    *leaf_node.leaf_node_key(cell_num) = key;
    serialize_row(value, leaf_node.leaf_node_value(cell_num));

}

```
除此之外，我们还需要修改`DB::execute_insert`方法
```c++
ExecuteResult DB::execute_insert(Statement &statement)
{
    LeafNode leaf_node = table->pager.get_page(table->root_page_num);
    uint32_t num_cells = *leaf_node.leaf_node_num_cells();

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

其中正如我之前所说，不再需要 `return EXECUTE_TABLE_FULL` ，因为我们将要实现对 `Leaf node` 进行分裂。

## 2. 如何制定叶子结点分裂标准？

当叶子结点满了之后，我们需要对其进行分裂，这个分裂标准是什么？

我们需要将现有的 `cell` 分成两个部分：上半部分与下半部分。上半部分的 `key` 严格大于下半部分的 `key` ，这样我们就可以将 `cell` 分成两个部分。因此，我们分配一个新的叶子结点，并将对应的上半部分移入该叶子结点。
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
            serialize_row(value, destination.get_node());
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
请注意，在我们移动结点后，需要更新对应修改后结点储存的`num cells`。

我们目前仅使用最简单的分配页面方式，即直接返回一个新页面。
```c++
/*
Until we start recycling free pages, new pages will always
go onto the end of the database file
*/
uint32_t Pager::get_unused_page_num()
{
    return num_pages;
}
```

此外，分裂结点时，我们还需要考虑尽可能的均匀分配。
```c++
const uint32_t LEAF_NODE_RIGHT_SPLIT_COUNT = (LEAF_NODE_MAX_CELLS + 1) / 2; 
const uint32_t LEAF_NODE_LEFT_SPLIT_COUNT =

    (LEAF_NODE_MAX_CELLS + 1) - LEAF_NODE_RIGHT_SPLIT_COUNT;

```

最后，我们还需要更新其对应的父结点，创造一个新的根结点作为父结点。我们使用右孩子结点作为输入，并且分配一个新页面用于储存左孩子结点。
```c++
void Table::create_new_root(uint32_t right_child_page_num)
{
    /*
    Handle splitting the root.
    Old root copied to new page, becomes left child.
    Address of right child passed in.
    Re-initialize root page to contain the new root node.
    New root node points to two children.
    */

    InternalNode root = pager.get_page(root_page_num);
    Node right_child = pager.get_page(right_child_page_num);
    uint32_t left_child_page_num = pager.get_unused_page_num();
    Node left_child = pager.get_page(left_child_page_num);

    /* Left child has data copied from old root */
    memcpy(left_child.get_node(), root.get_node(), PAGE_SIZE);
    left_child.set_node_root(false);

    /* Root node is a new internal node with one key and two children */
    root.initialize_internal_node();
    root.set_node_root(true);
    *root.internal_node_num_keys() = 1;
    *root.internal_node_child(0) = left_child_page_num;
    uint32_t left_child_max_key = left_child.get_node_max_key();
    *root.internal_node_key(0) = left_child_max_key;
    *root.internal_node_right_child() = right_child_page_num;
}
```

现在我们已经成功将根结点创建成功了，它具有两个子结点和一个最为重要的内部结点。

## 3. 如何实现内部结点？

首先，既然我们现在有了叶子结点和内部结点两种区别，我们可以把它们分别定义为两个类： `LeafNode` 和 `InternalNode` 。他们继承自 `Node` 类，并且涵盖了 `Node` 类的所有方法。

先看看基底类 `Node` ：
```c++
class Node
{
protected:

    void *node;

public:

    Node() {}
    Node(void *node) : node(node) {}

    NodeType get_node_type()
    {
        uint8_t value = *((uint8_t *)((char *)node + NODE_TYPE_OFFSET));
        return (NodeType)value;
    }
    void set_node_type(NodeType type)
    {
        *((uint8_t *)((char *)node + NODE_TYPE_OFFSET)) = (uint8_t)type;
    }
    void *get_node()
    {
        return node;
    }
    bool is_node_root()
    {
        uint8_t value = *((uint8_t *)((char *)node + IS_ROOT_OFFSET));
        return value == 1;
    }
    void set_node_root(bool is_root)
    {
        *((uint8_t *)((char *)node + IS_ROOT_OFFSET)) = is_root ? 1 : 0;
    }
    virtual uint32_t get_node_max_key();

}; 

```
基本上与前面章节的`LeafNode`一致，只不过从中只抽离了部分公共方法。同时我们设置了是否为`root`结点的标志位。所以我们在创造最初的结点时，可以设置`root`结点的标志位。
```c++
    Table(const char *filename) : pager(filename)
    {
        root_page_num = 0;
        if (pager.num_pages == 0)
        {
            // New file. Initialize page 0 as leaf node.
            LeafNode root_node = pager.get_page(0);
            root_node.initialize_leaf_node();
            root_node.set_node_root(true);
        }
    }
```

让我们来看看现在的`LeafNode`：
```c++
class LeafNode : public Node
{
public:
    LeafNode() {}
    LeafNode(void *node) : Node(node) {}

    void initialize_leaf_node()
    {
        set_node_type(NODE_LEAF);
        set_node_root(false);
        *leaf_node_num_cells() = 0;
    }
    uint32_t *leaf_node_num_cells()
    {
        return (uint32_t *)((char *)node + LEAF_NODE_NUM_CELLS_OFFSET);
    }
    void *leaf_node_cell(uint32_t cell_num)
    {
        return (char *)node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
    }
    uint32_t *leaf_node_key(uint32_t cell_num)
    {
        return (uint32_t *)leaf_node_cell(cell_num);
    }
    void *leaf_node_value(uint32_t cell_num)
    {
        return (char *)leaf_node_cell(cell_num) + LEAF_NODE_KEY_SIZE;
    }
    uint32_t get_node_max_key() override
    {
        return *leaf_node_key(*leaf_node_num_cells() - 1);
    }
};
```

可以看到，继承补全一些专属方法后， `LeafNode` 类基本与前面章节的 `LeafNode` 类一致了。

再看最为重要的 `InternalNode` ：
```c++
class InternalNode : public Node
{
public:

    InternalNode() {}
    InternalNode(void *node) : Node(node) {}

    void initialize_internal_node()
    {
        set_node_type(NODE_INTERNAL);
        set_node_root(false);
        *internal_node_num_keys() = 0;
    }
    uint32_t *internal_node_num_keys()
    {
        return (uint32_t *)((char *)node + INTERNAL_NODE_NUM_KEYS_OFFSET);
    }
    u_int32_t *internal_node_right_child()
    {
        return (u_int32_t *)((char *)node + INTERNAL_NODE_RIGHT_CHILD_OFFSET);
    }
    uint32_t *internal_node_cell(uint32_t cell_num)
    {
        return (uint32_t *)((char *)node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE);
    }
    uint32_t *internal_node_child(uint32_t child_num)
    {
        uint32_t num_keys = *internal_node_num_keys();
        if (child_num > num_keys)
        {
            std::cout << "Tried to access child_num " << child_num << " > num_keys " << num_keys << std::endl;
            exit(EXIT_FAILURE);
        }
        else if (child_num == num_keys)
        {
            return internal_node_right_child();
        }
        else
        {
            return internal_node_cell(child_num);
        }
    }
    uint32_t *internal_node_key(uint32_t key_num)
    {
        return internal_node_cell(key_num) + INTERNAL_NODE_CHILD_SIZE;
    }
    uint32_t get_node_max_key() override
    {
        return *internal_node_key(*internal_node_num_keys() - 1);
    }

}; 

```
现在让我们来分析一下它的基本常量是如何定义的：
```c++
/*
 * Internal Node Header Layout
 */
const uint32_t INTERNAL_NODE_NUM_KEYS_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_NUM_KEYS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t INTERNAL_NODE_RIGHT_CHILD_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_RIGHT_CHILD_OFFSET =
    INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE;
const uint32_t INTERNAL_NODE_HEADER_SIZE = COMMON_NODE_HEADER_SIZE +
                                           INTERNAL_NODE_NUM_KEYS_SIZE +
                                           INTERNAL_NODE_RIGHT_CHILD_SIZE;
```
它的头部我们将储存包含的`key`的数量，同时储存右孩子结点的`page_num`。通过它，我们可以获得孩子结点的相关信息。此时`page_num`即类似于相应的孩子指针。
```c++
/*
 * Internal Node Body Layout
 */
const uint32_t INTERNAL_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CHILD_SIZE = sizeof(uint32_t);
const uint32_t INTERNAL_NODE_CELL_SIZE =
    INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE;
```
主体部分，每个`cell`都包含一个左孩子结点的`key`和相应的`page_num`。每个`key`值都代表了在该左孩子结点中的最大`key`值。

## 4. 如何打印现在的树？
我们使用递归的方法来打印树，这样就可以让我们看到树的结构了。
```c++
void indent(uint32_t level)
{
    for (uint32_t i = 0; i < level; i++)
    {
        std::cout << "  ";
    }
}
void Pager::print_tree(uint32_t page_num, uint32_t indentation_level)
{
    Node *node = new Node(get_page(page_num));
    uint32_t num_keys, child;

    switch (node->get_node_type())
    {
    case (NODE_LEAF):
        num_keys = *((LeafNode *)node)->leaf_node_num_cells();
        indent(indentation_level);
        std::cout << "- leaf (size " << num_keys << ")" << std::endl;
        for (uint32_t i = 0; i < num_keys; i++)
        {
            indent(indentation_level + 1);
            std::cout << "- " << *((LeafNode *)node)->leaf_node_key(i) << std::endl;
        }
        break;
    case (NODE_INTERNAL):
        num_keys = *((InternalNode *)node)->internal_node_num_keys();
        indent(indentation_level);
        std::cout << "- internal (size " << num_keys << ")" << std::endl;
        for (uint32_t i = 0; i < num_keys; i++)
        {
            child = *((InternalNode *)node)->internal_node_child(i);
            print_tree(child, indentation_level + 1);

            indent(indentation_level + 1);
            std::cout << "- key " << *((InternalNode *)node)->internal_node_key(i) << std::endl;
        }
        child = *((InternalNode *)node)->internal_node_right_child();
        print_tree(child, indentation_level + 1);
        break;
    }
}
```
它接收任意结点指针，即对应的`page_num`来作为参数打印，同时使用`indent`作为深度结构的区分。

现在让我们来测试结果
```
...........

Finished in 0.03823 seconds (files took 0.07876 seconds to load)
11 examples, 0 failures
```
艰难的修改之后，我们通过了所有的测试。

## 5. 总结
在本章节中，我们把`Node`做出了两个类`LeafNode`和`InternalNode`的区分继承，尽管它本身内部已经储存了相应的类型信息，但是我们作为对象来说，还是应该规范其所能调用函数的范围。此外，我们注意到我们现在的树还具有许多的缺陷，譬如在插入多结点时，并不能很好的完成插入，在下一章中，我们将会把这些缺陷解决。