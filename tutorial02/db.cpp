#include <iostream>
#include <string>

class DB
{
private:
    enum MetaCommandResult
    {
        META_COMMAND_SUCCESS,
        META_COMMAND_UNRECOGNIZED_COMMAND
    };
    enum PrePareResult
    {
        PREPARE_SUCCESS,
        PREPARE_UNRECOGNIZED_STATEMENT
    };
    enum StatementType
    {
        STATEMENT_INSERT,
        STATEMENT_SELECT
    };

public:
    DB() {}
    ~DB() {}
    void start();
    void print_prompt();

    bool parse_meta_command(std::string command);
    MetaCommandResult do_meta_command(std::string command);

    PrePareResult prepare_statement(std::string statement, StatementType &type);
    bool parse_statement(std::string statement, StatementType &type);
    void excute_statement(StatementType &type);
};

void DB::print_prompt()
{
    std::cout << "db > ";
}

bool DB::parse_meta_command(std::string command)
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
DB::MetaCommandResult DB::do_meta_command(std::string command)
{
    if (command == ".exit")
    {
        std::cout << "Bye!" << std::endl;
        exit(EXIT_SUCCESS);
    }
    else
    {
        return META_COMMAND_UNRECOGNIZED_COMMAND;
    }
}

DB::PrePareResult DB::prepare_statement(std::string statement, StatementType &type)
{
    if (!statement.compare(0, 6, "insert"))
    {
        type = STATEMENT_INSERT;
        return PREPARE_SUCCESS;
    }
    else if (!statement.compare(0, 6, "select"))
    {
        type = STATEMENT_SELECT;
        return PREPARE_SUCCESS;
    }
    else
    {
        return PREPARE_UNRECOGNIZED_STATEMENT;
    }
}
bool DB::parse_statement(std::string statement, StatementType &type)
{
    switch (prepare_statement(statement, type))
    {
    case PREPARE_SUCCESS:
        return false;
    case PREPARE_UNRECOGNIZED_STATEMENT:
        std::cout << "Unrecognized keyword at start of '" << statement << "'." << std::endl;
        return true;
    }
    return false;
}
void DB::excute_statement(StatementType &type)
{
    switch (type)
    {
    case STATEMENT_INSERT:
        std::cout << "Executing insert statement" << std::endl;
        break;
    case STATEMENT_SELECT:
        std::cout << "Executing select statement" << std::endl;
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

        StatementType m_currentStatementType;

        if (parse_statement(input_line, m_currentStatementType))
        {
            continue;
        }

        excute_statement(m_currentStatementType);
    }
}

int main(int argc, char const *argv[])
{
    DB db;
    db.start();
    return 0;
}
