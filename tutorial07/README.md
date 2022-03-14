# 如何用C++实现一个简易数据库（七）

## 1. B-Tree是什么？
B树 ***（B-tree）*** 是一种自平衡的树，能够保持数据有序。这种资料结构能够让查找数据、顺序访问、插入数据及删除的动作，都在对数时间内完成。 B树，概括来说是一个一般化的二元搜寻树（binary search tree）一个结点可以拥有2个以上的子结点。与自平衡二叉查找树不同，B树适用于读写相对大的数据块的存储系统，例如磁盘。 B树减少定位记录时所经历的中间过程，从而加快存取速度。 B树这种数据结构可以用来描述外部存储。这种资料结构常被应用在数据库和文件系统的实现上。
![example B-Tree](https://upload.wikimedia.org/wikipedia/commons/6/65/B-tree.svg)
与二叉树不同，B-Tree中的每个结点可以有超过2个子结点。每个结点最多可以有m子结点，其中m称为树的“阶”。为了保持树的大部分平衡，我们还说结点必须至少有m/2子结点（四舍五入）。

但实际上，我们在这里使用的是B-Tree的一个变种情况，即：B+Tree。
我们在其中储存我们的数据，并且每个结点中存在多个键值对，而且这个键值是按照顺序排列的。

使用这种结构我们的查找时间复杂度是**O(log(n))**，而且插入和删除的时间复杂度也是**O(log(n))**。

## 2. 如何实现B-Tree?

上面文字性的话语，实际上来说仍旧很难理解。毕竟`Talk is cheap. Show me the code.`

按照我们`TDD`的惯例，先来更新一下测试用例：
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
                        "  - 0 : 3",
                        "  - 1 : 1",
                        "  - 2 : 2",
                        "db > Bye!",
                      ])
  end

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
                        "LEAF_NODE_HEADER_SIZE: 10",
                        "LEAF_NODE_CELL_SIZE: 297",
                        "LEAF_NODE_SPACE_FOR_CELLS: 4086",
                        "LEAF_NODE_MAX_CELLS: 13",
                        "db > Bye!",
                      ])
  end
```
注意到，我们新增两个元命令，一个是`.constants`，另一个是`.btree`。第一个用于打印测试我们所设置的常量，第二个用于打印B-Tree的结构。

那就让我们在`DB::do_meta_command`中增加它。
```c++
MetaCommandResult DB::do_meta_command(std::string &command)
{
    if (command == ".exit")
    {
        delete (table);
        std::cout << "Bye!" << std::endl;
        exit(EXIT_SUCCESS);
    }
    else if (command == ".btree")
    {
        std::cout << "Tree:" << std::endl;
        LeafNode root_node = table->pager.get_page(table->root_page_num);
        root_node.print_leaf_node();
        return META_COMMAND_SUCCESS;
    }
    else if (command == ".constants")
    {
        std::cout << "Constants:" << std::endl;
        std::cout << "ROW_SIZE: " << ROW_SIZE << std::endl;
        std::cout << "COMMON_NODE_HEADER_SIZE: " << COMMON_NODE_HEADER_SIZE << std::endl;
        std::cout << "LEAF_NODE_HEADER_SIZE: " << LEAF_NODE_HEADER_SIZE << std::endl;
        std::cout << "LEAF_NODE_CELL_SIZE: " << LEAF_NODE_CELL_SIZE << std::endl;
        std::cout << "LEAF_NODE_SPACE_FOR_CELLS: " << LEAF_NODE_SPACE_FOR_CELLS << std::endl;
        std::cout << "LEAF_NODE_MAX_CELLS: " << LEAF_NODE_MAX_CELLS << std::endl;
        return META_COMMAND_SUCCESS;
    }
    else
    {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}
```
现在大家并不明白什么是`LeafNode`，这个类是什么？同时也并不明白定义的这些常量是什么意思。让我们一步一步往下看：

首先我们定义一下我们将使用的结点类型
```c++
enum NodeType
{
    NODE_INTERNAL,
    NODE_LEAF
};
```
尽管目前我们还使用不到它，但请注意我们`BTree`的结点是不同的，存在内部结点和叶子结点的差异性。

现在来看我们重中之重的常量
```c++
/*
 * Common Node Header Layout
 */
const uint32_t NODE_TYPE_SIZE = sizeof(uint8_t);
const uint32_t NODE_TYPE_OFFSET = 0;
const uint32_t IS_ROOT_SIZE = sizeof(uint8_t);
const uint32_t IS_ROOT_OFFSET = NODE_TYPE_SIZE;
const uint32_t PARENT_POINTER_SIZE = sizeof(uint32_t);
const uint32_t PARENT_POINTER_OFFSET = IS_ROOT_OFFSET + IS_ROOT_SIZE;
const uint8_t COMMON_NODE_HEADER_SIZE =
    NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE;
```
我们将每个结点设置储存其本身结点类型，指向其父节点的指针（通过这个我们可以实现查找其兄弟结点），以及标记是否为根结点。我们将这三个数据定义为元数据作为结点的标头所存储。

下一步让我们来定义叶子结点需要储存的实际数据。
```c++
/*
 * Leaf Node Header Layout
 */
const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE =
    COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE;
```
我们储存其中包含多少个`CELL`，即对应的键值对（`id`与`ROW`中信息形成对应关系）。

```c++
/*
 * Leaf Node Body Layout
 */
const uint32_t LEAF_NODE_KEY_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_KEY_OFFSET = 0;
const uint32_t LEAF_NODE_VALUE_SIZE = ROW_SIZE;
const uint32_t LEAF_NODE_VALUE_OFFSET =
    LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE;
const uint32_t LEAF_NODE_CELL_SIZE = LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE;
const uint32_t LEAF_NODE_SPACE_FOR_CELLS = PAGE_SIZE - LEAF_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_MAX_CELLS =
    LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE;
```
我们定义我们键值对的键和值的大小，以及每个`CELL`的大小。同时我们也定义了叶子结点的最大`CELL`数量，以及它的空间大小，通过一个结点对应一个页面 ***(Page)***。

现在来看我们的`LeafNode`类
```c++
class LeafNode
{
private:
    void *node;

public:
    LeafNode(void *node) : node(node) {}
    void initialize_leaf_node()
    {
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
    void print_leaf_node()
    {
        uint32_t num_cells = *leaf_node_num_cells();
        std::cout << "leaf (size " << num_cells << ")" << std::endl;
        for (uint32_t i = 0; i < num_cells; i++)
        {
            uint32_t key = *leaf_node_key(i);
            std::cout << "  - " << i << " : " << key << std::endl;
        }
    }
};
```
`void* node`便是我们的`LeafNode`类的一个成员变量，用于存储结点的内存地址。同时我们设置一些相关的函数方法，都是用来返回对应的结点数据的，同时由于都是指针地址的关系，可以做出相应的`set`和`get`操作。

## 3. 如何调整我们的Pager?
不难发现，我们现在形成了`结点->页面`的联系，而非是以前所强调的`页面->数据行`的联系。所以我们对`Pager`的字段做出调整，增加一个`num_pages`字段，用于存储页面的数量。通过`num_pages = file_length / PAGE_SIZE;`来计算原有`DB`中储存的页面数量。
```c++
class Pager
{
private:
    int file_descriptor;
    uint32_t file_length;
    void *pages[TABLE_MAX_PAGES];
    uint32_t num_pages;

public:
    Pager(const char *filename);

    void *get_page(uint32_t page_num);
    void pager_flush(uint32_t page_num);

    friend class Table;
};
Pager::Pager(const char *filename)
{
    file_descriptor = open(filename,
                           O_RDWR |     // Read/Write mode
                               O_CREAT, // Create file if it does not exist
                           S_IWUSR |    // User write permission
                               S_IRUSR  // User read permission
    );
    if (file_descriptor < 0)
    {
        std::cerr << "Error: cannot open file " << filename << std::endl;
        exit(EXIT_FAILURE);
    }

    file_length = lseek(file_descriptor, 0, SEEK_END);
    num_pages = file_length / PAGE_SIZE;

    if (file_length % PAGE_SIZE != 0)
    {
        std::cerr << "Db file is not a whole number of pages. Corrupt file." << std::endl;
        exit(EXIT_FAILURE);
    }

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        pages[i] = nullptr;
    }
}
```
在我们获取页面的`get_page`中，请注意我们的`uint32_t num_pages = file_length / PAGE_SIZE;`这个`num_pages`代表的是`DB`中的页面数量，而不是当前`Pager`中的页面数量。也即`this->num_pages`。
```c++
void *Pager::get_page(uint32_t page_num)
{
    if (page_num > TABLE_MAX_PAGES)
    {
        std::cout << "Tried to fetch page number out of bounds. " << page_num << " > "
                  << TABLE_MAX_PAGES << std::endl;
        exit(EXIT_FAILURE);
    }

    if (pages[page_num] == nullptr)
    {
        // Cache miss. Allocate memory and load from file.
        void *page = malloc(PAGE_SIZE);
        uint32_t num_pages = file_length / PAGE_SIZE;

        // We might save a partial page at the end of the file
        if (file_length % PAGE_SIZE)
        {
            num_pages += 1;
        }

        if (page_num <= num_pages)
        {
            lseek(file_descriptor, page_num * PAGE_SIZE, SEEK_SET);
            ssize_t bytes_read = read(file_descriptor, page, PAGE_SIZE);
            if (bytes_read == -1)
            {
                std::cout << "Error reading file: " << errno << std::endl;
                exit(EXIT_FAILURE);
            }
        }

        pages[page_num] = page;

        if (page_num >= num_pages)
        {
            this->num_pages = page_num + 1;
        }
    }

    return pages[page_num];
}
```
我们对`Table`也要做出调整,关心的内容是`root_page_num`，这个字段用于存储根结点的页面号。
```c++
class Table
{
private:
    uint32_t root_page_num;
    Pager pager;

public:
    Table(const char *filename) : pager(filename)
    {
        root_page_num = 0;
        if (pager.num_pages == 0)
        {
            // New file. Initialize page 0 as leaf node.
            LeafNode root_node = pager.get_page(0);
            root_node.initialize_leaf_node();
        }
    }
    ~Table();

    friend class Cursor;
    friend class DB;
};
```
注意到我们现在一个结点即对应一个`Page`，所以我们需要调整我们的`pager_flush`函数，不再需要指定`size`而是直接刷上一整个`Page`。
```c++
void Pager::pager_flush(uint32_t page_num)
{
    if (pages[page_num] == nullptr)
    {
        std::cout << "Tried to flush null page" << std::endl;
        exit(EXIT_FAILURE);
    }

    off_t offset = lseek(file_descriptor, page_num * PAGE_SIZE, SEEK_SET);

    if (offset == -1)
    {
        std::cout << "Error seeking: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }

    ssize_t bytes_written =
        write(file_descriptor, pages[page_num], PAGE_SIZE);

    if (bytes_written == -1)
    {
        std::cout << "Error writing: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }
}
```

所以我们在`.exit`退出后，往硬盘回写数据时，也应该做出相应的调整。
```c++
Table::~Table()
{
    for (uint32_t i = 0; i < pager.num_pages; i++)
    {
        if (pager.pages[i] == nullptr)
        {
            continue;
        }
        pager.pager_flush(i);
        free(pager.pages[i]);
        pager.pages[i] = nullptr;
    }

    int result = close(pager.file_descriptor);
    if (result == -1)
    {
        std::cout << "Error closing db file." << std::endl;
        exit(EXIT_FAILURE);
    }
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        void *page = pager.pages[i];
        if (page)
        {
            free(page);
            pager.pages[i] = nullptr;
        }
    }
}
```

## 4. 如何调整我们的`Cursor`?
接下来，让我们对上一章所讲的`Cursor`进行调整，关心的内容不再是`ROW`而是结点对应的`Page`以及其内部储存键值对 ***（cell）***。
```c++
class Cursor
{
private:
    Table *table;
    uint32_t page_num;
    uint32_t cell_num;
    bool end_of_table;

public:
    Cursor(Table *&table, bool option);
    void *cursor_value();
    void cursor_advance();
    void leaf_node_insert(uint32_t key, Row &value);

    friend class DB;
};
```
我们新增了两个字段，`page_num`和`cell_num`，用于记录当前游标所在的页面和结点中的位置。

现在让我们对原有的`cursor_value`和`cursor_advance`根据新添字段进行调整。
```c++
Cursor::Cursor(Table *&table, bool option)
{
    this->table = table;
    page_num = table->root_page_num;
    LeafNode root_node = table->pager.get_page(table->root_page_num);
    uint32_t num_cells = *root_node.leaf_node_num_cells();
    if (option)
    {
        // start at the beginning of the table
        cell_num = 0;

        end_of_table = (num_cells == 0);
    }
    else
    {
        // end of the table
        cell_num = num_cells;

        end_of_table = true;
    }
}
void *Cursor::cursor_value()
{
    void *page = table->pager.get_page(page_num);

    return LeafNode(page).leaf_node_value(cell_num);
}
void Cursor::cursor_advance()
{
    LeafNode leaf_node = table->pager.get_page(page_num);
    cell_num += 1;
    if (cell_num >= *leaf_node.leaf_node_num_cells())
    {
        end_of_table = true;
    }
}
```
现在让我们来创建一个将我们的键值对 ***(cell)***插入的函数。
```c++
void Cursor::leaf_node_insert(uint32_t key, Row &value)
{
    LeafNode leaf_node = table->pager.get_page(page_num);
    uint32_t num_cells = *leaf_node.leaf_node_num_cells();

    if (num_cells >= LEAF_NODE_MAX_CELLS)
    {
        // Node full
        std::cout << "Need to implement splitting a leaf node." << std::endl;
        exit(EXIT_FAILURE);
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
    *(leaf_node.leaf_node_num_cells()) += 1;
    *(leaf_node.leaf_node_key(cell_num)) = key;
    serialize_row(value, leaf_node.leaf_node_value(cell_num));
}
```
实质上非常简单，就是使用序列化将新的键值对插入到结点中，并且更新结点中的键值对的数量。同时注意，我们需要更新结点的页面，因为我们可能需要将结点分裂（此部分还未实现）。

最后让我们将它作用于实际的`insert`操作中。
```c++
ExecuteResult DB::execute_insert(Statement &statement)
{
    LeafNode leaf_node = table->pager.get_page(table->root_page_num);
    if (*(leaf_node.leaf_node_num_cells()) >= LEAF_NODE_MAX_CELLS)
    {
        std::cout << "Leaf node full." << std::endl;
        return EXECUTE_TABLE_FULL;
    }

    // end of the table
    Cursor *cursor = new Cursor(table, false);

    cursor->leaf_node_insert(statement.row_to_insert.id, statement.row_to_insert);

    delete cursor;

    return EXECUTE_SUCCESS;
}
```
让我们来运行一下测试用例，看看它是否正确。
```
.........

Finished in 0.0491 seconds (files took 0.08083 seconds to load)
9 examples, 0 failures
```
恭喜并没有影响原来的功能。虽然此时我们储存的数据变少了（仅使用了一个结点），同时也没有完成B-Tree的核心按指定顺序排列。我们仍旧是按照插入顺序所储存的。

## 5. 总结
目前我们仅仅实现了使用结点关系来储存数据，但是一切来得却还是非常不容易。此章理解起来可能比较抽象，同时由于设置了许多为后续章节做铺垫的内容，就会导致我们的程序比较难以理解。下一章我们将会实现核心的按指定顺序排列，同时避免重复的`key`值出现。