# 如何用C++实现一个简易数据库（十一）

## 1. 为什么要更新拆分后的父结点？

现在让我们来加入一个乱序的测试用例来证明我们的需求。

```Ruby
it "allows printing out the structure of a 4-leaf-node btree" do
    script = [
      "insert 18 user18 person18@example.com",
      "insert 7 user7 person7@example.com",
      "insert 10 user10 person10@example.com",
      "insert 29 user29 person29@example.com",
      "insert 23 user23 person23@example.com",
      "insert 4 user4 person4@example.com",
      "insert 14 user14 person14@example.com",
      "insert 30 user30 person30@example.com",
      "insert 15 user15 person15@example.com",
      "insert 26 user26 person26@example.com",
      "insert 22 user22 person22@example.com",
      "insert 19 user19 person19@example.com",
      "insert 2 user2 person2@example.com",
      "insert 1 user1 person1@example.com",
      "insert 21 user21 person21@example.com",
      "insert 11 user11 person11@example.com",
      "insert 6 user6 person6@example.com",
      "insert 20 user20 person20@example.com",
      "insert 5 user5 person5@example.com",
      "insert 8 user8 person8@example.com",
      "insert 9 user9 person9@example.com",
      "insert 3 user3 person3@example.com",
      "insert 12 user12 person12@example.com",
      "insert 27 user27 person27@example.com",
      "insert 17 user17 person17@example.com",
      "insert 16 user16 person16@example.com",
      "insert 13 user13 person13@example.com",
      "insert 24 user24 person24@example.com",
      "insert 25 user25 person25@example.com",
      "insert 28 user28 person28@example.com",
      ".btree",
      ".exit",
    ]
    result = run_script(script)

    expect(result[22...(result.length)]).to match_array([
      "db > Tree:",
      "- internal (size 3)",
      "  - leaf (size 7)",
      "    - 1",
      "    - 2",
      "    - 3",
      "    - 4",
      "    - 5",
      "    - 6",
      "    - 7",
      "  - key 7",
      "  - leaf (size 8)",
      "    - 8",
      "    - 9",
      "    - 10",
      "    - 11",
      "    - 12",
      "    - 13",
      "    - 14",
      "    - 15",
      "  - key 15",
      "  - leaf (size 7)",
      "    - 16",
      "    - 17",
      "    - 18",
      "    - 19",
      "    - 20",
      "    - 21",
      "    - 22",
      "  - key 22",
      "  - leaf (size 8)",
      "    - 23",
      "    - 24",
      "    - 25",
      "    - 26",
      "    - 27",
      "    - 28",
      "    - 29",
      "    - 30",
      "db > Bye!",
    ])
  end
```

请注意 `expect(result[22...(result.length)])` 我专门设置了**22**这个数字，而非实际的插入条数**30**，现在让我们来运行一下看看结果如何。

```Ruby
............F

Failures:

  1) database allows printing out the structure of a 4-leaf-node btree
     Failure/Error:
       expect(result[22...(result.length)]).to match_array([
         "db > Tree:",
         "- internal (size 3)",
         "  - leaf (size 7)",
         "    - 1",
         "    - 2",
         "    - 3",
         "    - 4",
         "    - 5",
         "    - 6",
     
       expected collection contained:  ["    - 1", "    - 10", "    - 11", "    - 12", "    - 13", "    - 14", "    - 15", "    - 16", "    ...f (size 7)", "  - leaf (size 8)", "  - leaf (size 8)", "- internal (size 3)", "db > ", "db > Tree:"]
       actual collection contained:    ["db > Need to implement updating parent after split"]
       the missing elements were:      ["    - 1", "    - 10", "    - 11", "    - 12", "    - 13", "    - 14", "    - 15", "    - 16", "    ...f (size 7)", "  - leaf (size 8)", "  - leaf (size 8)", "- internal (size 3)", "db > ", "db > Tree:"]
       the extra elements were:        ["db > Need to implement updating parent after split"]
     # ./db_test.rb:308:in `block (2 levels) in <top (required)>'

Finished in 0.05649 seconds (files took 0.07963 seconds to load)
13 examples, 1 failure

Failed examples:

rspec ./db_test.rb:271 # database allows printing out the structure of a 4-leaf-node btree
```

前面的 `Executed` 部分被我省略，目前我们现在已经到了 `Need to implement updating parent after split` 这一步。

## 2. 如何实现更新拆分后的父结点？

直接看我们调用的接口 `Cursor::leaf_node_split_and_insert` ，它的实现如下：
```c++
void Cursor::leaf_node_split_and_insert(uint32_t key, Row &value)
{

    /*
    Create a new node and move half the cells over.
    Insert the new value in one of the two nodes.
    Update parent or create a new parent.
    */

    LeafNode old_node = table->pager.get_page(page_num);
    uint32_t old_max = old_node.get_node_max_key();

    uint32_t new_page_num = table->pager.get_unused_page_num();
    LeafNode new_node = table->pager.get_page(new_page_num);
    new_node.initialize_leaf_node();

    *new_node.node_parent() = *old_node.node_parent();

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
        uint32_t parent_page_num = *old_node.node_parent();
        uint32_t new_max = old_node.get_node_max_key();
        InternalNode parent = table->pager.get_page(parent_page_num);
        parent.update_internal_node_key(old_max, new_max);
        internal_node_insert(parent_page_num, new_page_num);
        return;
    }

}

```
我们直接关注最后的`else`部分，现在我们需要在父节点中找到受影响的`cell`，但是对应的这个孩子结点不知道自己的`page_num`，所以我们无法查找。但它知道自己的`max_key`，因此我们可以在父密钥中搜索该密钥。.我们在此来调用一个接口`InternalNode::update_internal_node_key`，它的实现如下：
```c++
    void update_internal_node_key(uint32_t old_key, uint32_t new_key)
    {
        uint32_t old_child_index = internal_node_find_child(old_key);
        *internal_node_key(old_child_index) = new_key;
    }
```

此外我们现在为了获得父结点的引用，我们需要开始在每个结点中记录指向其父节点的指针。同样为 `Node` 添加这样一个方法。

```
    uint32_t *node_parent()
    {
        return (uint32_t *)((char *)node + PARENT_POINTER_OFFSET);
    }
```

我们在 `Table::create_new_root` 中最后两行增加储存对应父结点的指针的代码即可。
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

    *left_child.node_parent() = root_page_num;
    *right_child.node_parent() = root_page_num;

}

```
我们再来重构一下`Table::internal_node_find`的方法，将其中拆分出`InternalNode::internal_node_find_child`方法。
```c++
Cursor *Table::internal_node_find(uint32_t page_num, uint32_t key)
{
    InternalNode node = pager.get_page(page_num);

    uint32_t child_index = node.internal_node_find_child(key);
    uint32_t child_num = *node.internal_node_child(child_index);
    Node child = pager.get_page(child_num);
    switch (child.get_node_type())
    {
    case NODE_INTERNAL:
        return internal_node_find(child_num, key);
    case NODE_LEAF:
    default:
        return new Cursor(this, child_num, key);
    }
}
uint32_t internal_node_find_child(uint32_t key)
{
      /*
      Return the index of the child which should contain
      the given key.
      */

      uint32_t num_keys = *internal_node_num_keys();

      /* Binary search */
      uint32_t min_index = 0;
        uint32_t max_index = num_keys; /* there is one more child than key */

        while (min_index != max_index)
        {
            uint32_t index = (min_index + max_index) / 2;
            uint32_t key_to_right = *internal_node_key(index);
            if (key_to_right >= key)
            {
                max_index = index;
            }
            else
            {
                min_index = index + 1;
            }
        }

        return min_index;
}
```
好的，现在我们已经完成了基本的前置工作，下面让我们来将构筑好的`InternalNode`插入。

## 3. 如何插入一个`InternalNode`？
我们首先限制一下单个`InternalNode`的最大`cell`数目，这样我们就可以在插入的时候进行判断了。
```c++
/* Keep this small for testing */
const uint32_t INTERNAL_NODE_MAX_CELLS = 3;
```
现在让我们来直接看看`Cursor::internal_node_insert`的实现。
```c++
void Cursor::internal_node_insert(uint32_t parent_page_num, uint32_t child_page_num)
{
    /*
    Add a new child/key pair to parent that corresponds to child
    */

    InternalNode parent = table->pager.get_page(parent_page_num);
    Node child = table->pager.get_page(child_page_num);
    uint32_t child_max_key = child.get_node_max_key();
    uint32_t index = parent.internal_node_find_child(child_max_key);

    uint32_t original_num_keys = *parent.internal_node_num_keys();
    *parent.internal_node_num_keys() = original_num_keys + 1;

    if (original_num_keys >= INTERNAL_NODE_MAX_CELLS)
    {
        std::cout << "Need to implement splitting internal node" << std::endl;
        exit(EXIT_FAILURE);
    }

    uint32_t right_child_page_num = *parent.internal_node_right_child();
    Node right_child = table->pager.get_page(right_child_page_num);

    if (child_max_key > right_child.get_node_max_key())
    {
        /* Replace right child */
        *parent.internal_node_child(original_num_keys) = right_child_page_num;
        *parent.internal_node_key(original_num_keys) =
            right_child.get_node_max_key();
        *parent.internal_node_right_child() = child_page_num;
    }
    else
    {
        /* Make room for the new cell */
        for (uint32_t i = original_num_keys; i > index; i--)
        {
            void *destination = parent.internal_node_cell(i);
            void *source = parent.internal_node_cell(i - 1);
            memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
        }
        *parent.internal_node_child(index) = child_page_num;
        *parent.internal_node_key(index) = child_max_key;
    }
}
```
在插入一个新的`cell`取决于该`cell`对应的`max_key`值，我们首先需要找到该`cell`对应的`max_key`值，同时如果当前`cell`数目已经超出，我们需要进行分裂（还未实现）。由于我们把最右边的孩子结点指针（`key_max`对应的结点）与其他孩子结点分开存储，所以如果新孩子结点要成为最右边的孩子结点，我们必须以不同的方式处理事情。要么代替原来的最右边的孩子结点，要么把新的孩子结点插入到原来的最右边的孩子结点的后面。同时其他孩子结点需要向后移动一个位置。以空出相应的`cell`来给新结点。

最后我们来修改一下测试用例中的部分情况。
```Ruby
  it "prints error message when table is full" do
    script = (1..1400).map do |i|
      "insert #{i} user#{i} person#{i}@example.com"
    end
    script << ".exit"
    result = run_script(script)
    expect(result.last(2)).to match_array([
      "db > Executed.",
      "db > Need to implement splitting internal node",
    ])
  end

  it "allows printing out the structure of a 4-leaf-node btree" do
    script = [
      "insert 18 user18 person18@example.com",
      "insert 7 user7 person7@example.com",
      "insert 10 user10 person10@example.com",
      "insert 29 user29 person29@example.com",
      "insert 23 user23 person23@example.com",
      "insert 4 user4 person4@example.com",
      "insert 14 user14 person14@example.com",
      "insert 30 user30 person30@example.com",
      "insert 15 user15 person15@example.com",
      "insert 26 user26 person26@example.com",
      "insert 22 user22 person22@example.com",
      "insert 19 user19 person19@example.com",
      "insert 2 user2 person2@example.com",
      "insert 1 user1 person1@example.com",
      "insert 21 user21 person21@example.com",
      "insert 11 user11 person11@example.com",
      "insert 6 user6 person6@example.com",
      "insert 20 user20 person20@example.com",
      "insert 5 user5 person5@example.com",
      "insert 8 user8 person8@example.com",
      "insert 9 user9 person9@example.com",
      "insert 3 user3 person3@example.com",
      "insert 12 user12 person12@example.com",
      "insert 27 user27 person27@example.com",
      "insert 17 user17 person17@example.com",
      "insert 16 user16 person16@example.com",
      "insert 13 user13 person13@example.com",
      "insert 24 user24 person24@example.com",
      "insert 25 user25 person25@example.com",
      "insert 28 user28 person28@example.com",
      ".btree",
      ".exit",
    ]
    result = run_script(script)

    expect(result[30...(result.length)]).to match_array([
      "db > Tree:",
      "- internal (size 3)",
      "  - leaf (size 7)",
      "    - 1",
      "    - 2",
      "    - 3",
      "    - 4",
      "    - 5",
      "    - 6",
      "    - 7",
      "  - key 7",
      "  - leaf (size 8)",
      "    - 8",
      "    - 9",
      "    - 10",
      "    - 11",
      "    - 12",
      "    - 13",
      "    - 14",
      "    - 15",
      "  - key 15",
      "  - leaf (size 7)",
      "    - 16",
      "    - 17",
      "    - 18",
      "    - 19",
      "    - 20",
      "    - 21",
      "    - 22",
      "  - key 22",
      "  - leaf (size 8)",
      "    - 23",
      "    - 24",
      "    - 25",
      "    - 26",
      "    - 27",
      "    - 28",
      "    - 29",
      "    - 30",
      "db > Bye!",
    ])
  end
```
特别注意一下，最后一个测试用例，我们仅修改了`expect(result[30...(result.length)])`为**30**，因为我们的`result`数组中，第一个元素是`db > Tree:`，所以我们需要从第**31**个元素开始截取。运行结果中，我们可以看到，我们的`db > Tree:`和`db > Bye!`之间的内容，是我们的树的结构。
```Ruby
.............

Finished in 0.05272 seconds (files took 0.0802 seconds to load)
13 examples, 0 failures
```
又双叒叕通过了！

## 4. 总结
在本章中，我们实现了对分裂叶子结点后，对父结点的更新，核心思路是实现对内部结点的调整。虽然这是`cstack/db_tutorial`的最后一章了，但我们的更新不会结束。在下一章我们将实现内部结点的分裂，来完成我们的数据库。
