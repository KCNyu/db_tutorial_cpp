# 如何用C++实现一个简易数据库（十）

## 1. 如何实现递归搜索`B-Tree`？
按照惯例，先看一下假设我们修复错误后应该如何正常打印我们的`B-Tree`。
```ruby
  it "allows printing out the structure of a 3-leaf-node btree" do
    script = (1..14).map do |i|
      "insert #{i} user#{i} person#{i}@example.com"
    end
    script << ".btree"
    script << "insert 15 user15 person15@example.com"
    script << ".btree"
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
      "db > Executed.",
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
      "  - leaf (size 8)",
      "    - 8",
      "    - 9",
      "    - 10",
      "    - 11",
      "    - 12",
      "    - 13",
      "    - 14",
      "    - 15",
      "db > Bye!",
    ])
  end
```
我们此时应该是能够很好的工作了，同时我们针对我们的测试`run_script`方法进行了一些改进。
```ruby
  def run_script(commands)
    raw_output = nil
    IO.popen("./db test.db", "r+") do |pipe|
      commands.each do |command|
        begin
          pipe.puts command
        rescue Errno::EPIPE
          break
        end
      end

      pipe.close_write

      # Read entire output
      raw_output = pipe.gets(nil)
    end
    raw_output.split("\n")
  end
```
此外，现在我们的限制应该是如何更新我们的父亲结点，这个将会在之后一一实现，我们现在仅需要修改对应的报错提示。
```ruby
  it "prints error message when table is full" do
    script = (1..1400).map do |i|
      "insert #{i} user#{i} person#{i}@example.com"
    end
    script << ".exit"
    result = run_script(script)
    expect(result.last(2)).to match_array([
      "db > Executed.",
      "db > Need to implement updating parent after split",
    ])
  end
```

## 2. 如何实现搜索`InternalNode`?
既然是搜索，那么我们首先来看搜索的最顶层接口`Table::table_find`，我们现在需要支持搜索`InternalNode`。
```c++
Cursor *Table::table_find(uint32_t key)
{
    Node root_node = pager.get_page(root_page_num);

    if (root_node.get_node_type() == NODE_LEAF)
    {
        return new Cursor(this, root_page_num, key);
    }
    else
    {
        return internal_node_find(root_page_num, key);
    }
}
```
我们需要在`internal_node_find`中实现搜索`InternalNode`的逻辑。
```c++
Cursor *Table::internal_node_find(uint32_t page_num, uint32_t key)
{
    InternalNode node = pager.get_page(page_num);
    uint32_t num_keys = *node.internal_node_num_keys();

    /* Binary search to find index of child to search */
    uint32_t min_index = 0;
    uint32_t max_index = num_keys; /* there is one more child than key */

    while (max_index != min_index)
    {
        uint32_t index = (min_index + max_index) / 2;
        uint32_t key_to_right = *node.internal_node_key(index);
        if (key_to_right >= key)
        {
            max_index = index;
        }
        else
        {
            min_index = index + 1;
        }
    }
    uint32_t child_num = *node.internal_node_child(min_index);
    Node child = pager.get_page(child_num);
    switch (child.get_node_type())
    {
    case NODE_INTERNAL:
        return internal_node_find(child_num, key);
    case NODE_LEAF: default:
        return new Cursor(this, child_num, key);
    }
}
```
核心仍旧是使用二分搜索+递归。我们知道内部结点中所储存的孩子结点的指针的右侧储存的是该孩子指针所包含的最大的`key`值，所以我们只需要将被搜索的`key`不断与`key_to_right`进行比较，直到找到`key`的位置。

此外，当我们找到了对应的孩子结点时，要注意判断其类型仍旧为`InternalNode`，我们需要递归调用`internal_node_find`。亦或是我们找到了`LeafNode`，我们仅需要返回一个相应指向该结点的`Cursor`对象即可。

运行一下我们的测试
```ruby
...........

Finished in 0.04058 seconds (files took 0.0803 seconds to load)
11 examples, 0 failures
```
我们可以看到我们的搜索逻辑已经很好的工作了。

## 3. 总结
我们已经基本实现了搜索逻辑，但我们的`Cursor`其实在多层路径下还存在着一些问题。在下一章我们会通过更新`Cursor`类中的方法来解决这些问题。