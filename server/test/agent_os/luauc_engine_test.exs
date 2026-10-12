defmodule AgentOS.LuaucEngineTest do
  use ExUnit.Case, async: false

  alias AgentOS.Luauc.Request
  alias AgentOS.LuaucEngine

  @moduletag :luauc_engine
  @moduletag timeout: 180_000

  test "failed startup removes staged artifacts" do
    pattern = Path.join(System.tmp_dir!(), "agentos-luauc-*")
    before = MapSet.new(Path.wildcard(pattern))
    executable = Path.join(System.tmp_dir!(), "missing-luauc-#{System.unique_integer([:positive])}")

    assert {:error, {:luauc_engine_missing, ^executable}} =
             LuaucEngine.start(executable: executable, pack: <<>>, profile: <<>>)

    assert MapSet.new(Path.wildcard(pattern)) == before
  end

  test "adapter rejects malformed guest requests before compile" do
    {profile, pack} = digests()

    assert {:reject, 10, "InvalidHeader"} =
             prepare(%{entry: "main", modules: [main("return 1\n")], budget_mask: 8}, profile, pack)

    assert {:reject, 13, "InvalidEntry"} =
             prepare(%{entry: "nope", modules: [main("return 1\n")]}, profile, pack)

    assert {:reject, 2, "NonCanonicalLayout"} =
             prepare(%{entry: "main", modules: [main("")]}, profile, pack)

    assert {:reject, 16, "RequestTooLarge"} =
             prepare(%{entry: "main", modules: [main(:binary.copy("a", 16_777_216 - 100))]}, profile, pack)

    assert {:reject, 16, "RequestTooLarge"} =
             prepare(%{entry: "main", modules: [main("return 1\n")], total_size: 16_777_216 + 1}, profile, pack)

    assert {:reject, 19, "Truncated"} = Request.prepare(short_header(), profile, pack, 0xFF)

    assert {:reject, 17, "InvalidTier"} =
             prepare(
               %{
                 entry: "main",
                 tier: 1,
                 budget_mask: 7,
                 mem: 1,
                 fuel: 1,
                 table: 1,
                 modules: [main("return 1\n")]
               },
               profile,
               pack,
               97
             )
  end

  test "native port compiles return 1" do
    assert {:ok, pid} = LuaucEngine.start([])

    try do
      %{profile: profile, pack: pack} = LuaucEngine.digests(pid)

      assert {:compile, package, %{tier: "full", mem: 16_777_216, fuel: 50_000_000_000, table: 10_000}} =
               prepare(%{entry: "main", modules: [main("return 1\n")]}, profile, pack)

      assert {:ok, 0, _diagnostic, guest} = LuaucEngine.compile(pid, package)
      assert <<0, 0x61, 0x73, 0x6D, _::binary>> = guest
    after
      LuaucEngine.stop(pid)
    end
  end

  defp prepare(opts, profile, pack, mask \\ 0xFF) do
    Request.prepare(Request.encode_guest(opts), profile, pack, mask)
  end

  defp main(source), do: %{name: "main", source: source}

  defp digests, do: {<<0::256>>, <<0::256>>}

  defp short_header do
    <<
      "AOLC1",
      0,
      0,
      0,
      1::little-unsigned-16,
      56::little-unsigned-16,
      (16_777_216 + 1)::little-unsigned-32,
      2::little-unsigned-32,
      1::little-unsigned-32,
      0::little-unsigned-16,
      0::little-unsigned-16,
      0::little-unsigned-32
    >>
  end
end
