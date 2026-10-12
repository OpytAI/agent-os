defmodule AgentOS.LuaucEngine do
  @moduledoc """
  One native `compiler_port` process.

  The request is a little-endian `u32` length and one SourcePackageV1 body.
  The response is `u32` status, `u32` diagnostic length, diagnostic bytes,
  `u32` artifact length, artifact bytes. Arguments are the profile path and the
  pack path. The process receives no guest path and no network address.
  """

  use GenServer

  require Logger

  alias AgentOS.Luauc.Metrics
  alias AgentOS.Luauc.Wasm

  @request_timeout 120_000
  @max_artifact 16 * 1024 * 1024
  @max_diagnostic 1024 * 1024

  @spec start(keyword()) :: GenServer.on_start()
  def start(opts \\ []), do: GenServer.start(__MODULE__, opts)

  @spec stop(pid()) :: :ok
  def stop(pid) when is_pid(pid) do
    GenServer.stop(pid, :normal, 5_000)
  catch
    :exit, _ -> :ok
  end

  @spec digests(pid()) :: %{profile: binary(), pack: binary()}
  def digests(pid) when is_pid(pid), do: GenServer.call(pid, :digests, 5_000)

  @spec compile(pid(), binary()) :: {:ok, non_neg_integer(), binary(), binary()} | {:error, term()}
  def compile(pid, package) when is_pid(pid) and is_binary(package) do
    GenServer.call(pid, {:compile, package}, @request_timeout)
  catch
    :exit, reason -> {:error, {:port_exit, reason}}
  end

  @impl true
  def init(opts) do
    executable = resolve_executable(opts)

    with {:ok, paths} <- stage_artifacts(opts) do
      case init_engine(executable, paths) do
        {:ok, _state} = started ->
          started

        {:stop, _reason} = stopped ->
          File.rm_rf(paths.temp_dir)
          stopped
      end
    else
      {:error, reason} -> {:stop, reason}
    end
  end

  defp init_engine(executable, paths) do
    with true <- File.regular?(executable) || {:error, {:luauc_engine_missing, executable}},
         {:ok, port} <- open_port(executable, paths.profile, paths.pack) do
      state = %{
        port: port,
        executable: executable,
        buffer: <<>>,
        temp_dir: paths.temp_dir,
        profile_digest: :crypto.hash(:sha256, paths.profile_bytes),
        pack_digest: :crypto.hash(:sha256, paths.pack_bytes)
      }

      case await_startup(port) do
        :ok ->
          Logger.info(
            "luauc compiler_attached profile_sha256=#{hex(state.profile_digest)} pack_sha256=#{hex(state.pack_digest)}"
          )

          {:ok, state}

        {:error, status} ->
          Logger.warning("luauc engine exited executable=#{executable} status=#{inspect(status)}")
          Metrics.inc_restart()
          {:stop, {:luauc_context_failed, status}}
      end
    else
      {:error, reason} -> {:stop, reason}
    end
  end

  @impl true
  def handle_call(:digests, _from, state) do
    {:reply, %{profile: state.profile_digest, pack: state.pack_digest}, state}
  end

  def handle_call({:compile, package}, _from, %{port: nil} = state) do
    {:reply, {:error, :eio}, state}
  end

  def handle_call({:compile, package}, _from, state) do
    started = System.monotonic_time(:microsecond)
    Port.command(state.port, <<byte_size(package)::little-unsigned-32, package::binary>>)

    case read_response(state.port, state.buffer, System.monotonic_time(:millisecond) + @request_timeout) do
      {:ok, status, diagnostic, artifact, rest} ->
        artifact = if status == 0, do: artifact, else: <<>>
        duration_us = max(System.monotonic_time(:microsecond) - started, 0)
        Metrics.inc_compile(status)
        Metrics.add_duration_us(:port, duration_us)
        Metrics.add_guest_bytes(byte_size(artifact))

        Metrics.log_compile(
          :port,
          request_id(package),
          status,
          div(duration_us, 1000),
          byte_size(package),
          byte_size(artifact),
          byte_size(diagnostic),
          module_count(package)
        )

        {:reply, {:ok, status, diagnostic, artifact}, %{state | buffer: rest}}

      {:error, {:port_exit, status}, rest} ->
        stop_port(state, {:port_exit, status}, rest)

      {:error, reason, rest} ->
        stop_port(state, reason, rest)
    end
  end

  @impl true
  def handle_info({port, {:exit_status, status}}, %{port: port} = state) do
    Logger.warning("luauc engine exited executable=#{state.executable} status=#{inspect(status)}")
    Metrics.inc_restart()
    {:stop, {:port_exit, status}, %{state | port: nil}}
  end

  def handle_info(_message, state), do: {:noreply, state}

  @impl true
  def terminate(_reason, state) do
    if is_port(state.port), do: Port.close(state.port)
    if is_binary(state.temp_dir), do: File.rm_rf(state.temp_dir)
    :ok
  end

  defp stop_port(state, reason, rest) do
    Logger.warning("luauc engine exited executable=#{state.executable} reason=#{inspect(reason)}")
    Metrics.inc_restart()
    if is_port(state.port), do: Port.close(state.port)
    {:stop, reason, {:error, reason}, %{state | port: nil, buffer: rest}}
  end

  defp await_startup(port) do
    receive do
      {^port, {:exit_status, status}} -> {:error, status}
    after
      2_000 -> :ok
    end
  end

  defp read_response(port, buffer, deadline) do
    case take_response(buffer) do
      {:ok, status, diagnostic, artifact, rest} ->
        {:ok, status, diagnostic, artifact, rest}

      :need_more ->
        remaining = deadline - System.monotonic_time(:millisecond)

        if remaining <= 0 do
          {:error, :timeout, buffer}
        else
          receive do
            {^port, {:data, data}} ->
              read_response(port, buffer <> data, deadline)

            {^port, {:exit_status, status}} ->
              {:error, {:port_exit, status}, buffer}
          after
            remaining -> {:error, :timeout, buffer}
          end
        end

      {:error, reason} ->
        {:error, reason, buffer}
    end
  end

  defp take_response(<<status::little-unsigned-32, diag_len::little-unsigned-32, rest::binary>>)
       when diag_len > @max_diagnostic, do: {:error, :diagnostic_too_large}

  defp take_response(<<status::little-unsigned-32, diag_len::little-unsigned-32, rest::binary>>)
       when byte_size(rest) >= diag_len do
    <<diagnostic::binary-size(diag_len), rest::binary>> = rest

    case rest do
      <<art_len::little-unsigned-32, _::binary>> when art_len > @max_artifact ->
        {:error, :artifact_too_large}

      <<art_len::little-unsigned-32, rest::binary>> when byte_size(rest) >= art_len ->
        <<artifact::binary-size(art_len), tail::binary>> = rest
        {:ok, status, diagnostic, artifact, tail}

      _ ->
        :need_more
    end
  end

  defp take_response(_), do: :need_more

  defp open_port(executable, profile, pack) do
    {:ok,
     Port.open({:spawn_executable, String.to_charlist(executable)}, [
       :binary,
       :exit_status,
       args: [String.to_charlist(profile), String.to_charlist(pack)]
     ])}
  rescue
    error -> {:error, {:luauc_engine_open_failed, error}}
  end

  defp stage_artifacts(opts) do
    temp = Path.join(System.tmp_dir!(), "agentos-luauc-#{System.unique_integer([:positive])}")
    File.mkdir_p!(temp)

    with {:ok, pack_path, pack_bytes} <- place(temp, "pack", opts, :pack, :pack_path, "luauc-runtime.pack.wasm"),
         {:ok, profile_path, profile_bytes} <- place_profile(temp, opts, pack_path, pack_bytes) do
      {:ok,
       %{
         temp_dir: temp,
         pack: pack_path,
         profile: profile_path,
         pack_bytes: pack_bytes,
         profile_bytes: profile_bytes
       }}
    else
      {:error, reason} ->
        File.rm_rf(temp)
        {:error, reason}
    end
  end

  defp place_profile(temp, opts, pack_path, pack_bytes) do
    cond do
      is_binary(Keyword.get(opts, :profile)) or is_binary(Keyword.get(opts, :profile_path)) ->
        place(temp, "profile", opts, :profile, :profile_path, "luauc-runtime.profile")

      true ->
        path = resolve_artifact("luauc-runtime.profile", ["AGENTOS_LUAUC_PROFILE", "MC_LUAUC_PROFILE"])

        cond do
          File.regular?(path) ->
            {:ok, path, File.read!(path)}

          true ->
            case Wasm.profile_from_pack(pack_bytes) do
              {:ok, payload} ->
                out = Path.join(temp, "profile")
                File.write!(out, payload)
                {:ok, out, payload}

              :error ->
                _ = pack_path
                {:error, :luauc_profile_missing}
            end
        end
    end
  end

  defp place(temp, leaf, opts, bytes_key, path_key, install_name) do
    cond do
      is_binary(Keyword.get(opts, bytes_key)) ->
        bytes = Keyword.get(opts, bytes_key)
        path = Path.join(temp, leaf)
        File.write!(path, bytes)
        {:ok, path, bytes}

      is_binary(Keyword.get(opts, path_key)) and File.regular?(Keyword.get(opts, path_key)) ->
        path = Keyword.get(opts, path_key)
        {:ok, path, File.read!(path)}

      true ->
        path = resolve_artifact(install_name, env_names(install_name))
        if File.regular?(path), do: {:ok, path, File.read!(path)}, else: {:error, {:luauc_artifact_missing, path}}
    end
  end

  defp env_names("luauc-runtime.pack.wasm"), do: ["AGENTOS_LUAUC_PACK", "MC_LUAUC_PACK"]
  defp env_names("luauc-runtime.profile"), do: ["AGENTOS_LUAUC_PROFILE", "MC_LUAUC_PROFILE"]
  defp env_names(_), do: []

  defp resolve_executable(opts) do
    Keyword.get(opts, :executable) ||
      System.get_env("AGENTOS_LUAUC_ENGINE") ||
      System.get_env("MC_LUAUC_ENGINE") ||
      install_file("luauc") ||
      priv_file("luauc") ||
      Path.expand("../memcontainers/programs/luauc/engine/luauc", File.cwd!())
  end

  defp resolve_artifact(name, env_names) do
    Enum.find_value(env_names, fn key -> System.get_env(key) end) ||
      install_file(name) ||
      priv_file(name) ||
      Path.expand("../memcontainers/programs/luauc/engine/#{name}", File.cwd!())
  end

  defp install_file(name) do
    root = System.get_env("AGENTOS_DIR") || System.get_env("MC_ARTIFACT_HOME")
    if is_binary(root), do: Path.join(root, name), else: nil
  end

  defp priv_file(name) do
    case :code.priv_dir(:agent_os) do
      path when is_list(path) ->
        candidate = Path.join(List.to_string(path), name)
        if File.regular?(candidate), do: candidate, else: nil

      _ ->
        nil
    end
  end

  defp request_id(<<_::binary-size(32), id::binary-size(16), _::binary>>), do: hex(id)
  defp request_id(_), do: "-"

  defp module_count(<<_::binary-size(16), count::little-unsigned-32, _::binary>>), do: count
  defp module_count(_), do: 0

  defp hex(bytes), do: Base.encode16(bytes, case: :lower)
end
