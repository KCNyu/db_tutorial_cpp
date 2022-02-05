# 如何用C++实现一个简易数据库（五）

## 1. 我们怎样规范我们的储存行为？
首先，为了确定我们程序的运行行为，我们需要确定一个规范。即只有通过`.exit`合法退出时，我们才能将数据正常储存到硬盘(文件)中。我们将执行从内存页面刷新(flush)到硬盘(文件)的操作。为此，我们新增如下的测试。
```ruby
describe "database" do
  before do
    `rm -rf test.db`
  end

  def run_script(commands)
    raw_output = nil
    IO.popen("./db test.db", "r+") do |pipe|
      commands.each do |command|
        pipe.puts command
      end

      pipe.close_write

      # Read entire output
      raw_output = pipe.gets(nil)
    end
    raw_output.split("\n")
  end
it "keeps data after closing connection" do
    result1 = run_script([
      "insert 1 user1 person1@example.com",
      ".exit",
    ])
    expect(result1).to match_array([
      "db > Executed.",
      "db > Bye!",
    ])
    result2 = run_script([
      "select",
      ".exit",
    ])
    expect(result2).to match_array([
      "db > (1, user1, person1@example.com)",
      "Executed.",
      "db > Bye!",
    ])
  end
end
```
同时注意，我们对`run_script`中`db`的打开方式也做了新的修改，增加了一个额外的命令行参数。

## 2. 如何将数据储存到硬盘中？

首先，我们对`main`函数进行全新的处理，以便获得命令行参数。
```c++
int main(int argc, char const *argv[])
{
    if (argc < 2)
    {
        std::cout << "Must supply a database filename." << std::endl;
        exit(EXIT_FAILURE);
    }

    DB db(argv[1]);
    db.start();
    return 0;
}
```
我们看到，我们创建了一个全新的构造函数形式，以便我们可以接受命令行参数。我们接着往下看，我们将`Table`这个对象作为属性添加到了`DB`类中。这样子，我们的`execute`类函数不再需要额外的`table`参数传递，而是直接调用自身属性内的`table`对象。
```c++
class DB
{
private:
    Table* table;

public:
    DB(const char *filename)
    {
        table = new Table(filename);
    }
    void start();
    void print_prompt();

    bool parse_meta_command(std::string &command);
    MetaCommandResult do_meta_command(std::string &command);

    PrepareResult prepare_insert(std::string &input_line, Statement &statement);
    PrepareResult prepare_statement(std::string &input_line, Statement &statement);
    bool parse_statement(std::string &input_line, Statement &statement);
    void execute_statement(Statement &statement);
    ExecuteResult execute_insert(Statement &statement);
    ExecuteResult execute_select(Statement &statement);

    ~DB()
    {
        delete table;
    }
};
```
现在让我们来看一下我们在`new Table(filename)`中到底做了什么。
```c++
class Table
{
public:
    uint32_t num_rows;
    Pager pager;
    Table(const char *filename) : pager(filename)
    {
        num_rows = pager.file_length / ROW_SIZE;
    }
    ~Table();
    void *row_slot(uint32_t row_num);
};
```
我们创建了一个全新的`Pager分页`对象。那么这个`Pager`究竟是做了些什么呢？

## 3. 如何实现一个分页?
现在我们见到了在`Table`中消失的`void *pages[TABLE_MAX_PAGES];`
```c++
class Pager
{
public:
    int file_descriptor;
    uint32_t file_length;
    void *pages[TABLE_MAX_PAGES];
    Pager(const char *filename);
    void *get_page(uint32_t page_num);
    void pager_flush(uint32_t page_num, uint32_t size);
};
```
我们首先来看一下如何构造这个`Pager`对象。
```c++
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

    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        pages[i] = nullptr;
    }
}
```
我们看到，我们创建了一个新的`file_descriptor`用作我们物理磁盘上储存交互，并且设置了`file_length`属性来获取其文件大小。

此外我们在这当中添加了一个`get_page`函数来作用于`row_slot`当中，用于获取指定页的内存。逻辑依旧十分简单，如果我们没有获取到页面，我们就创建一个新的页面，并且将其存储到`pages`数组中。
```c++
void *Table::row_slot(uint32_t row_num)
{
    uint32_t page_num = row_num / ROWS_PER_PAGE;
    void *page = pager.get_page(page_num);
    uint32_t row_offset = row_num % ROWS_PER_PAGE;
    uint32_t byte_offset = row_offset * ROW_SIZE;
    return (char *)page + byte_offset;
}

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
    }

    return pages[page_num];
}
```
我们注意到，如果我们所获取的页码是已经在我们磁盘(文件)中的，我们就直接从其中读取。

## 4. 如何将内存中的数据写入磁盘?

关键行为规范的是，在我们的用户合理使用`.exit`退出时，我们便将所有的数据写入磁盘。
```c++
MetaCommandResult DB::do_meta_command(std::string &command)
{
    if (command == ".exit")
    {
        delete(table);
        std::cout << "Bye!" << std::endl;
        exit(EXIT_SUCCESS);
    }
    else
    {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}
```
核心非常简单，就是`delete(table)`，这个函数将所有的数据写入磁盘。那让我们来看看它到底是怎么做的。
```c++
Table::~Table()
{
    uint32_t num_full_pages = num_rows / ROWS_PER_PAGE;

    for (uint32_t i = 0; i < num_full_pages; i++)
    {
        if (pager.pages[i] == nullptr)
        {
            continue;
        }
        pager.pager_flush(i, PAGE_SIZE);
        free(pager.pages[i]);
        pager.pages[i] = nullptr;
    }

    // There may be a partial page to write to the end of the file
    // This should not be needed after we switch to a B-tree
    uint32_t num_additional_rows = num_rows % ROWS_PER_PAGE;
    if (num_additional_rows > 0)
    {
        uint32_t page_num = num_full_pages;
        if (pager.pages[page_num] != nullptr)
        {
            pager.pager_flush(page_num, num_additional_rows * ROW_SIZE);
            free(pager.pages[page_num]);
            pager.pages[page_num] = nullptr;
        }
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
在这个函数中，我们首先关闭文件，然后释放所有的页面。关键在于释放页面的前提是，我们调用了`page_flush`这个函数，先让我们来看看这个函数。
```c++
void Pager::pager_flush(uint32_t page_num, uint32_t size)
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
        write(file_descriptor, pages[page_num], size);

    if (bytes_written == -1)
    {
        std::cout << "Error writing: " << errno << std::endl;
        exit(EXIT_FAILURE);
    }
}
```
本质上来说非常简单，打开对应位置，借助`write`写入即可。

回到`~Table`的实现，我们可以看到，存在某种特殊情况，即单个页面中并没有全部使用，此时我们仅需要通过调整`flush`的`size`参数将剩余的部分写入磁盘即可。最后关闭文件并二次保险确认释放页面。

让我们来测试一下储存功能。
```
.......

Finished in 0.03502 seconds (files took 0.07738 seconds to load)
7 examples, 0 failures
```
非常棒，我们可以看到，我们的数据已经被成功存储到磁盘中。

## 5. 总结
现在我们已经完成了数据库的基本操作，并且也可以储存到磁盘中去了。但我们可以看到，我们每次都在重复将已有的数据读取和写入磁盘，这是一个很慢的过程。在下一章，我们将通过实现`cursor`来解决这个问题。