#include <iostream>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <unistd.h>

enum MetaCommandResult
{
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
};
enum PrepareResult
{
    PREPARE_SUCCESS,
    PREPARE_NEGATIVE_ID,
    PREPARE_STRING_TOO_LONG,
    PREPARE_SYNTAX_ERROR,
    PREPARE_UNRECOGNIZED_STATEMENT
};
enum StatementType
{
    STATEMENT_INSERT,
    STATEMENT_SELECT
};
enum ExecuteResult
{
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL
};
enum NodeType
{
    NODE_INTERNAL,
    NODE_LEAF
};
#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255
class Row
{
public:
    uint32_t id;
    char username[COLUMN_USERNAME_SIZE + 1];
    char email[COLUMN_EMAIL_SIZE + 1];
    Row()
    {
        id = 0;
        username[0] = '\0';
        email[0] = '\0';
    }
    Row(uint32_t id, const char *username, const char *email)
    {
        this->id = id;
        strncpy(this->username, username, COLUMN_USERNAME_SIZE + 1);
        strncpy(this->email, email, COLUMN_EMAIL_SIZE + 1);
    }
};

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
    strncpy((char *)destination + USERNAME_OFFSET, source.username, USERNAME_SIZE);
    strncpy((char *)destination + EMAIL_OFFSET, source.email, EMAIL_SIZE);
}

void deserialize_row(void *source, Row &destination)
{
    memcpy(&(destination.id), (char *)source + ID_OFFSET, ID_SIZE);
    strncpy(destination.username, (char *)source + USERNAME_OFFSET, USERNAME_SIZE);
    strncpy(destination.email, (char *)source + EMAIL_OFFSET, EMAIL_SIZE);
}

#define TABLE_MAX_PAGES 100
const uint32_t PAGE_SIZE = 4096;

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

/*
 * Leaf Node Header Layout
 */
const uint32_t LEAF_NODE_NUM_CELLS_SIZE = sizeof(uint32_t);
const uint32_t LEAF_NODE_NUM_CELLS_OFFSET = COMMON_NODE_HEADER_SIZE;
const uint32_t LEAF_NODE_HEADER_SIZE =
    COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE;

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
    *leaf_node.leaf_node_num_cells() += 1;
    *leaf_node.leaf_node_key(cell_num) = key;
    serialize_row(value, leaf_node.leaf_node_value(cell_num));
}
class Statement
{
public:
    StatementType type;
    Row row_to_insert;
};
class DB
{
private:
    Table *table;

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

void DB::print_prompt()
{
    std::cout << "db > ";
}

bool DB::parse_meta_command(std::string &command)
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

PrepareResult DB::prepare_insert(std::string &input_line, Statement &statement)
{
    statement.type = STATEMENT_INSERT;

    char *insert_line = (char *)input_line.c_str();
    char *keyword = strtok(insert_line, " ");
    char *id_string = strtok(NULL, " ");
    char *username = strtok(NULL, " ");
    char *email = strtok(NULL, " ");

    if (id_string == NULL || username == NULL || email == NULL)
    {
        return PREPARE_SYNTAX_ERROR;
    }
    int id = atoi(id_string);
    if (id < 0)
    {
        return PREPARE_NEGATIVE_ID;
    }
    if (strlen(username) > COLUMN_USERNAME_SIZE)
    {
        return PREPARE_STRING_TOO_LONG;
    }
    if (strlen(email) > COLUMN_EMAIL_SIZE)
    {
        return PREPARE_STRING_TOO_LONG;
    }
    statement.row_to_insert = Row(id, username, email);

    return PREPARE_SUCCESS;
}
PrepareResult DB::prepare_statement(std::string &input_line, Statement &statement)
{
    if (!input_line.compare(0, 6, "insert"))
    {
        return prepare_insert(input_line, statement);
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
bool DB::parse_statement(std::string &input_line, Statement &statement)
{
    switch (prepare_statement(input_line, statement))
    {
    case PREPARE_SUCCESS:
        return false;
    case (PREPARE_NEGATIVE_ID):
        std::cout << "ID must be positive." << std::endl;
        return true;
    case (PREPARE_STRING_TOO_LONG):
        std::cout << "String is too long." << std::endl;
        return true;
    case PREPARE_SYNTAX_ERROR:
        std::cout << "Syntax error. Could not parse statement." << std::endl;
        return true;
    case PREPARE_UNRECOGNIZED_STATEMENT:
        std::cout << "Unrecognized keyword at start of '" << input_line << "'." << std::endl;
        return true;
    }
    return false;
}
ExecuteResult DB::execute_insert(Statement &statement)
{
    LeafNode leaf_node = table->pager.get_page(table->root_page_num);
    if (*leaf_node.leaf_node_num_cells() >= LEAF_NODE_MAX_CELLS)
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
    case EXECUTE_TABLE_FULL:
        std::cout << "Error: Table full." << std::endl;
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

        Statement statement;

        if (parse_statement(input_line, statement))
        {
            continue;
        }

        execute_statement(statement);
    }
}

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
