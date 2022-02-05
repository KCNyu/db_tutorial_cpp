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

  it "test exit and unrecognized command and sql sentence" do
    result = run_script([
      "hello world",
      ".HELLO WORLD",
      ".exit",
    ])
    expect(result).to match_array([
      "db > Unrecognized keyword at start of 'hello world'.",
      "db > Unrecognized command: .HELLO WORLD",
      "db > Bye!",
    ])
  end

  it "inserts and retrieves a row" do
    result = run_script([
      "insert 1 user1 person1@example.com",
      "insert 2 user2",
      "select",
      ".exit",
    ])
    expect(result).to match_array([
      "db > Executed.",
      "db > Syntax error. Could not parse statement.",
      "db > (1, user1, person1@example.com)",
      "Executed.",
      "db > Bye!",
    ])
  end

  it "prints error message when table is full" do
    script = (1..1401).map do |i|
      "insert #{i} user#{i} person#{i}@example.com"
    end
    script << ".exit"
    result = run_script(script)
    expect(result[-2]).to eq("Error: Table full.")
  end

  it "allows inserting strings that are the maximum length" do
    long_username = "a" * 32
    long_email = "a" * 255
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

  it "prints error message if strings are too long" do
    long_username = "a" * 33
    long_email = "a" * 256
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

  it "prints an error message if id is negative" do
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
