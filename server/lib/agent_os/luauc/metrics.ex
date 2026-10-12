defmodule AgentOS.Luauc.Metrics do
  @moduledoc """
  In-process counters for the optional luauc engine.

  Not Prometheus. `snapshot/0` and `reset/0` are for tests. Counters never store
  source text.
  """

  require Logger

  @table :agent_os_luauc_metrics

  @spec ensure_table() :: :ok
  def ensure_table do
    case :ets.whereis(@table) do
      :undefined ->
        :ets.new(@table, [
          :named_table,
          :public,
          :set,
          read_concurrency: true,
          write_concurrency: true
        ])

        :ok

      _tid ->
        :ok
    end
  rescue
    ArgumentError -> :ok
  end

  @spec inc_compile(non_neg_integer()) :: :ok
  def inc_compile(status) when is_integer(status) and status >= 0 do
    bump({:compile_total, status})
  end

  @spec add_duration_us(atom(), non_neg_integer()) :: :ok
  def add_duration_us(host, microseconds)
      when host in [:wasm, :port] and is_integer(microseconds) and microseconds >= 0 do
    bump({:duration_us, host}, microseconds)
  end

  @spec add_guest_bytes(non_neg_integer()) :: :ok
  def add_guest_bytes(bytes) when is_integer(bytes) and bytes > 0, do: bump({:guest_bytes, :sum}, bytes)
  def add_guest_bytes(0), do: :ok

  @spec inc_restart() :: :ok
  def inc_restart, do: bump({:engine_restart, :total})

  @spec inc_host_call(atom()) :: :ok
  def inc_host_call(result) when result in [:ok, :detached, :rejected] do
    bump({:host_call, result})
  end

  @spec snapshot() :: map()
  def snapshot do
    ensure_table()

    :ets.tab2list(@table)
    |> Map.new()
  end

  @spec reset() :: :ok
  def reset do
    ensure_table()
    :ets.delete_all_objects(@table)
    :ok
  end

  @spec log_compile(atom(), binary(), non_neg_integer(), non_neg_integer(), non_neg_integer(), non_neg_integer(), non_neg_integer(), non_neg_integer()) ::
          :ok
  def log_compile(host, request_id, status, duration_ms, request_bytes, guest_bytes, diagnostic_bytes, module_count) do
    Logger.info(
      "luauc compile host=#{host} request_id=#{request_id} status=#{status} duration_ms=#{duration_ms} request_bytes=#{request_bytes} guest_bytes=#{guest_bytes} diagnostic_bytes=#{diagnostic_bytes} module_count=#{module_count}"
    )
  end

  defp bump(key, n \\ 1) do
    ensure_table()
    :ets.update_counter(@table, key, {2, n}, {key, 0})
    :ok
  end
end
