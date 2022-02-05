# 如何用C++实现一个简易数据库（四）

## 1. 如何测试边界情况？
在上一章我们已经初步完成了数据库的设计，现在让我们来测试边界情况，也就是`Table`内存占满的情况。
```ruby
    it 'prints error message when table is full' do
        script = (1..1401).map do |i|
          "insert #{i} user#{i} person#{i}@example.com"
        end
        script << ".exit"
        result = run_script(script)
        expect(result[-2]).to eq('Error: Table full.')
    end
```
同样是通过了我们的边界测试。
```
...
Finished in 0.01882 seconds (files took 0.08296 seconds to load)
3 examples, 0 failures
```
那这样子就证明我们写的程序已经十分健壮了吗？让我们再来测试一下属性的边界情况。
```ruby
  it 'allows inserting strings that are the maximum length' do
    long_username = "a"*32
    long_email = "a"*255
    script = [
      "insert 1 #{long_username} #{long_email}",
      "select",
      ".exit",
    ]
    result = run_script(script)
    expect(result).to match_array([
      "db > Executed.",
      "db > (1, #{long_username}, #{long_email})",
      "Executed.",
      "db > Bye!",
    ])
  end
```
再次输入`rspec spec db_test.rb`
```
...F

Finished in 0.02798 seconds (files took 0.07835 seconds to load)
4 examples, 1 failure

Failed examples:

rspec ./db_test.rb:55 # database allows inserting strings that are the maximum length
```
其实问题很明显，就是内存边界确定错误了，边界加一即可了。
```c++
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
```
再来看看现在能否通过测试呢？
```
....

Finished in 0.01829 seconds (files took 0.08065 seconds to load)
4 examples, 0 failures
```
非常不错，让我们再来看看这两个测试。
```ruby
it 'prints error message if strings are too long' do
    long_username = "a"*33
    long_email = "a"*256
    script = [
      "insert 1 #{long_username} #{long_email}",
      "select",
      ".exit",
    ]
    result = run_script(script)
    expect(result).to match_array([
      "db > String is too long.",
      "db > Executed.",
      "db > Bye!",
    ])
  end
it 'prints an error message if id is negative' do
  script = [
    "insert -1 cstack foo@bar.com",
    "select",
    ".exit",
  ]
  result = run_script(script)
  expect(result).to match_array([
    "db > ID must be positive.",
    "db > Executed.",
    "db > Bye!",
  ])
end
```
当我们多加了两个之后，让我们再测试看看。
```
....FF

Finished in 0.03622 seconds (files took 0.07645 seconds to load)
6 examples, 2 failure

Failed examples:

rspec ./db_test.rb:72 # database prints error message if strings are too long
rspec ./db_test.rb:88 # database prints an error message if id is negative
```
结果明显再次错误了，显然我们不能在用户层限制输入，而应该从程序方面就做出限制。

## 2.如何根据测试结果来修复程序？
让我们再将我们的`PrepareResult`再添加两条属性`PREPARE_NEGATIVE_ID`与`
PREPARE_STRING_TOO_LONG`

我们将`insert`函数单独独立出来，使用`strtok`进行字符串分割（btw，`std::string`居然没有对应的`split`方法）
```c++
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
```
最后根据返回的状态码补充报错信息。
```c++
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
```
让我们再来测试一下。
```
......

Finished in 0.02078 seconds (files took 0.0785 seconds to load)
6 examples, 0 failures
```

## 4.总结
作为一个完整的程序，自然是需要考虑程序各种极端边界情况。而这就需要我们尽可能的编写覆盖率高的单元测试来进行检验。在下一章，我们将要实现，把内存中的数据储存到硬盘中去，像一个真正的数据库一样，而不是退出程序即丢失数据。