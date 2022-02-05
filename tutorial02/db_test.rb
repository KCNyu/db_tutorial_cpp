describe 'database' do
  def run_script(commands)
    raw_output = nil
    IO.popen("./db", "r+") do |pipe|
      commands.each do |command|
        pipe.puts command
      end

      pipe.close_write

      # Read entire output
      raw_output = pipe.gets(nil)
    end
    raw_output.split("\n")
  end

  it 'test exit and unrecognized command and sql sentence' do
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

  it 'test insert and select' do
    result = run_script([
      "insert 1 user1",
      "select",
      ".exit",
    ])
    expect(result).to match_array([
      "db > Executing insert statement",
      "db > Executing select statement",
      "db > Bye!",
    ])
  end
end
