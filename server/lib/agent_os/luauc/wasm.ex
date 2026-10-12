defmodule AgentOS.Luauc.Wasm do
  @moduledoc """
  Wasm custom-section helpers shared by profile extraction and install stamps.

  The stamp matches `bazel/tools/mc-stamp`: drop `mc_tier`, `mc_budget`,
  `mc_service`, and `mc_applets`, then append `mc_tier` and, when the budget is
  nonzero, `mc_budget`.
  """

  import Bitwise

  @stamp_names ["mc_tier", "mc_budget", "mc_service", "mc_applets"]

  @spec profile_from_pack(binary()) :: {:ok, binary()} | :error
  def profile_from_pack(pack) when is_binary(pack) do
    case payloads(pack, "luauc.runtime.v1") do
      [payload] -> {:ok, payload}
      _ -> :error
    end
  end

  @spec stamp(binary(), binary(), non_neg_integer(), non_neg_integer(), non_neg_integer()) :: binary()
  def stamp(wasm, tier_name, mem, fuel, table)
      when is_binary(wasm) and is_binary(tier_name) and is_integer(mem) and mem >= 0 and
             is_integer(fuel) and fuel >= 0 and is_integer(table) and table >= 0 do
    stripped = strip(wasm, @stamp_names)
    parts = [stripped, custom_section("mc_tier", tier_name)]

    parts =
      if mem == 0 and fuel == 0 and table == 0 do
        parts
      else
        budget = <<1::little-unsigned-32, mem::little-unsigned-64, fuel::little-unsigned-64, table::little-unsigned-32>>
        parts ++ [custom_section("mc_budget", budget)]
      end

    IO.iodata_to_binary(parts)
  end

  defp strip(wasm, names) when byte_size(wasm) < 8, do: wasm

  defp strip(wasm, names) do
    <<prefix::binary-size(8), _::binary>> = wasm
    walk(wasm, 8, [prefix], names)
  end

  defp walk(wasm, cursor, acc, _names) when cursor >= byte_size(wasm), do: IO.iodata_to_binary(acc)

  defp walk(wasm, cursor, acc, names) do
    <<_::binary-size(cursor), id, _::binary>> = wasm

    case read_uleb(wasm, cursor + 1) do
      :error ->
        IO.iodata_to_binary([acc, binary_part(wasm, cursor, byte_size(wasm) - cursor)])

      {:ok, size, body} ->
        ending = body + size

        if ending > byte_size(wasm) do
          IO.iodata_to_binary([acc, binary_part(wasm, cursor, byte_size(wasm) - cursor)])
        else
          acc =
            if id == 0 and drop?(wasm, body, ending, names) do
              acc
            else
              [acc, binary_part(wasm, cursor, ending - cursor)]
            end

          walk(wasm, ending, acc, names)
        end
    end
  end

  defp drop?(wasm, body, ending, names) do
    case read_uleb(wasm, body) do
      {:ok, name_len, name_at} ->
        name_end = name_at + name_len
        name_end <= ending and binary_part(wasm, name_at, name_len) in names

      :error ->
        false
    end
  end

  defp payloads(wasm, wanted) when byte_size(wasm) < 8, do: []

  defp payloads(wasm, wanted) do
    <<0, 0x61, 0x73, 0x6D, _::binary>> = wasm
    collect(wasm, 8, wanted, [])
  rescue
    MatchError -> []
  end

  defp collect(wasm, cursor, _wanted, acc) when cursor >= byte_size(wasm), do: Enum.reverse(acc)

  defp collect(wasm, cursor, wanted, acc) do
    <<_::binary-size(cursor), id, _::binary>> = wasm

    case read_uleb(wasm, cursor + 1) do
      :error ->
        Enum.reverse(acc)

      {:ok, size, body} ->
        ending = body + size
        if ending > byte_size(wasm), do: Enum.reverse(acc), else: collect_section(wasm, id, body, ending, wanted, acc)
    end
  end

  defp collect_section(wasm, 0, body, ending, wanted, acc) do
    acc =
      case read_uleb(wasm, body) do
        {:ok, name_len, name_at} ->
          name_end = name_at + name_len

          if name_end <= ending and binary_part(wasm, name_at, name_len) == wanted do
            [binary_part(wasm, name_end, ending - name_end) | acc]
          else
            acc
          end

        :error ->
          acc
      end

    collect(wasm, ending, wanted, acc)
  end

  defp collect_section(wasm, _id, _body, ending, wanted, acc), do: collect(wasm, ending, wanted, acc)

  defp custom_section(name, payload) do
    body = IO.iodata_to_binary([uleb(byte_size(name)), name, payload])
    IO.iodata_to_binary([<<0>>, uleb(byte_size(body)), body])
  end

  defp uleb(value) when value < 128, do: <<value>>

  defp uleb(value) do
    <<bor(band(value, 0x7F), 0x80)>> <> uleb(div(value, 128))
  end

  defp read_uleb(bytes, at, acc \\ 0, shift \\ 0)
  defp read_uleb(_bytes, _at, _acc, shift) when shift > 28, do: :error

  defp read_uleb(bytes, at, acc, shift) when at < byte_size(bytes) do
    <<_::binary-size(at), byte, _::binary>> = bytes
    value = acc + bsl(band(byte, 0x7F), shift)
    if band(byte, 0x80) == 0, do: {:ok, value, at + 1}, else: read_uleb(bytes, at + 1, value, shift + 7)
  end

  defp read_uleb(_bytes, _at, _acc, _shift), do: :error
end
