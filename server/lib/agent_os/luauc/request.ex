defmodule AgentOS.Luauc.Request do
  @moduledoc """
  Guest compile request (`AOLC1`) and the SourcePackageV1 body the compiler
  accepts. The host stamps `mc_tier` and `mc_budget` on the linked wasm.
  Empty plans, coverage 0, and the pinned frontend contract. Statuses 8..19
  are returned before the compiler runs.
  """

  alias AgentOS.Contracts.Constants
  alias AgentOS.Luauc.Metrics

  import Bitwise

  @max_request 16_777_216
  # The office package object is 39 MiB. This ceiling covers that object and the linked module.
  @compile_budget_bytes 64 * 1024 * 1024
  @header_size 56
  @default_mem 16 * 1024 * 1024
  @default_fuel 50_000_000_000
  @default_table 10_000
  @hard_mem 1024 * 1024 * 1024
  @hard_fuel 4_000_000_000_000
  @hard_table 1_000_000
  @magic <<"AOLC1", 0, 0, 0>>
  @frontend_contract <<
    0x73,
    0x8B,
    0x75,
    0x00,
    0x4F,
    0xC1,
    0xF8,
    0x0F,
    0xF8,
    0x91,
    0xFD,
    0x88,
    0x3A,
    0xE7,
    0x0E,
    0xAC,
    0x3D,
    0x65,
    0x7C,
    0x9C,
    0x01,
    0x62,
    0x09,
    0x96,
    0x46,
    0x92,
    0x6E,
    0x33,
    0xEB,
    0xAB,
    0xD4,
    0x4C
  >>

  @spec prepare(binary(), binary(), binary(), non_neg_integer(), map()) ::
          {:reject, non_neg_integer(), binary()}
          | {:compile, binary(), map()}
  def prepare(body, profile_digest, pack_digest, caller_mask, limits \\ %{})
      when is_binary(body) and byte_size(profile_digest) == 32 and byte_size(pack_digest) == 32 and
             is_integer(caller_mask) and caller_mask >= 0 and is_map(limits) do
    started = System.monotonic_time(:millisecond)

    case parse(body, caller_mask) do
      {:reject, status, diagnostic} = rejected ->
        log_reject(started, body, status, diagnostic)
        rejected

      {:ok, request} ->
        case resolve_stamp(request, caller_mask, limits) do
          {:reject, status, diagnostic} = rejected ->
            log_reject(started, body, status, diagnostic)
            rejected

          {:ok, stamp} ->
            package = encode_package(request.modules, request.entry, profile_digest, pack_digest)
            {:compile, package, stamp}
        end
    end
  end

  @spec frame(non_neg_integer(), binary(), binary()) :: binary()
  def frame(status, diagnostic, artifact)
      when is_integer(status) and is_binary(diagnostic) and is_binary(artifact) do
    guest = if status == 0, do: artifact, else: <<>>
    <<status::little-unsigned-32, byte_size(diagnostic)::little-unsigned-32, diagnostic::binary,
      byte_size(guest)::little-unsigned-32, guest::binary>>
  end

  @spec encode_package([map()], binary(), binary(), binary()) :: binary()
  def encode_package(modules, entry_name, profile_digest, pack_digest) do
    sorted = Enum.sort_by(modules, & &1.name)
    entry_id = Enum.find_index(sorted, &(&1.name == entry_name))
    count = length(sorted)

    prepared =
      Enum.map(sorted, fn %{name: name, source: source} ->
        source_name = <<"@", name::binary, ".luau">>

        %{
          name: name,
          source_name: source_name,
          source: source,
          digest: :crypto.hash(:sha256, source)
        }
      end)

    manifest = [
      <<count::little-unsigned-32, entry_id::little-unsigned-32>>
      | Enum.flat_map(prepared, fn module ->
          [
            <<byte_size(module.name)::little-unsigned-32>>,
            module.name,
            <<byte_size(module.source_name)::little-unsigned-32>>,
            module.source_name,
            module.digest,
            <<0::little-unsigned-32>>
          ]
        end)
    ]

    manifest_digest = :crypto.hash(:sha256, manifest)

    option = <<
      0::little-unsigned-32,
      64::little-unsigned-32,
      0::little-unsigned-32,
      1_048_576::little-unsigned-32,
      4096::little-unsigned-32,
      @compile_budget_bytes::little-unsigned-32,
      0::little-unsigned-32,
      0::little-unsigned-32
    >>

    plan_preimage = for _ <- prepared, do: <<0::little-unsigned-32>>

    request_id =
      :crypto.hash(
        :sha256,
        [
          <<"luauc-source-request-v1", 0>>,
          <<0::little-unsigned-32>>,
          @frontend_contract,
          profile_digest,
          pack_digest,
          manifest_digest,
          option,
          plan_preimage
        ]
      )
      |> binary_part(0, 16)

    payload_size =
      Enum.reduce(prepared, 0, fn module, acc ->
        acc + byte_size(module.name) + byte_size(module.source_name) + byte_size(module.source)
      end)

    total = 240 + 64 * count + payload_size
    header = header_bytes(total, count, entry_id, request_id, profile_digest, pack_digest, manifest_digest, option)
    {records, payload} = records_and_payload(prepared, 240 + 64 * count)
    IO.iodata_to_binary([header, records, payload])
  end

  @spec encode_guest(map()) :: binary()
  def encode_guest(opts) when is_map(opts) do
    header_size = Map.get(opts, :header_size, @header_size)
    modules = opts.modules
    mask = Map.get(opts, :budget_mask, 0)
    mem = if band(mask, 1) == 1, do: Map.get(opts, :mem, 0), else: 0
    fuel = if band(mask, 2) == 2, do: Map.get(opts, :fuel, 0), else: 0
    table = if band(mask, 4) == 4, do: Map.get(opts, :table, 0), else: 0

    payload =
      [counted(opts.entry)] ++
        Enum.flat_map(modules, fn module ->
          [counted(module.name), counted(module.source)]
        end)

    payload = IO.iodata_to_binary(payload)
    total = Map.get(opts, :total_size, header_size + byte_size(payload))

    header = <<
      @magic::binary,
      1::little-unsigned-16,
      header_size::little-unsigned-16,
      total::little-unsigned-32,
      length(modules)::little-unsigned-32,
      Map.get(opts, :tier, 0)::little-unsigned-16,
      mask::little-unsigned-16,
      0::little-unsigned-32,
      0::little-unsigned-32,
      mem::little-unsigned-64,
      fuel::little-unsigned-64,
      table::little-unsigned-64
    >>

    pad = max(header_size - byte_size(header), 0)
    binary_part(header <> :binary.copy(<<0>>, pad) <> payload, 0, header_size + byte_size(payload))
  end

  defp parse(body, _caller_mask) when byte_size(body) < 8, do: reject(19)

  defp parse(<<@magic, _::binary>> = body, caller_mask) do
    if byte_size(body) < @header_size do
      reject(19)
    else
      <<_::binary-size(8), version::little-unsigned-16, header_size::little-unsigned-16, total::little-unsigned-32,
        count::little-unsigned-32, tier::little-unsigned-16, budget_mask::little-unsigned-16,
        reserved::little-unsigned-32, pad::little-unsigned-32, mem::little-unsigned-64, fuel::little-unsigned-64,
        table::little-unsigned-64, _::binary>> = body

      cond do
        version != 1 ->
          reject(9)

        header_size != @header_size or reserved != 0 or pad != 0 or budget_mask > 7 ->
          reject(10)

        bit_clear_nonzero?(budget_mask, 1, mem) or bit_set_zero?(budget_mask, 1, mem) ->
          reject(10)

        bit_clear_nonzero?(budget_mask, 2, fuel) or bit_set_zero?(budget_mask, 2, fuel) ->
          reject(10)

        bit_clear_nonzero?(budget_mask, 4, table) or bit_set_zero?(budget_mask, 4, table) ->
          reject(10)

        table > 0xFFFFFFFF ->
          reject(10)

        true ->
          parse_counts(body, header_size, total, count, tier, budget_mask, mem, fuel, table, caller_mask)
      end
    end
  end

  defp parse(_body, _caller_mask), do: reject(8)

  defp parse_counts(body, header_size, total, count, tier, budget_mask, mem, fuel, table, caller_mask) do
    cond do
      count < 1 or count > 128 ->
        reject(12)

      total > @max_request or byte_size(body) > @max_request ->
        reject(16)

      byte_size(body) < total or total < header_size ->
        reject(19)

      byte_size(body) > total ->
        reject(10)

      true ->
        parse_records(
          binary_part(body, 0, total),
          header_size,
          tier,
          budget_mask,
          mem,
          fuel,
          table,
          count,
          caller_mask
        )
    end
  end

  defp parse_records(body, header_size, tier, budget_mask, mem, fuel, table, count, caller_mask) do
    with {:ok, entry, cursor} <- read_text(body, header_size),
         {:ok, modules, cursor} <- read_modules(body, cursor, count, []) do
      cond do
        cursor < byte_size(body) ->
          reject(19)

        cursor > byte_size(body) ->
          reject(10)

        duplicate?(modules) ->
          reject(14)

        not Enum.any?(modules, &(&1.name == entry)) ->
          reject(13)

        tier != 0 and tier not in 1..4 ->
          reject(17)

        tier != 0 and band(Constants.tier_caps(tier), caller_mask) != Constants.tier_caps(tier) ->
          reject(17)

        Enum.any?(modules, &(&1.source == <<>>)) ->
          {:reject, 2, "NonCanonicalLayout"}

        encoded_length(modules) > @max_request ->
          reject(16)

        true ->
          {:ok,
           %{
             tier: tier,
             budget_mask: budget_mask,
             mem: mem,
             fuel: fuel,
             table: table,
             entry: entry,
             modules: modules
           }}
      end
    end
  end

  defp read_text(body, cursor) do
    case read_counted(body, cursor) do
      {:ok, text, next} -> if String.valid?(text), do: {:ok, text, next}, else: reject(15)
      :error -> reject(19)
    end
  end

  defp read_modules(_body, cursor, 0, acc), do: {:ok, Enum.reverse(acc), cursor}

  defp read_modules(body, cursor, left, acc) do
    with {:ok, name, cursor} <- read_counted(body, cursor),
         {:ok, source, cursor} <- read_counted(body, cursor) do
      if String.valid?(name) and valid_module_name?(name) do
        read_modules(body, cursor, left - 1, [%{name: name, source: source} | acc])
      else
        reject(15)
      end
    else
      :error -> reject(19)
    end
  end

  defp read_counted(body, cursor) when cursor + 4 > byte_size(body), do: :error

  defp read_counted(body, cursor) do
    <<_::binary-size(cursor), length::little-unsigned-32, _::binary>> = body
    start = cursor + 4
    ending = start + length
    if ending > byte_size(body), do: :error, else: {:ok, binary_part(body, start, length), ending}
  end

  defp duplicate?(modules) do
    names = Enum.map(modules, & &1.name)
    length(Enum.uniq(names)) != length(names)
  end

  defp encoded_length(modules) do
    Enum.reduce(modules, 240 + 64 * length(modules), fn module, acc ->
      name = byte_size(module.name)
      acc + name + name + 6 + byte_size(module.source)
    end)
  end

  defp valid_module_name?(name) when is_binary(name) and name != "" do
    not String.starts_with?(name, "/") and not String.ends_with?(name, "/") and not String.contains?(name, "..") and
      String.match?(name, ~r{^[a-z0-9_./-]+$})
  end

  defp valid_module_name?(_), do: false

  defp counted(value) when is_binary(value), do: <<byte_size(value)::little-unsigned-32, value::binary>>

  defp log_reject(started, body, status, diagnostic) do
    Metrics.log_compile(
      :port,
      "-",
      status,
      max(System.monotonic_time(:millisecond) - started, 0),
      byte_size(body),
      0,
      byte_size(diagnostic),
      0
    )
  end

  defp resolve_stamp(request, caller_mask, limits) do
    tier = if request.tier == 0, do: Map.get(limits, :default_tier, 1), else: request.tier
    caps = Constants.tier_caps(tier)

    cond do
      tier not in 1..4 ->
        reject(17)

      band(caps, caller_mask) != caps ->
        reject(17)

      true ->
        mem_ceiling = Map.get(limits, :mem_ceiling, @hard_mem)
        fuel_ceiling = Map.get(limits, :fuel_ceiling, @hard_fuel)
        table_ceiling = Map.get(limits, :table_ceiling, @hard_table)
        mem = if band(request.budget_mask, 1) == 1, do: request.mem, else: @default_mem
        fuel = if band(request.budget_mask, 2) == 2, do: request.fuel, else: @default_fuel
        table = if band(request.budget_mask, 4) == 4, do: request.table, else: @default_table

        {:ok,
         %{
           tier: tier_name(tier),
           mem: clamp(mem, mem_ceiling, @hard_mem),
           fuel: clamp(fuel, fuel_ceiling, @hard_fuel),
           table: clamp(table, table_ceiling, @hard_table)
         }}
    end
  end

  defp bit_clear_nonzero?(mask, bit, value), do: band(mask, bit) == 0 and value != 0
  defp bit_set_zero?(mask, bit, value), do: band(mask, bit) == bit and value == 0

  defp clamp(value, ceiling, hard), do: min(min(value, ceiling), hard)

  defp tier_name(1), do: "full"
  defp tier_name(2), do: "read-write"
  defp tier_name(3), do: "read-only"
  defp tier_name(4), do: "isolated"

  defp header_bytes(total, count, entry_id, request_id, profile_digest, pack_digest, manifest_digest, option) do
    <<
      "LUAUCS1",
      0,
      1::little-unsigned-16,
      240::little-unsigned-16,
      total::little-unsigned-32,
      count::little-unsigned-32,
      entry_id::little-unsigned-32,
      64::little-unsigned-32,
      0::little-unsigned-32,
      request_id::binary,
      @frontend_contract::binary,
      profile_digest::binary,
      pack_digest::binary,
      manifest_digest::binary,
      option::binary,
      0::256
    >>
  end

  defp records_and_payload(modules, base) do
    Enum.map_reduce(modules, {base, []}, fn module, {cursor, payload} ->
      name_at = cursor
      source_name_at = name_at + byte_size(module.name)
      content_at = source_name_at + byte_size(module.source_name)
      next = content_at + byte_size(module.source)

      record = <<
        name_at::little-unsigned-32,
        byte_size(module.name)::little-unsigned-32,
        source_name_at::little-unsigned-32,
        byte_size(module.source_name)::little-unsigned-32,
        content_at::little-unsigned-32,
        byte_size(module.source)::little-unsigned-32,
        module.digest::binary,
        0::little-unsigned-32,
        0::little-unsigned-32
      >>

      {record, {next, [payload, module.name, module.source_name, module.source]}}
    end)
    |> then(fn {records, {_cursor, payload}} -> {records, payload} end)
  end

  defp reject(8), do: {:reject, 8, "InvalidMagic"}
  defp reject(9), do: {:reject, 9, "InvalidVersion"}
  defp reject(10), do: {:reject, 10, "InvalidHeader"}
  defp reject(11), do: {:reject, 11, "InvalidFlags"}
  defp reject(12), do: {:reject, 12, "InvalidModuleCount"}
  defp reject(13), do: {:reject, 13, "InvalidEntry"}
  defp reject(14), do: {:reject, 14, "DuplicateModule"}
  defp reject(15), do: {:reject, 15, "InvalidModuleName"}
  defp reject(16), do: {:reject, 16, "RequestTooLarge"}
  defp reject(17), do: {:reject, 17, "InvalidTier"}
  defp reject(18), do: {:reject, 18, "InstallRejected"}
  defp reject(19), do: {:reject, 19, "Truncated"}
end
