defmodule AgentOS.Luauc.Host do
  @moduledoc """
  Answers the `"compiler"` host call on a VM.

  Parsing stays on the VM process. Compilation runs under
  `AgentOS.SidecarTaskSupervisor` so a tick is not blocked for the port timeout.
  One compile runs at a time. The host stamps the wasm and returns the bytes.
  The guest writes the file.
  """

  alias AgentOS.Host.Nif
  alias AgentOS.Luauc.Metrics
  alias AgentOS.Luauc.Request
  alias AgentOS.Luauc.Wasm
  alias AgentOS.LuaucEngine

  @spec answer(map(), map()) :: {:answered, map()} | :unclaimed
  def answer(state, %{kind: :host_call, handle: handle, name: "compiler", body: body})
      when is_integer(handle) and is_binary(body) do
    if state.compiler_engine == nil do
      Metrics.inc_host_call(:detached)

      if map_size(state.git_engines || %{}) > 0 do
        _ = Nif.relay_host_call_fail(state.nif, handle)
        {:answered, state}
      else
        :unclaimed
      end
    else
      {:answered, enqueue_or_start(state, handle, body)}
    end
  end

  def answer(state, %{kind: :host_call_close, handle: handle} = event) when is_integer(handle) do
    name = Map.get(event, :name)
    owns? = name == "compiler" or queued?(state, handle) or Map.has_key?(state.luauc_tasks || %{}, handle)

    if owns? do
      {:answered, drop(state, handle)}
    else
      :unclaimed
    end
  end

  def answer(_state, _event), do: :unclaimed

  @spec finish(map(), reference(), integer(), term(), map()) :: map()
  def finish(state, ref, handle, result, stamp) do
    refs = Map.delete(state.luauc_task_refs || %{}, ref)
    tasks = state.luauc_tasks || %{}
    meta = Map.get(tasks, handle)
    state = %{state | luauc_task_refs: refs, luauc_tasks: Map.delete(tasks, handle)}
    state = if meta && meta.closed, do: state, else: respond(state, handle, result, stamp)
    start_next(state)
  end

  @spec task_down(map(), reference()) :: {:ok, map()} | :unknown
  def task_down(state, ref) when is_reference(ref) do
    case Map.pop(state.luauc_task_refs || %{}, ref) do
      {nil, _} ->
        :unknown

      {handle, refs} ->
        tasks = state.luauc_tasks || %{}
        meta = Map.get(tasks, handle)
        state = %{state | luauc_task_refs: refs, luauc_tasks: Map.delete(tasks, handle)}

        state =
          if meta && meta.closed do
            state
          else
            Metrics.inc_host_call(:rejected)
            _ = Nif.relay_host_call_fail(state.nif, handle)
            state
          end

        {:ok, start_next(state)}
    end
  end

  @spec engine_down(map()) :: map()
  def engine_down(state) do
    Enum.each(state.luauc_tasks || %{}, fn {handle, meta} ->
      unless meta.closed, do: Nif.relay_host_call_fail(state.nif, handle)
    end)

    Enum.each(state.luauc_queue || [], fn {handle, _} ->
      Nif.relay_host_call_fail(state.nif, handle)
    end)

    %{state | compiler_engine: nil, compiler_mon: nil, luauc_tasks: %{}, luauc_task_refs: %{}, luauc_queue: []}
  end

  @spec stop(map()) :: map()
  def stop(state) do
    if is_pid(state.compiler_engine) do
      LuaucEngine.stop(state.compiler_engine)
    end

    if is_reference(state.compiler_mon) do
      Process.demonitor(state.compiler_mon, [:flush])
    end

    %{state | compiler_engine: nil, compiler_mon: nil, luauc_tasks: %{}, luauc_task_refs: %{}, luauc_queue: []}
  end

  @spec attach(map(), keyword()) :: {:ok, map()} | {:error, term()}
  def attach(state, opts) when is_list(opts) do
    if is_pid(state.compiler_engine) do
      {:error, :compiler_already_attached}
    else
      case LuaucEngine.start(opts) do
        {:ok, pid} ->
          %{profile: profile, pack: pack} = LuaucEngine.digests(pid)

          {:ok,
           %{
             state
             | compiler_engine: pid,
               compiler_mon: Process.monitor(pid),
               compiler_profile_digest: profile,
               compiler_pack_digest: pack
           }}

        {:error, reason} ->
          {:error, reason}
      end
    end
  end

  defp enqueue_or_start(state, handle, body) do
    tasks = state.luauc_tasks || %{}
    queue = state.luauc_queue || []

    cond do
      Map.has_key?(tasks, handle) or queued?(state, handle) ->
        state

      map_size(tasks) > 0 ->
        %{state | luauc_queue: queue ++ [{handle, body}]}

      true ->
        start(state, handle, body)
    end
  end

  defp start(state, handle, body) do
    case Request.prepare(
           body,
           state.compiler_profile_digest,
           state.compiler_pack_digest,
           state.compiler_caller_mask,
           %{
             default_tier: state.compiler_default_tier,
             mem_ceiling: state.compiler_mem_ceiling,
             fuel_ceiling: state.compiler_fuel_ceiling
           }
         ) do
      {:reject, status, diagnostic} ->
        Metrics.inc_host_call(:ok)
        _ = Nif.relay_host_call_respond(state.nif, handle, Request.frame(status, diagnostic, <<>>))
        start_next(state)

      {:compile, package, stamp} ->
        engine = state.compiler_engine

        task =
          Task.Supervisor.async_nolink(AgentOS.SidecarTaskSupervisor, fn ->
            {:luauc, handle, LuaucEngine.compile(engine, package), stamp}
          end)

        %{
          state
          | luauc_tasks: Map.put(state.luauc_tasks || %{}, handle, %{task: task, closed: false}),
            luauc_task_refs: Map.put(state.luauc_task_refs || %{}, task.ref, handle)
        }
    end
  end

  defp start_next(%{luauc_queue: [{handle, body} | rest]} = state) do
    if state.compiler_engine == nil do
      Enum.each([{handle, body} | rest], fn {queued_handle, _} ->
        Nif.relay_host_call_fail(state.nif, queued_handle)
      end)

      %{state | luauc_queue: []}
    else
      start(%{state | luauc_queue: rest}, handle, body)
    end
  end

  defp start_next(state), do: state

  defp respond(state, handle, {:ok, status, diagnostic, guest}, stamp) do
    {status, diagnostic, artifact} =
      if status == 0 do
        stamped = Wasm.stamp(guest, stamp.tier, stamp.mem, stamp.fuel, stamp.table)
        {0, <<>>, stamped}
      else
        {status, diagnostic, <<>>}
      end

    Metrics.inc_host_call(:ok)
    _ = Nif.relay_host_call_respond(state.nif, handle, Request.frame(status, diagnostic, artifact))
    state
  end

  defp respond(state, handle, {:error, _reason}, _stamp) do
    Metrics.inc_host_call(:rejected)
    _ = Nif.relay_host_call_fail(state.nif, handle)
    state
  end

  defp drop(state, handle) do
    queue = Enum.reject(state.luauc_queue || [], fn {queued, _} -> queued == handle end)

    tasks =
      case Map.get(state.luauc_tasks || %{}, handle) do
        nil -> state.luauc_tasks || %{}
        meta -> Map.put(state.luauc_tasks || %{}, handle, %{meta | closed: true})
      end

    %{state | luauc_queue: queue, luauc_tasks: tasks}
  end

  defp queued?(state, handle) do
    Enum.any?(state.luauc_queue || [], fn {queued, _} -> queued == handle end)
  end
end
