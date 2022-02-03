int main(int argc, char const *argv[])
{
    while (true)
    {
        print_prompt();
        std::string input_line;
        std::getline(std::cin, input_line);

        if (input_line == ".exit")
        {
            exit(EXIT_SUCCESS);
        }
        else
        {
            std::cout << "Unrecognized command " << input_line << "." << std::endl;
        }
    }
    return 0;
}